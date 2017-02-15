/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#include <config.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <util.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <cairo-xlib.h>

#include "../core/screen-private.h"
#include "../core/workspace.h"
#include "deepin-stated-image.h"
#include "boxes.h"
#include "deepin-cloned-widget.h"
#include "compositor.h"
#include "deepin-design.h"
#include "deepin-ease.h"
#include "deepin-workspace-overview.h"
#include "deepin-window-surface-manager.h"
#include "deepin-background-cache.h"
#include "deepin-message-hub.h"

#define SET_STATE(w, state)  \
    gtk_style_context_set_state(gtk_widget_get_style_context(GTK_WIDGET(w)), (state))

/* TODO: handle live window add/remove events */

typedef struct _MonitorData
{
    gint monitor;
    MetaRectangle mon_rect;
    MetaRectangle mon_workarea;
    MetaRectangle place_rect;
    GPtrArray* clones;
    cairo_surface_t* desktop_surface;
} MonitorData;

MonitorData* monitor_data_new()
{
    MonitorData* md = g_slice_new0(MonitorData);

    return md;
}

static void monitor_data_destroy(MonitorData* md)
{
    g_slice_free(MonitorData, md);
}

struct _DeepinWorkspaceOverviewPrivate
{
    gint disposed: 1;
    gint ready: 1; /* if dynamic, this is set after presentation finished, 
                      else, set when window placements are done */
    gint all_window_mode: 1; // used for Super+a

    GHashTable* present_xids; // used if user request presenting specific windows

    gint fixed_width, fixed_height;

    gint primary;
    GPtrArray* monitors;
    
    MetaDeepinClonedWidget* hovered_clone;
    MetaWorkspace* workspace;

    GdkWindow* event_window;

    GtkWidget* close_button; /* for focused clone */
    MetaDeepinClonedWidget* window_need_focused;

    int dock_height;
};

typedef struct _ClonedPrivateInfo
{
    gint monitor; // remember in which monitor it resides
} ClonedPrivateInfo;

static GQuark _cloned_widget_key_quark = 0;

static void clone_set_info(MetaDeepinClonedWidget* w, gpointer data)
{
    if (!_cloned_widget_key_quark) {
        _cloned_widget_key_quark = g_quark_from_static_string("cloned-widget-key");
    }
    g_object_set_qdata_full(G_OBJECT(w), _cloned_widget_key_quark, data, g_free);
}

static ClonedPrivateInfo* clone_get_info(MetaDeepinClonedWidget* w)
{
    if (!_cloned_widget_key_quark) {
        _cloned_widget_key_quark = g_quark_from_static_string("cloned-widget-key");
    }
    ClonedPrivateInfo* info;
    info = (ClonedPrivateInfo*)g_object_get_qdata(G_OBJECT(w), _cloned_widget_key_quark);
    if (!info) {
        info = (ClonedPrivateInfo*)g_malloc(sizeof(ClonedPrivateInfo));
        clone_set_info(w, info);
    }
    return info;
}

static void _hide_close_button(DeepinWorkspaceOverview* self)
{
    if (self->priv->close_button) {
        gtk_widget_set_opacity(self->priv->close_button, 0.0);
        deepin_fixed_move(DEEPIN_FIXED(self), self->priv->close_button,
                -100, -100,
                FALSE);
    }
}

static void _move_close_button_for(DeepinWorkspaceOverview* self,
        MetaDeepinClonedWidget* cloned)
{
    GtkAllocation alloc;
    gtk_widget_get_allocation(GTK_WIDGET(cloned), &alloc);

    gint x = 0, y = 0;
    gtk_container_child_get(GTK_CONTAINER(self), GTK_WIDGET(cloned),
            "x", &x, "y", &y, NULL);

    gdouble sx = 1.0;
    meta_deepin_cloned_widget_get_scale(cloned, &sx, NULL);

    deepin_fixed_move(DEEPIN_FIXED(self), self->priv->close_button,
            x + alloc.width * sx /2,
            y - alloc.height * sx /2,
            FALSE);
}

G_DEFINE_TYPE (DeepinWorkspaceOverview, deepin_workspace_overview, DEEPIN_TYPE_FIXED);

static void place_window(DeepinWorkspaceOverview* self,
        MetaDeepinClonedWidget* clone, MetaRectangle rect)
{
    GtkRequisition req;
    gtk_widget_get_preferred_size(GTK_WIDGET(clone), &req, NULL);

    float fscale = (float)rect.width / req.width;
    req.width *= fscale;
    req.height *= fscale;

    deepin_fixed_move(DEEPIN_FIXED(self), GTK_WIDGET(clone),
            rect.x + req.width/2, rect.y + req.height/2,
            FALSE);

    meta_deepin_cloned_widget_set_size(clone, req.width, req.height);
}

static const int GAPS = 10;
static const int MAX_TRANSLATIONS = 100000;
static const int ACCURACY = 20;

//some math utilities
static gboolean rect_is_overlapping_any(MetaRectangle rect, MetaRectangle* rects, gint n, MetaRectangle border)
{
    if (!meta_rectangle_contains_rect(&border, &rect))
        return TRUE;

    for (int i = 0; i < n; i++) {
        if (meta_rectangle_equal(&rects[i], &rect))
            continue;

        if (meta_rectangle_overlap(&rects[i], &rect))
            return TRUE;
    }

    return FALSE;
}

static MetaRectangle rect_adjusted(MetaRectangle rect, int dx1, int dy1, int dx2, int dy2)
{
    return (MetaRectangle){rect.x + dx1, rect.y + dy1, rect.width + (-dx1 + dx2), rect.height + (-dy1 + dy2)};
}

static GdkPoint rect_center(MetaRectangle rect)
{
    return (GdkPoint){rect.x + rect.width / 2, rect.y + rect.height / 2};
}

static int squared_distance (GdkPoint a, GdkPoint b)
{
    int k1 = b.x - a.x;
    int k2 = b.y - a.y;

    return k1*k1 + k2*k2;
}

typedef struct _TilableWindow
{
    MetaRectangle rect;
    MetaDeepinClonedWidget* id;
} TilableWindow;

static void grid_placement ( DeepinWorkspaceOverview* self, 
        MonitorData* md, MetaRectangle area, gboolean closest)
{
    DeepinWorkspaceOverviewPrivate* priv = self->priv;
    GPtrArray* clones = md->clones;
    if (!clones || clones->len == 0) return;

    int window_count = clones->len;
    int columns = (int)ceil (sqrt (window_count));
    int rows = (int)ceil (window_count / (double)columns);

    // Assign slots
    int slot_width = area.width / columns;
    int slot_height = area.height / rows;

    GList* windows = NULL;
    for (int i = 0; i < clones->len; i++) {
        MetaDeepinClonedWidget* clone = g_ptr_array_index(clones, i);

        TilableWindow* tw = g_new0(TilableWindow, 1);
        tw->id = clone;

        MetaWindow* win = meta_deepin_cloned_widget_get_window(clone);
        meta_window_get_input_rect(win, &tw->rect);

        windows = g_list_append(windows, tw);
    }

    TilableWindow* taken_slots[rows * columns];
    memset(taken_slots, 0, sizeof taken_slots);

    if (closest) {
        // Assign each window to the closest available slot.

        // precalculate all slot centers
        GdkPoint slot_centers[rows * columns];
        memset(slot_centers, 0, sizeof slot_centers);

        for (int x = 0; x < columns; x++) {
            for (int y = 0; y < rows; y++) {
                slot_centers[x + y * columns] = (GdkPoint){
                    area.x + slot_width  * x + slot_width  / 2,
                    area.y + slot_height * y + slot_height / 2
                };
            }
        }

        GList* tmplist = g_list_copy(windows);
        while (g_list_length(tmplist) > 0) {
            GList* link = g_list_nth(tmplist, 0);
            TilableWindow* window = (TilableWindow*)link->data;
            MetaRectangle rect = window->rect;

            int slot_candidate = -1;
            int slot_candidate_distance = INT_MAX;
            GdkPoint pos = rect_center (rect);

            // all slots
            for (int i = 0; i < columns * rows; i++) {
                if (i > window_count - 1)
                    break;

                int dist = squared_distance (pos, slot_centers[i]);

                if (dist < slot_candidate_distance) {
                    // window is interested in this slot
                    TilableWindow* occupier = taken_slots[i];
                    if (occupier == window)
                        continue;

                    if (occupier == NULL || dist < squared_distance (rect_center (occupier->rect), slot_centers[i])) {
                        // either nobody lives here, or we're better - takeover the slot if it's our best
                        slot_candidate = i;
                        slot_candidate_distance = dist;
                    }
                }
            }

            if (slot_candidate == -1)
                continue;

            if (taken_slots[slot_candidate] != NULL)
                tmplist = g_list_prepend(tmplist, taken_slots[slot_candidate]);

            tmplist = g_list_remove_link(tmplist, link);
            taken_slots[slot_candidate] = window;
            g_list_free(link);
        }
        g_list_free(tmplist);

    } else {
        // Assign each window as the origin order.
        for (int i = 0; i < clones->len; i++) {
            GList* link = g_list_nth (windows, i);
            taken_slots[i] = (TilableWindow*)link->data;
        }
    }

    // see how many windows we have on the last row
    int left_over = (int)window_count - columns * (rows - 1);

    for (int slot = 0; slot < columns * rows; slot++) {
        TilableWindow* window = taken_slots[slot];
        // some slots might be empty
        if (window == NULL)
            continue;

        MetaRectangle rect = window->rect;

        // Work out where the slot is
        MetaRectangle target = {
            area.x + (slot % columns) * slot_width,
            area.y + (slot / columns) * slot_height,
            slot_width, 
            slot_height
        };
        target = rect_adjusted (target, 10, 10, -10, -10);

        float scale;
        if (target.width / (double)rect.width < target.height / (double)rect.height) {
            // Center vertically
            scale = target.width / (float)rect.width;
            target.y += (target.height - (int)(rect.height * scale)) / 2;
            target.height = (int)floorf (rect.height * scale);
        } else {
            // Center horizontally
            scale = target.height / (float)rect.height;
            target.x += (target.width - (int)(rect.width * scale)) / 2;
            target.width = (int)floorf (rect.width * scale);
        }

        // Don't scale the windows too much
        if (scale > 1.0) {
            scale = 1.0f;
            target = (MetaRectangle){
                rect_center (target).x - (int)floorf (rect.width * scale) / 2,
                rect_center (target).y - (int)floorf (rect.height * scale) / 2,
                (int)floorf (scale * rect.width), 
                (int)floorf (scale * rect.height)
            };
        }

        // put the last row in the center, if necessary
        if (left_over != columns && slot >= columns * (rows - 1))
            target.x += (columns - left_over) * slot_width / 2;

        place_window(self, window->id, target);
    }

    g_list_free_full(windows, g_free);
}

static int padding_top  = 12;
static int padding_left  = 12;
static int padding_right  = 12;
static int padding_bottom  = 12;
static void calculate_places(DeepinWorkspaceOverview* self)
{
    DeepinWorkspaceOverviewPrivate* priv = self->priv;

    for (int i = 0; i < priv->monitors->len; i++) {
        MonitorData* md = (MonitorData*)g_ptr_array_index(priv->monitors, i);

        if (md->clones && md->clones->len) {
            MetaRectangle area = {
                md->mon_workarea.x + padding_top,
                md->mon_workarea.y + padding_left, 
                md->mon_workarea.width - padding_left - padding_right,
                md->mon_workarea.height - padding_top - padding_bottom
            };

            md->place_rect = area;

            grid_placement(self, md, area, FALSE);
        }
    }
    priv->ready = TRUE;
}

static gboolean on_idle(DeepinWorkspaceOverview* self)
{
    DeepinWorkspaceOverviewPrivate* priv = self->priv;
    if (priv->disposed) return G_SOURCE_REMOVE;

    if (priv->close_button) {
        deepin_fixed_raise(DEEPIN_FIXED(self), priv->close_button);
        _hide_close_button(self);
    }
    calculate_places(self);
    return G_SOURCE_REMOVE;
}

static void on_window_removed(DeepinMessageHub* hub, MetaWindow* window, 
        gpointer data)
{
    DeepinWorkspaceOverview* self = DEEPIN_WORKSPACE_OVERVIEW(data);
    DeepinWorkspaceOverviewPrivate* priv = self->priv;

    for (int i = 0; i < priv->monitors->len; i++) {
        MonitorData* md = (MonitorData*)g_ptr_array_index(priv->monitors, i);
        if (!md->clones) continue;

        for (gint i = 0; i < md->clones->len; i++) {
            MetaDeepinClonedWidget* clone = g_ptr_array_index(md->clones, i);
            if (meta_deepin_cloned_widget_get_window(clone) == window) {
                g_ptr_array_remove(md->clones, clone);
                gtk_container_remove(GTK_CONTAINER(self), (GtkWidget*)clone);

                meta_verbose("%s remove clone for %s\n", __func__, window->desc);

                if (priv->hovered_clone == clone) {
                    priv->hovered_clone = NULL;
                    _hide_close_button(self);
                }

                if (priv->ready) g_idle_add((GSourceFunc)on_idle, self);

                return;
            }
        }
    }
}

static void deepin_workspace_overview_init (DeepinWorkspaceOverview *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_WORKSPACE_OVERVIEW, DeepinWorkspaceOverviewPrivate);
    memset(self->priv, 0, sizeof *self->priv);

    gtk_widget_set_sensitive(GTK_WIDGET(self), TRUE);
    gtk_widget_set_app_paintable(GTK_WIDGET(self), TRUE);
    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);
}

static void deepin_workspace_overview_finalize (GObject *object)
{
    DeepinWorkspaceOverview* self = DEEPIN_WORKSPACE_OVERVIEW(object);
    DeepinWorkspaceOverviewPrivate* priv = self->priv;
    if (priv->disposed) return;

    priv->disposed = TRUE;

    g_signal_handlers_disconnect_by_data(G_OBJECT(deepin_message_hub_get()), 
            self);

    for (int i = 0; i < priv->monitors->len; i++) {
        MonitorData* md = g_ptr_array_index(priv->monitors, i);

        g_ptr_array_free(md->clones, FALSE);
        g_clear_pointer(&md->desktop_surface, cairo_surface_destroy);
    }
    g_ptr_array_unref(priv->monitors);

    if (priv->present_xids) g_hash_table_destroy(priv->present_xids);

    G_OBJECT_CLASS (deepin_workspace_overview_parent_class)->finalize (object);
}

static void _style_get_borders (GtkStyleContext *context, GtkBorder *border_out)
{
    GtkBorder padding, border;
    GtkStateFlags state;

    state = gtk_style_context_get_state (context);
    gtk_style_context_get_padding (context, state, &padding);
    gtk_style_context_get_border (context, state, &border);

    border_out->top = padding.top + border.top;
    border_out->bottom = padding.bottom + border.bottom;
    border_out->left = padding.left + border.left;
    border_out->right = padding.right + border.right;
}

static void deepin_workspace_overview_get_preferred_width (GtkWidget *widget,
        gint *minimum, gint *natural)
{
    DeepinWorkspaceOverview *self = DEEPIN_WORKSPACE_OVERVIEW (widget);

    *minimum = *natural = self->priv->fixed_width;
}

static void deepin_workspace_overview_get_preferred_height (GtkWidget *widget,
        gint *minimum, gint *natural)
{
    DeepinWorkspaceOverview *self = DEEPIN_WORKSPACE_OVERVIEW (widget);

    *minimum = *natural = self->priv->fixed_height;
}

static void _draw_round_box(cairo_t* cr, gint width, gint height, double radius)
{
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    double xc = radius, yc = radius;
    double angle1 = 180.0  * (M_PI/180.0);  /* angles are specified */
    double angle2 = 270.0 * (M_PI/180.0);  /* in radians           */

    cairo_arc (cr, xc, yc, radius, angle1, angle2);

    xc = width - radius;
    angle1 = 270.0 * (M_PI/180.0);
    angle2 = 360.0 * (M_PI/180.0);
    cairo_arc (cr, xc, yc, radius, angle1, angle2);

    yc = height - radius;
    angle1 = 0.0 * (M_PI/180.0);
    angle2 = 90.0 * (M_PI/180.0);
    cairo_arc (cr, xc, yc, radius, angle1, angle2);

    xc = radius;
    angle1 = 90.0 * (M_PI/180.0);
    angle2 = 180.0 * (M_PI/180.0);
    cairo_arc (cr, xc, yc, radius, angle1, angle2);

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_close_path(cr);
}

static gboolean deepin_workspace_overview_draw (GtkWidget *widget,
        cairo_t *cr)
{
    DeepinWorkspaceOverview *fixed = DEEPIN_WORKSPACE_OVERVIEW (widget);
    DeepinWorkspaceOverviewPrivate *priv = fixed->priv;

    GdkRectangle r;
    gdk_cairo_get_clip_rectangle(cr, &r);

    GtkAllocation req;
    gtk_widget_get_allocation(widget, &req);

    GtkStyleContext* context = gtk_widget_get_style_context(widget);

    cairo_save(cr);

    gtk_render_background(context, cr, 0, 0, req.width, req.height);

    for (int i = 0; i < priv->monitors->len; i++) {
        MonitorData* md = (MonitorData*)g_ptr_array_index(priv->monitors, i);
        cairo_set_source_surface(cr, md->desktop_surface, md->mon_rect.x, md->mon_rect.y);
        cairo_paint(cr);
    }

    cairo_restore(cr);
    
    GTK_WIDGET_CLASS(deepin_workspace_overview_parent_class)->draw(widget, cr);

    return TRUE;
}

static void union_with_clip (GtkWidget *widget,
                 gpointer   clip)
{
    GtkAllocation widget_clip;

    if (!gtk_widget_is_visible (widget) ||
            !gtk_widget_get_child_visible (widget))
        return;

    gtk_widget_get_clip (widget, &widget_clip);

    gdk_rectangle_union (&widget_clip, clip, clip);
}

static void _gtk_widget_set_simple_clip (GtkWidget     *widget,
                             GtkAllocation *content_clip)
{
    GtkStyleContext *context;
    GtkAllocation clip, allocation;
    GtkBorder extents;

    context = gtk_widget_get_style_context (widget);
    _style_get_borders(context, &extents);

    gtk_widget_get_allocation (widget, &allocation);

    clip = allocation;
    clip.x -= extents.left;
    clip.y -= extents.top;
    clip.width += extents.left + extents.right;
    clip.height += extents.top + extents.bottom;

    /*gtk_container_forall (GTK_CONTAINER (widget), union_with_clip, &clip);*/

    // HACK: leave space for shadow 
    clip.x -= 10;
    clip.y -= 10;
    clip.width += 20;
    clip.height += 20;

    gtk_widget_set_clip (widget, &clip);
}

static void deepin_workspace_overview_size_allocate(GtkWidget* widget, 
        GtkAllocation* allocation)
{
    GTK_WIDGET_CLASS(deepin_workspace_overview_parent_class)->size_allocate(
            widget, allocation);

    DeepinWorkspaceOverview *self = DEEPIN_WORKSPACE_OVERVIEW (widget);
    if (gtk_widget_get_realized (widget))
        gdk_window_move_resize (self->priv->event_window,
                allocation->x,
                allocation->y,
                allocation->width,
                allocation->height);

    _gtk_widget_set_simple_clip(widget, NULL);
}

static void deepin_workspace_overview_realize (GtkWidget *widget)
{
    DeepinWorkspaceOverview *self = DEEPIN_WORKSPACE_OVERVIEW (widget);
    DeepinWorkspaceOverviewPrivate *priv = self->priv;
    GtkAllocation allocation;
    GdkWindow *window;
    GdkWindowAttr attributes;
    gint attributes_mask;

    gtk_widget_get_allocation (widget, &allocation);

    gtk_widget_set_realized (widget, TRUE);

    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = allocation.x;
    attributes.y = allocation.y;
    attributes.width = allocation.width;
    attributes.height = allocation.height;
    attributes.wclass = GDK_INPUT_ONLY;
    attributes.event_mask = gtk_widget_get_events (widget);
    attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
            GDK_BUTTON_RELEASE_MASK |
            GDK_ENTER_NOTIFY_MASK |
            GDK_EXPOSURE_MASK |
            GDK_LEAVE_NOTIFY_MASK);

    attributes_mask = GDK_WA_X | GDK_WA_Y;

    window = gtk_widget_get_parent_window (widget);
    gtk_widget_set_window (widget, window);
    g_object_ref (window);

    priv->event_window = gdk_window_new (window,
            &attributes, attributes_mask);
    gtk_widget_register_window (widget, priv->event_window);
    gdk_window_lower(priv->event_window);
}

static void deepin_workspace_overview_unrealize (GtkWidget *widget)
{
    DeepinWorkspaceOverview *self = DEEPIN_WORKSPACE_OVERVIEW (widget);
    DeepinWorkspaceOverviewPrivate *priv = self->priv;

    if (priv->event_window) {
        gtk_widget_unregister_window (widget, priv->event_window);
        gdk_window_destroy (priv->event_window);
        priv->event_window = NULL;
    }

    GTK_WIDGET_CLASS (deepin_workspace_overview_parent_class)->unrealize (widget);
}

static void deepin_workspace_overview_map (GtkWidget *widget)
{
    DeepinWorkspaceOverview *self = DEEPIN_WORKSPACE_OVERVIEW (widget);
    DeepinWorkspaceOverviewPrivate *priv = self->priv;

    GTK_WIDGET_CLASS (deepin_workspace_overview_parent_class)->map (widget);

    if (priv->event_window)
        gdk_window_show_unraised(priv->event_window);
}

static void deepin_workspace_overview_unmap (GtkWidget *widget)
{
    DeepinWorkspaceOverview *self = DEEPIN_WORKSPACE_OVERVIEW (widget);
    DeepinWorkspaceOverviewPrivate *priv = self->priv;

    if (priv->event_window) {
        gdk_window_hide (priv->event_window);
    }

    GTK_WIDGET_CLASS (deepin_workspace_overview_parent_class)->unmap (widget);
}

static void deepin_workspace_overview_class_init (DeepinWorkspaceOverviewClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass* widget_class = (GtkWidgetClass*) klass;

    g_type_class_add_private (klass, sizeof (DeepinWorkspaceOverviewPrivate));
    widget_class->get_preferred_width = deepin_workspace_overview_get_preferred_width;
    widget_class->get_preferred_height = deepin_workspace_overview_get_preferred_height;
    widget_class->size_allocate = deepin_workspace_overview_size_allocate;
    widget_class->draw = deepin_workspace_overview_draw;

    widget_class->realize = deepin_workspace_overview_realize;
    widget_class->unrealize = deepin_workspace_overview_unrealize;
    widget_class->map = deepin_workspace_overview_map;
    widget_class->unmap = deepin_workspace_overview_unmap;

    object_class->finalize = deepin_workspace_overview_finalize;
}

// propagate from cloned
static gboolean on_deepin_cloned_widget_leaved(MetaDeepinClonedWidget* cloned,
               GdkEvent* event, gpointer data)
{
    DeepinWorkspaceOverview* self = (DeepinWorkspaceOverview*)data;
    if (!self->priv->ready) return FALSE;

    meta_verbose ("%s\n", __func__);

    /* FIXME: there is a problem: when cloned is gets focused (so scaled up),
     * leave event will looks like as if it happened inside of cloned. need
     * a workaround */
    gint x, y;
    x = event->crossing.x_root;
    y = event->crossing.y_root;

    GtkAllocation alloc;
    gtk_widget_get_allocation(GTK_WIDGET(cloned), &alloc);

    GdkRectangle r = {alloc.x, alloc.y, alloc.width, alloc.height};
    if (x > r.x && x < r.x + r.width && y > r.y && y < r.y + r.height) {
        return FALSE;
    }

    self->priv->hovered_clone = NULL;
    _hide_close_button(self);
    return TRUE;
}

// propagate from cloned
static gboolean on_deepin_cloned_widget_entered(MetaDeepinClonedWidget* cloned,
               GdkEvent* event, gpointer data)
{
    DeepinWorkspaceOverview* self = (DeepinWorkspaceOverview*)data;
    DeepinWorkspaceOverviewPrivate* priv = self->priv;

    if (!priv->ready) return FALSE;
    meta_verbose ("%s\n", __func__);

    priv->hovered_clone = cloned;
    if (priv->ready) {
        _move_close_button_for(self, cloned);
        gtk_widget_set_opacity(priv->close_button, 1.0);
    }
    return TRUE;
}

static gboolean on_idle_end_grab(guint timestamp)
{
    meta_verbose ("%s\n", __func__);
    meta_display_end_grab_op(meta_get_display(), timestamp);
    return G_SOURCE_REMOVE;
}

static gboolean on_deepin_cloned_widget_released(MetaDeepinClonedWidget* cloned,
               GdkEvent* event, gpointer data)
{
    DeepinWorkspaceOverview* self = (DeepinWorkspaceOverview*)data;
    meta_verbose("%s\n", __func__);
    if (!self->priv->ready) return FALSE;

    MetaWindow* mw = meta_deepin_cloned_widget_get_window(cloned);
    if (mw->workspace && mw->workspace != mw->screen->active_workspace) {
        meta_workspace_activate(mw->workspace, gdk_event_get_time(event));
    }
    meta_window_activate(mw, gdk_event_get_time(event));
    g_idle_add((GSourceFunc)on_idle_end_grab, gdk_event_get_time(event));
    return TRUE;
}

static gboolean on_deepin_cloned_widget_motion(MetaDeepinClonedWidget* cloned,
               GdkEvent* event, gpointer data)
{
    DeepinWorkspaceOverview* self = (DeepinWorkspaceOverview*)data;
    DeepinWorkspaceOverviewPrivate* priv = self->priv;
    if (!priv->ready) return FALSE;

    if (priv->hovered_clone == NULL) {
        meta_verbose("%s\n", __func__);
        priv->hovered_clone = cloned;
        _move_close_button_for(self, priv->hovered_clone);
        gtk_widget_set_opacity(priv->close_button, 1.0);
        return TRUE;
    }

    /* pass to parent workspace */
    return FALSE;
}

static gboolean on_close_button_clicked(GtkWidget* widget,
               GdkEvent* event, gpointer data)
{
    meta_verbose("%s\n", __func__);
    DeepinWorkspaceOverview* self = (DeepinWorkspaceOverview*)data;
    DeepinWorkspaceOverviewPrivate* priv = self->priv;

    if (!priv->ready) return FALSE;

    MetaWindow* meta_window = meta_deepin_cloned_widget_get_window(priv->hovered_clone);
    for (int i = 0; i < priv->monitors->len; i++) {
        MonitorData* md = (MonitorData*)g_ptr_array_index(priv->monitors, i);
        if (!md->clones) continue;

        for (gint i = 0; i < md->clones->len; i++) {
            MetaDeepinClonedWidget* clone = g_ptr_array_index(md->clones, i);
            if (clone == priv->hovered_clone) {
                g_ptr_array_remove(md->clones, clone);
                gtk_container_remove(GTK_CONTAINER(self), clone);
                meta_window_delete(meta_window, CurrentTime);

                priv->hovered_clone = NULL;
                _hide_close_button(self);

                g_idle_add((GSourceFunc)on_idle, self);
                return TRUE;
            }
        }
    }


    return TRUE;
}

static gboolean on_close_button_leaved(GtkWidget* widget,
               GdkEvent* event, gpointer data)
{
    meta_verbose("%s\n", __func__);
    DeepinWorkspaceOverview* self = (DeepinWorkspaceOverview*)data;

    if (!self->priv->hovered_clone) return FALSE;
    // redirect to hover window
    return on_deepin_cloned_widget_leaved(self->priv->hovered_clone, event, data);
}

static gint get_nearest_monitor (GdkScreen *screen,
		     gint       x,
		     gint       y)
{
    gint num_monitors, i;
    gint nearest_dist = G_MAXINT;
    gint nearest_monitor = 0;

    num_monitors = gdk_screen_get_n_monitors (screen);

    for (i = 0; i < num_monitors; i++) {
        GdkRectangle monitor;
        gint dist_x, dist_y, dist;

        gdk_screen_get_monitor_geometry (screen, i, &monitor);

        if (x < monitor.x)
            dist_x = monitor.x - x;
        else if (x >= monitor.x + monitor.width)
            dist_x = x - (monitor.x + monitor.width) + 1;
        else
            dist_x = 0;

        if (y < monitor.y)
            dist_y = monitor.y - y;
        else if (y >= monitor.y + monitor.height)
            dist_y = y - (monitor.y + monitor.height) + 1;
        else
            dist_y = 0;

        dist = dist_x + dist_y;
        if (dist < nearest_dist) {
            nearest_dist = dist;
            nearest_monitor = i;
        }
    }

    return nearest_monitor;
}

//borrowed from gtk
static gint get_monitor_at_window(MetaWindow* window)
{
    gint num_monitors, i, area = 0, screen_num = -1;
    GdkRectangle win_rect;

    GdkScreen* screen = gdk_screen_get_default();

    meta_window_get_outer_rect(window, (MetaRectangle*)&win_rect);
    num_monitors = gdk_screen_get_n_monitors (screen);

    for (i=0; i<num_monitors; i++) {
        GdkRectangle tmp_monitor, intersect;

        gdk_screen_get_monitor_geometry (screen, i, &tmp_monitor);
        gdk_rectangle_intersect (&win_rect, &tmp_monitor, &intersect);

        if (intersect.width * intersect.height > area) { 
            area = intersect.width * intersect.height;
            screen_num = i;
        }
    }
    if (screen_num >= 0)
        return screen_num;
    else
        return get_nearest_monitor (screen,
                win_rect.x + win_rect.width / 2,
                win_rect.y + win_rect.height / 2);
}

static MetaDeepinClonedWidget* _clone_window(DeepinWorkspaceOverview* self, 
        MetaWindow* window)
{
    DeepinWorkspaceOverviewPrivate* priv = self->priv;
    GtkWidget* widget = meta_deepin_cloned_widget_new(window, TRUE);
    gtk_widget_set_sensitive(widget, TRUE);

    ClonedPrivateInfo* info = clone_get_info(widget);

    info->monitor = get_monitor_at_window(window);
    g_assert(info->monitor < priv->monitors->len);
    MonitorData* md = g_ptr_array_index(priv->monitors, info->monitor);
    g_ptr_array_add(md->clones, widget);

    MetaRectangle r;
    meta_window_get_outer_rect(window, &r);
    meta_deepin_cloned_widget_set_size(
            META_DEEPIN_CLONED_WIDGET(widget), r.width, r.height);
    meta_deepin_cloned_widget_set_render_frame(
            META_DEEPIN_CLONED_WIDGET(widget), TRUE);

    //it doesn't matter where we put it, since we'll move it very soon.
    deepin_fixed_put(DEEPIN_FIXED(self), widget, 0, 0);

    g_object_connect(G_OBJECT(widget),
            "signal::enter-notify-event", on_deepin_cloned_widget_entered, self,
            "signal::leave-notify-event", on_deepin_cloned_widget_leaved, self,
            "signal::motion-notify-event", on_deepin_cloned_widget_motion, self,
            "signal::button-release-event", on_deepin_cloned_widget_released, self,
            NULL);

    return META_DEEPIN_CLONED_WIDGET(widget);
}

//TODO: response to screen-changed?
void deepin_workspace_overview_populate(DeepinWorkspaceOverview* self,
        MetaWorkspace* ws)
{
    DeepinWorkspaceOverviewPrivate* priv = self->priv;
    priv->workspace = ws;

    int workspace_index = meta_workspace_index(priv->workspace);

    GdkScreen* screen = gdk_screen_get_default();
    priv->primary = gdk_screen_get_primary_monitor(screen);
    gint n_monitors = gdk_screen_get_n_monitors(screen);
    priv->monitors = g_ptr_array_new_full(n_monitors, monitor_data_destroy);
    for (int i = 0; i < n_monitors; i++) {
        MonitorData* md = monitor_data_new();
        g_ptr_array_add(priv->monitors, md);
        md->monitor = i;
        md->clones = g_ptr_array_new();
        gdk_screen_get_monitor_geometry(screen, i, (GdkRectangle*)&md->mon_rect);

        /**
         * gdk_screen_get_monitor_workarea fails to honor struts, so I have to use xinerama
         * to get correct workarea
         */

        const MetaXineramaScreenInfo* xinerama = meta_screen_get_xinerama_for_rect(ws->screen, &md->mon_rect);
        meta_workspace_get_work_area_for_xinerama(ws, xinerama->number, (GdkRectangle*)&md->mon_workarea);
    }

    GList* ls = meta_stack_list_windows(ws->screen->stack,
            priv->all_window_mode? NULL: ws);
    GList* l = ls;
    while (l) {
        MetaWindow* win = (MetaWindow*)l->data;
        if (win->type == META_WINDOW_NORMAL) {
            if (priv->present_xids && g_hash_table_size(priv->present_xids)) {
                if (g_hash_table_contains(priv->present_xids, GINT_TO_POINTER(win->xwindow))) {
                    _clone_window(self, win);
                }
            } else {
                _clone_window(self, win);
            }
        }

        l = l->next;
    }
    g_list_free(ls);

    {
        priv->close_button = deepin_stated_image_new_from_file ("close");

        deepin_fixed_put(DEEPIN_FIXED(self), priv->close_button, 0, 0);
        gtk_widget_set_opacity(self->priv->close_button, 0.0);

        g_object_connect(G_OBJECT(priv->close_button), 
                "signal::leave-notify-event", on_close_button_leaved, self,
                "signal::button-release-event", on_close_button_clicked, self,
                NULL);
    }


    priv->dock_height = 0;
    MetaWindow *dock_win = NULL;

    GList* windows = priv->workspace->mru_list;
    while (windows != NULL) {
        MetaWindow *w = (MetaWindow*)windows->data;
        if (w->type == META_WINDOW_DOCK) {
            dock_win = w;
            break;
        }

        windows = windows->next;
    }

    for (int i = 0; i < n_monitors; i++) {
        MonitorData* md = (MonitorData*)g_ptr_array_index(priv->monitors, i);
        if (i == priv->primary) {
            MetaRectangle r2 = {0, 0, 0, 0};
            cairo_surface_t *aux2 = NULL;

            if (dock_win) {
                meta_window_get_outer_rect(dock_win, &r2);
                aux2 = deepin_window_surface_manager_get_surface(dock_win, 1.0); 
                priv->dock_height = r2.height;
                r2.x -= md->mon_rect.x;
                r2.y -= md->mon_rect.y;
                meta_verbose ("%s: dock offset(%d, %d)\n", __func__, r2.x, r2.y);
            }

            md->desktop_surface = deepin_window_surface_manager_get_combined3(
                    deepin_background_cache_get_surface(md->monitor, workspace_index, 1.0), 
                    NULL, 0, 0,
                    aux2, r2.x, r2.y,
                    1.0);
        } else {
            md->desktop_surface = deepin_background_cache_get_surface(md->monitor, workspace_index, 1.0);
            if (md->desktop_surface) cairo_surface_reference(md->desktop_surface);
        }
    }

    gtk_widget_queue_resize(GTK_WIDGET(self));
}

static void on_deepin_workspace_overview_show(DeepinWorkspaceOverview* self, gpointer data)
{
    DeepinWorkspaceOverviewPrivate* priv = self->priv;
    g_idle_add((GSourceFunc)on_idle, self);
}

static gboolean on_deepin_workspace_overview_pressed(DeepinWorkspaceOverview* self,
               GdkEvent* event, gpointer user_data)
{
    DeepinWorkspaceOverviewPrivate* priv = self->priv;
    meta_verbose("%s: ws %s\n", __func__, meta_workspace_get_name(priv->workspace));

    if (!priv->ready || priv->hovered_clone) return TRUE;

    g_idle_add((GSourceFunc)on_idle_end_grab, gdk_event_get_time(event));
    return TRUE;
}

static void on_window_change_workspace(DeepinMessageHub* hub, MetaWindow* window,
        MetaWorkspace* new_workspace, gpointer user_data)
{
    DeepinWorkspaceOverview* self = (DeepinWorkspaceOverview*)user_data;
    DeepinWorkspaceOverviewPrivate* priv = self->priv;

    if (!priv->ready) return;
    
    if (priv->workspace == new_workspace) { // dest workspace
        if (window->type != META_WINDOW_NORMAL) return;
        meta_verbose("%s: add window\n", __func__);

        //add window
        MetaDeepinClonedWidget* widget = _clone_window(self, window);
        gtk_widget_show(widget);
        g_idle_add((GSourceFunc)on_idle, self);

    } else if (window->workspace == priv->workspace) { // maybe source workspace
        on_window_removed(hub, window, user_data);
    }
}
    
GtkWidget* deepin_workspace_overview_new(void)
{
    DeepinWorkspaceOverview* self = (DeepinWorkspaceOverview*)g_object_new(
            DEEPIN_TYPE_WORKSPACE_OVERVIEW, NULL);

    GdkScreen* screen = gdk_screen_get_default();

    self->priv->fixed_width = gdk_screen_get_width(screen);
    self->priv->fixed_height = gdk_screen_get_height(screen);

    SET_STATE (self, GTK_STATE_FLAG_NORMAL);
    deepin_setup_style_class(GTK_WIDGET(self), "deepin-workspace-clone"); 
    
    g_object_connect(G_OBJECT(self),
            "signal::show", on_deepin_workspace_overview_show, NULL,
            "signal::button-press-event", on_deepin_workspace_overview_pressed, NULL,
            NULL);

    g_object_connect(G_OBJECT(deepin_message_hub_get()), 
            "signal::window-removed", on_window_removed, self,
            "signal::about-to-change-workspace", on_window_change_workspace, self,
            NULL);

    return (GtkWidget*)self;
}

void deepin_workspace_overview_focus_next(DeepinWorkspaceOverview* self,
        gboolean backward)
{
    DeepinWorkspaceOverviewPrivate* priv = self->priv;
    gint i = 0, j = 0;

    if (priv->monitors->len == 0) return;

    if (priv->window_need_focused) {
        gboolean found = FALSE;
        for (i = 0; i < priv->monitors->len; i++) {
           MonitorData* md = (MonitorData*)g_ptr_array_index(priv->monitors, i);
           GPtrArray* clones = md->clones;
           if (!clones || clones->len == 0) continue;

           for (j = 0; j < clones->len; j++) {
               MetaDeepinClonedWidget* clone = g_ptr_array_index(clones, j);
               if (clone == priv->window_need_focused) {
                   found = TRUE;
                   break;
               }
           }

           if (found) break;
       }

       if (i == priv->monitors->len) { i = j = 0; }
       MonitorData* md = (MonitorData*)g_ptr_array_index(priv->monitors, i);
       if (backward) {
           j--;
           if (j < 0) { i--; j = 0; }
       } else {
           j++;
           if (j == md->clones->len) { i++; j = 0; }
       }
       if (i == priv->monitors->len) { i = j = 0; }
    }

#define SCALE_FACTOR 1.03
    if (priv->window_need_focused) {
        double scale = 1.0;
        meta_deepin_cloned_widget_set_scale(priv->window_need_focused, scale, scale);
        meta_deepin_cloned_widget_unselect(priv->window_need_focused);
        if (priv->hovered_clone == priv->window_need_focused) {
            _move_close_button_for(self, priv->window_need_focused);
        }
    }

    MonitorData* md = (MonitorData*)g_ptr_array_index(priv->monitors, i);
    if (md->clones->len == 0) {
        return;
    }

    MetaDeepinClonedWidget* next = g_ptr_array_index(md->clones, j);
    double scale = SCALE_FACTOR;
    meta_deepin_cloned_widget_set_scale(next, scale, scale);
    meta_deepin_cloned_widget_select(next);

    if (priv->hovered_clone == next) {
        _move_close_button_for(self, next);
    }
    priv->window_need_focused = next;
}

MetaDeepinClonedWidget* deepin_workspace_overview_get_focused(DeepinWorkspaceOverview* self)
{
    DeepinWorkspaceOverviewPrivate* priv = self->priv;
    return priv->window_need_focused;
}

void deepin_workspace_overview_handle_event(DeepinWorkspaceOverview* self,
        XIDeviceEvent* event, KeySym keysym, MetaKeyBindingAction action)
{
    DeepinWorkspaceOverviewPrivate* priv = self->priv;
    if (!priv->ready) return;

    gboolean backward = FALSE;
    if (keysym == XK_Tab
            || action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS
            || action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD) {
        meta_verbose("tabbing inside expose windows\n");
        if (keysym == XK_Tab)
            backward = event->mods.base & ShiftMask;
        else
            backward = action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD;
        deepin_workspace_overview_focus_next(self, backward);

    } else if (keysym == XK_Return) {
        MetaDeepinClonedWidget* clone = deepin_workspace_overview_get_focused(self);
        if (!clone) {
            meta_workspace_focus_default_window(priv->workspace, NULL, event->time);
        } else {
            MetaWindow* mw = meta_deepin_cloned_widget_get_window(clone);
            g_assert(mw != NULL);
            if (mw->workspace && mw->workspace != priv->workspace) {
                meta_workspace_activate(mw->workspace, event->time);
            }
            meta_window_activate(mw, event->time);
        }

        g_idle_add((GSourceFunc)on_idle_end_grab, event->time);
    }
}

MetaWorkspace* deepin_workspace_overview_get_workspace(DeepinWorkspaceOverview* self)
{
    return self->priv->workspace;
}

GdkWindow* deepin_workspace_overview_get_event_window(DeepinWorkspaceOverview* self)
{
    return self->priv->event_window;
}

void deepin_workspace_overview_set_show_all_windows(DeepinWorkspaceOverview* self,
        gboolean val)
{
    self->priv->all_window_mode = val;
}

void deepin_workspace_overview_set_present_windows(DeepinWorkspaceOverview* self, GVariant* xids)
{
    DeepinWorkspaceOverviewPrivate* priv = self->priv;
    if (xids) {
        priv->all_window_mode = TRUE;
        priv->present_xids = g_hash_table_new(g_direct_hash, g_direct_equal);

        guint32 xid;
        GVariantIter* vi = g_variant_iter_new(xids);
        while (g_variant_iter_next(vi, "u", &xid, NULL)) {
            g_hash_table_insert(priv->present_xids, GINT_TO_POINTER(xid), GINT_TO_POINTER(1));
            meta_verbose("presenting xid %d\n", xid);
        }
        g_variant_iter_free(vi);
    }
}

gboolean deepin_workspace_overview_get_is_all_window_mode(DeepinWorkspaceOverview* self)
{
    return self->priv->all_window_mode;
}

