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
#include <util.h>
#include <stdlib.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <prefs.h>
#include "deepin-wm-background.h"
#include "../core/screen-private.h"
#include "../core/display-private.h"
#include "deepin-design.h"
#include "deepin-shadow-workspace.h"
#include "deepin-stated-image.h"
#include "deepin-window-surface-manager.h"
#include "deepin-fixed.h"
#include "deepin-workspace-adder.h"

typedef enum WorkspaceDragOperation {
    DRAG_OP_NONE,
    DRAG_TO_REMOVE,
    DRAG_TO_SWITCH,
} WorkspaceDragOperation;

struct _DeepinWMBackgroundPrivate
{
    MetaScreen* screen;
    GdkScreen* gscreen;

    gint disposed: 1;
    gint workspace_changing: 1;

    GtkWidget* fixed;

    DeepinShadowWorkspace* active_workspace;
    GList* workspaces;
    GList* workspace_thumbs;
    DeepinWorkspaceAdder* adder;

    /* calculation cache */
    int top_offset;
    int bottom_offset;
    float scale;
    gint width, height; /* for workspace */
    gint thumb_width, thumb_height; /* for top thumbs */

    guint idle_id;

    DeepinShadowWorkspace* hover_ws;
    GtkWidget* close_button; /* for hovered workspace thumb */

    WorkspaceDragOperation current_op;
};


G_DEFINE_TYPE (DeepinWMBackground, deepin_wm_background, GTK_TYPE_WINDOW);

static inline gboolean _show_adder(MetaScreen* screen)
{
    return meta_screen_get_n_workspaces(screen) < MAX_WORKSPACE_NUM;
}

static inline gboolean _show_closer(MetaScreen* screen)
{
    return meta_screen_get_n_workspaces(screen) > 1;
}

static void _hide_close_button(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv= self->priv;

    if (priv->close_button) {
        gtk_widget_set_opacity(priv->close_button, 0.0);
        deepin_fixed_move(DEEPIN_FIXED(priv->fixed), priv->close_button,
                -100, -100,
                FALSE);
    }
}

static void _move_close_button_for(DeepinWMBackground* self,
        DeepinShadowWorkspace* ws_thumb)
{
    DeepinWMBackgroundPrivate* priv= self->priv;

    GtkAllocation alloc;
    gtk_widget_get_allocation(GTK_WIDGET(ws_thumb), &alloc);

    gint x = 0, y = 0;
    gtk_container_child_get(GTK_CONTAINER(priv->fixed), GTK_WIDGET(ws_thumb),
            "x", &x, "y", &y, NULL);

    deepin_fixed_raise(DEEPIN_FIXED(priv->fixed), priv->close_button);
    deepin_fixed_move(DEEPIN_FIXED(priv->fixed), priv->close_button,
            x + alloc.width/2,
            y - alloc.height/2,
            FALSE);
}

static gint find_by_meta_workspace(gconstpointer a, gconstpointer b)
{
    DeepinShadowWorkspace* ws = (DeepinShadowWorkspace*)a;
    MetaWorkspace* meta = (MetaWorkspace*)b;
    if (deepin_shadow_workspace_get_workspace(ws) == meta) {
        return 0;
    }

    return 1;
}

static DeepinShadowWorkspace* _find_workspace(GList* l, MetaWorkspace* next)
{
    GList* tmp = g_list_find_custom(l, next, find_by_meta_workspace);
    g_assert(tmp->data);
    return (DeepinShadowWorkspace*)tmp->data;
}

static void _create_workspace(DeepinWMBackground*);

static void _delete_workspace(DeepinWMBackground*, DeepinShadowWorkspace*);

static gboolean on_deepin_workspace_adder_adder_pressed(GtkWidget* adder, GdkEvent* event, gpointer user_data)
{
    DeepinWMBackground* self = DEEPIN_WM_BACKGROUND(user_data);
    _create_workspace(self);
    return TRUE;
}

struct IdleData {
    MetaWindow *target_window;
    MetaWorkspace *target_workspace;
};

static gboolean on_idle_move_window(struct IdleData *data)
{
    meta_window_change_workspace(data->target_window, data->target_workspace);
    g_free(data);
    return G_SOURCE_REMOVE;
}

static void on_deepin_workspace_adder_drag_data_received(GtkWidget* widget, GdkDragContext* context,
        gint x, gint y, GtkSelectionData *data, guint info,
        guint time, gpointer user_data)
{
    DeepinWMBackground *self = DEEPIN_WM_BACKGROUND(user_data);
    DeepinWMBackgroundPrivate *priv = self->priv;
    meta_verbose("%s: x %d, y %d\n", __func__, x, y);

    const guchar* raw_data = gtk_selection_data_get_data(data);
    if (raw_data) {
        gpointer p = (gpointer)atol(raw_data);
        MetaDeepinClonedWidget* target_clone = META_DEEPIN_CLONED_WIDGET(p);
        MetaWindow* meta_win = meta_deepin_cloned_widget_get_window(target_clone);
        meta_verbose("%s: get %p\n", __func__, target_clone);
        if (meta_win->on_all_workspaces) {
            gtk_drag_finish(context, FALSE, FALSE, time);
            return;
        }

        gtk_drag_finish(context, TRUE, FALSE, time);
        _create_workspace(self);
        struct IdleData *data = g_new0(struct IdleData, 1);
        data->target_window = meta_win;
        data->target_workspace = priv->screen->active_workspace;
        g_idle_add((GSourceFunc)on_idle_move_window, data);

    } else 
        gtk_drag_finish(context, FALSE, FALSE, time);
}

static void _create_adder(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    priv->adder = (DeepinWorkspaceAdder*)deepin_workspace_adder_new();
    gtk_widget_set_size_request(GTK_WIDGET(priv->adder), 
            priv->thumb_width, priv->thumb_height);

    static GtkTargetEntry targets[] = {
        {(char*)"window", GTK_TARGET_OTHER_WIDGET, DRAG_TARGET_WINDOW},
    };
    // as drop target to create new workspace
    gtk_drag_dest_set(GTK_WIDGET(priv->adder),
            GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
            targets, 1, GDK_ACTION_COPY);

    g_object_connect(G_OBJECT(priv->adder),
            "signal::drag-data-received", on_deepin_workspace_adder_drag_data_received, self, 
            "signal::button-release-event", on_deepin_workspace_adder_adder_pressed, self,
            NULL);
}

static void relayout(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    gboolean adder_renewed = FALSE;

    if (!_show_adder(priv->screen) && priv->adder) {
        gtk_container_remove(GTK_CONTAINER(priv->fixed), GTK_WIDGET(priv->adder));
        priv->adder = NULL;

    } else if (_show_adder(priv->screen) && !priv->adder) {
        _create_adder(self);
        gtk_widget_show(GTK_WIDGET(priv->adder));

        adder_renewed = TRUE;
    }

    GList *l = priv->screen->workspaces;
    gint current = g_list_index(priv->workspaces, priv->active_workspace);
    if (current < 0) current = 0;
    meta_verbose("current: %d\n", current);
    
    GdkRectangle geom;
    gint monitor_index = gdk_screen_get_primary_monitor(priv->gscreen);
    gdk_screen_get_monitor_geometry(priv->gscreen, monitor_index, &geom);
            

    gint i = 0, pad = FLOW_CLONE_DISTANCE_PERCENT * geom.width;
    l = priv->workspaces;
    while (l) {
        gint x = (geom.width - priv->width) / 2 +  (i - current) * (priv->width + pad);
        deepin_fixed_move(DEEPIN_FIXED(priv->fixed), (GtkWidget*)l->data,
                x + priv->width/2, priv->top_offset + priv->height/2,
                FALSE);

        i++;
        l = l->next;
    }

    i = 0;
    l = priv->workspace_thumbs;
    int thumb_spacing = geom.width * SPACING_PERCENT;
    
    gint count = g_list_length(priv->workspace_thumbs) + _show_adder(priv->screen);
    int thumb_y = (int)(geom.height * HORIZONTAL_OFFSET_PERCENT);
    int thumb_x = (geom.width - count * (priv->thumb_width + thumb_spacing))/2;

    while (l) {
        int x = thumb_x + i * (priv->thumb_width + thumb_spacing);
        deepin_fixed_move(DEEPIN_FIXED(priv->fixed), (GtkWidget*)l->data,
                x + priv->thumb_width/2, thumb_y + priv->thumb_height/2,
                FALSE);

        i++;
        l = l->next;
    }

    if (priv->adder) {
        int x = thumb_x + i * (priv->thumb_width + thumb_spacing);
        int y = thumb_y + priv->thumb_height/2;

        if (adder_renewed) {
            deepin_fixed_put(DEEPIN_FIXED(priv->fixed), (GtkWidget*)priv->adder,
                    x + priv->thumb_width/2, y);
        } else {
            deepin_fixed_move(DEEPIN_FIXED(priv->fixed), (GtkWidget*)priv->adder,
                    x + priv->thumb_width/2, y, FALSE);
        }
    }
}

static gboolean on_close_button_clicked(GtkWidget* widget,
               GdkEvent* event, gpointer data)
{
    meta_verbose("%s\n", __func__);
    DeepinWMBackground* self = (DeepinWMBackground*)data;
    DeepinWMBackgroundPrivate* priv = self->priv;

    gint index = g_list_index(priv->workspace_thumbs, priv->hover_ws);
    DeepinShadowWorkspace* ws = g_list_nth(priv->workspaces, index)->data;
    _delete_workspace(self, ws);

    return TRUE;
}

static gboolean on_close_button_leaved(GtkWidget* widget,
               GdkEvent* event, gpointer data)
{
    meta_verbose("%s\n", __func__);
    DeepinWMBackground* self = (DeepinWMBackground*)data;
    DeepinWMBackgroundPrivate* priv = self->priv;

    if (priv->hover_ws) {
        /* FIXME: there is a problem: when cloned is gets focused (so scaled up),
         * leave event will looks like as if it happened inside of cloned. need
         * a workaround */
        gint x, y;
        x = event->crossing.x_root;
        y = event->crossing.y_root;

        GtkAllocation alloc;
        gtk_widget_get_allocation(GTK_WIDGET(priv->hover_ws), &alloc);

        GdkRectangle r = {alloc.x, alloc.y, alloc.width, priv->thumb_height};
        if (x > r.x && x < r.x + r.width && y > r.y && y < r.y + r.height) {
            return FALSE;
        }
    }

    priv->hover_ws = NULL;
    _hide_close_button(self);
    return FALSE;
}

static void _create_close_button(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv= self->priv;

    priv->close_button = deepin_stated_image_new_from_file ("close");

    deepin_fixed_put(DEEPIN_FIXED(priv->fixed), priv->close_button, -100, -100);
    gtk_widget_set_opacity(priv->close_button, 0.0);

    g_object_connect(G_OBJECT(priv->close_button), 
            "signal::leave-notify-event", on_close_button_leaved, self,
            "signal::button-release-event", on_close_button_clicked, self,
            NULL);
}

static gboolean on_workspace_released(DeepinShadowWorkspace* ws,
               GdkEvent* event, gpointer user_data)
{
    meta_verbose("%s\n", __func__);
    DeepinWMBackground* self = (DeepinWMBackground*)user_data;
    DeepinWMBackgroundPrivate* priv = self->priv;
    
    if (priv->active_workspace != ws) {
        MetaWorkspace* next = deepin_shadow_workspace_get_workspace(ws);
        deepin_wm_background_switch_workspace(self, next);
        return TRUE;
    } 

    return FALSE;
}

static void place_workspace_thumb(DeepinWMBackground *self, DeepinShadowWorkspace *dsw_thumb, int new_index)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    GdkRectangle geom;
    gint monitor_index = gdk_screen_get_primary_monitor(priv->gscreen);
    gdk_screen_get_monitor_geometry(priv->gscreen, monitor_index, &geom);

    int thumb_spacing = geom.width * SPACING_PERCENT;

    gint count = g_list_length(priv->workspace_thumbs) + _show_adder(priv->screen);
    int thumb_y = (int)(geom.height * HORIZONTAL_OFFSET_PERCENT);
    int thumb_x = (geom.width - count * (priv->thumb_width + thumb_spacing))/2;

    int x = thumb_x + new_index * (priv->thumb_width + thumb_spacing);
    deepin_fixed_move(DEEPIN_FIXED(priv->fixed), GTK_WIDGET(dsw_thumb),
            x + priv->thumb_width/2, thumb_y + priv->thumb_height/2,
            FALSE);

}

static void start_reorder_workspace_thumbs(DeepinWMBackground *self, DeepinShadowWorkspace *dsw_dragging,
        DeepinShadowWorkspace *dsw_switching)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    int i = g_list_index(priv->workspace_thumbs, dsw_dragging),
        j = g_list_index(priv->workspace_thumbs, dsw_switching);
    /*fprintf(stderr, "switch %d => %d\n", i, j);*/

    priv->workspace_thumbs = g_list_remove(priv->workspace_thumbs, dsw_dragging);
    priv->workspace_thumbs = g_list_insert(priv->workspace_thumbs, dsw_dragging, j);
    place_workspace_thumb (self, dsw_dragging, j);

    int d = i < j ? 1 : -1;
    for (int k = i; d > 0 ? k < j : k > j; k += d) {
        place_workspace_thumb(self, g_list_nth_data(priv->workspace_thumbs, k), k);
    }
}

static gboolean rect_contains(GdkRectangle* r, int x, int y)
{
    return r->x < x && r->y < y && r->x+r->width > x && r->y+r->height > y;
}

static GdkRectangle drag_to_remove_box = {0, 0, -1, -1};

static GdkRectangle get_drag_to_remove_area(DeepinWMBackground* self, DeepinShadowWorkspace *dsw)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    if (drag_to_remove_box.width < 0) {
        GdkRectangle geom;
        gint monitor_index = gdk_screen_get_primary_monitor(priv->gscreen);
        gdk_screen_get_monitor_geometry(priv->gscreen, monitor_index, &geom);

        int thumb_spacing = geom.width * SPACING_PERCENT;
        float allowed_height = geom.height * FLOW_CLONE_TOP_OFFSET_PERCENT;

        GtkAllocation alloc;
        gtk_widget_get_allocation(dsw, &alloc);

        drag_to_remove_box.x = alloc.x - thumb_spacing - priv->thumb_width / 2;
        drag_to_remove_box.y = 0;
        drag_to_remove_box.width = (priv->thumb_width + thumb_spacing) * 2;
        drag_to_remove_box.height = allowed_height;

        /*fprintf(stderr, "drag_to_remove_area (%d, %d, %d, %d)\n",*/
                /*drag_to_remove_box.x, drag_to_remove_box.y,*/
                /*drag_to_remove_box.width, drag_to_remove_box.height);*/
    }

    return drag_to_remove_box;
}

WorkspaceDragOperation get_drag_operation(DeepinWMBackground* self, DeepinShadowWorkspace* dsw, int x, int y)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    WorkspaceDragOperation old_op = priv->current_op;

    if (priv->current_op == DRAG_TO_SWITCH) {
        return priv->current_op;
    }
    GdkRectangle box = get_drag_to_remove_area(self, dsw);

    if (rect_contains(&box, x, y)) {
        priv->current_op = DRAG_TO_REMOVE;
    } else {
        priv->current_op = DRAG_TO_SWITCH;
    }

    if (old_op != priv->current_op)
        /*fprintf(stderr, "%s: op -> %s\n", __func__,*/
                /*priv->current_op == DRAG_TO_REMOVE ? "drag to remove" :*/
                /*priv->current_op == DRAG_TO_SWITCH ? "drag to switch" : "none");*/
    return priv->current_op;
}

// widget could be DeepinWMBackground or DeepinShadowWorkspace, and the drag rules
// should follow deepin-wm
static void _handle_drag_motion(GtkWidget* widget, GdkDragContext* context,
               gint x, gint y, guint time, DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    gint ox, oy;

    GdkDevice *dev = gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_display_get_default()));
    gdk_device_get_position(dev, NULL, &x, &y);

    GtkWidget *top = gtk_widget_get_toplevel(widget);
    gdk_window_get_position(gtk_widget_get_window(top), &ox, &oy);
    /*fprintf(stderr, "%s: (%d, %d), - (%d, %d)\n", __func__, x, y, ox, oy);*/
    x -= ox;
    y -= oy;

    DeepinShadowWorkspace *dsw_switching = NULL;
    DeepinShadowWorkspace *dsw_dragging = NULL;
    GList *l = priv->workspace_thumbs;
    while (l) {
        DeepinShadowWorkspace *dsw = DEEPIN_SHADOW_WORKSPACE(l->data);

        if (dsw_switching == NULL) {
            GtkAllocation alloc;
            gtk_widget_get_allocation(GTK_WIDGET(dsw), &alloc);

            if (rect_contains(&alloc, x, y)) {
                dsw_switching = dsw;
            }
        }

        if (dsw_dragging == NULL) {
            if (deepin_shadow_workspace_is_dragging(dsw)) 
                dsw_dragging = dsw;
        }
        l = l->next;
    }

    if (dsw_switching == dsw_dragging) 
        dsw_switching = NULL;

    // we need to take care of the case when a cloned widget is dragging, 
    // so no dsw_dragging at all
    if (dsw_dragging == NULL) {
        return;
    }

    WorkspaceDragOperation op = get_drag_operation(self, dsw_dragging, x, y);
    switch(op) {
        case DRAG_TO_REMOVE: {
            GtkAllocation alloc;
            gtk_widget_get_allocation(dsw_dragging, &alloc);
            if (y - alloc.y < 10) {
                deepin_shadow_workspace_show_remove_tip(dsw_dragging, TRUE);
            } else {
                deepin_shadow_workspace_show_remove_tip(dsw_dragging, FALSE);
                priv->current_op = DRAG_OP_NONE;
                gtk_widget_queue_draw(dsw_dragging);
            }
            break;
        }

        case DRAG_TO_SWITCH:
            if (deepin_shadow_workspace_get_is_show_remove_tip(dsw_dragging))
                deepin_shadow_workspace_show_remove_tip(dsw_dragging, FALSE);

            if (dsw_switching && dsw_dragging) {
                /*fprintf(stderr, "%s: switching #%d with #%d\n", __func__,*/
                        /*g_list_index(priv->workspace_thumbs, dsw_dragging),*/
                        /*g_list_index(priv->workspace_thumbs, dsw_switching));*/
                meta_verbose("%s: switching #%d with #%d\n", __func__,
                        g_list_index(priv->workspace_thumbs, dsw_dragging),
                        g_list_index(priv->workspace_thumbs, dsw_switching));
                start_reorder_workspace_thumbs(self, dsw_dragging, dsw_switching);
            }
            break;
    }
}

static gboolean on_workspace_thumb_drag_motion(GtkWidget* ws_thumb, GdkDragContext* context,
               gint x, gint y, guint time, gpointer user_data)
{
    DeepinWMBackground* self = (DeepinWMBackground*)user_data;
    _handle_drag_motion(ws_thumb, context, x, y, time, self);
    return TRUE;
}

static gboolean on_workspace_thumb_released(DeepinShadowWorkspace* ws_thumb,
               GdkEvent* event, gpointer user_data)
{
    meta_verbose("%s\n", __func__);
    DeepinWMBackground* self = (DeepinWMBackground*)user_data;
    DeepinWMBackgroundPrivate* priv = self->priv;
    
    MetaWorkspace* next = deepin_shadow_workspace_get_workspace(ws_thumb);
    if (deepin_shadow_workspace_get_workspace(priv->active_workspace) != next) {
        deepin_wm_background_switch_workspace(self, next);
    } 
    return TRUE;
}

static gboolean on_workspace_thumb_leaved(DeepinShadowWorkspace* ws_thumb,
               GdkEvent* event, gpointer data)
{
    DeepinWMBackground* self = (DeepinWMBackground*)data;
    meta_verbose("%s\n", __func__);

    gint x, y;
    x = event->crossing.x_root;
    y = event->crossing.y_root;

    GtkAllocation alloc;
    gtk_widget_get_allocation(GTK_WIDGET(ws_thumb), &alloc);

    GdkRectangle r = {alloc.x, alloc.y, alloc.width, alloc.height};
    if (x > r.x && x < r.x + r.width && y > r.y && y < r.y + r.height) {
        return FALSE;
    }

    self->priv->hover_ws = NULL;
    _hide_close_button(self);
    return TRUE;
}

static const int SMOOTH_SCROLL_DELAY = 500;
static gboolean is_smooth_scrolling = FALSE;
static gboolean on_scroll_timeout(gpointer data)
{
    is_smooth_scrolling = FALSE;
    return G_SOURCE_REMOVE;
}

static int scroll_amount = 0;
static gboolean on_background_scrolled(DeepinWMBackground* self,
               GdkEvent* event, gpointer user_data)
{
    GdkEventScroll scroll = event->scroll;
    DeepinWMBackgroundPrivate* priv = self->priv;
    MetaMotionDirection direction = META_MOTION_LEFT;

    double dx, dy;
    gdk_event_get_scroll_deltas(event, &dx, &dy);

    meta_verbose("%s, deltas %f, %f, direction %d\n", __func__, dx, dy, scroll.direction);
    if (scroll.direction != GDK_SCROLL_SMOOTH) {
        // non smooth scrolling handling
        if (scroll.direction == GDK_SCROLL_DOWN || scroll.direction == GDK_SCROLL_RIGHT) {
            direction = META_MOTION_RIGHT;
            scroll_amount--;
        } else {
            scroll_amount++;
        }
    } else {
        //this is smooth scrolling from deepin-wm

        // concept from maya to detect mouse wheel and proper smooth scroll and prevent too much
        // repetition on the events
        if (fabs (dy) == 1.0) {
            // mouse wheel scroll
            direction = dy > 0 ? META_MOTION_RIGHT : META_MOTION_LEFT;
        } else if (!is_smooth_scrolling) {
            // actual smooth scroll
            double choice = fabs (dx) > fabs (dy) ? dx : dy;

            if (choice > 0.3) {
                direction = META_MOTION_RIGHT;
            } else if (choice < -0.3) {
                direction = META_MOTION_LEFT;
            } else {
                return FALSE;
            }

            is_smooth_scrolling = TRUE;
            g_timeout_add(SMOOTH_SCROLL_DELAY, on_scroll_timeout, NULL);
        } else {
            // smooth scroll delay still active
            return FALSE;
        }
    }

    if (abs(scroll_amount) < 1) {
        return FALSE;
    }
    scroll_amount = 0;
    MetaWorkspace* current = deepin_shadow_workspace_get_workspace(priv->active_workspace); 
    MetaWorkspace* next = meta_workspace_get_neighbor(current, direction);
    if (next) {
        if (next == current) {
            //bouncing
            return FALSE;
        }

        deepin_wm_background_switch_workspace(self, next);
    }


    return FALSE;
}

static gboolean _idle_show_close_button(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    if (priv->disposed) return G_SOURCE_REMOVE;

    if (_show_closer(priv->screen) && priv->hover_ws) {
        _move_close_button_for(self, priv->hover_ws);
        gtk_widget_set_opacity(priv->close_button, 1.0);
    }

    priv->idle_id = 0;
    return G_SOURCE_REMOVE;
}

static gboolean on_workspace_thumb_entered(DeepinShadowWorkspace* ws_thumb,
               GdkEvent* event, gpointer data)
{
    DeepinWMBackground* self = DEEPIN_WM_BACKGROUND(data);
    meta_verbose("%s\n", __func__);

    self->priv->hover_ws = ws_thumb;
    self->priv->idle_id = g_idle_add((GSourceFunc)_idle_show_close_button, self);
    return TRUE;
}

static void deepin_wm_background_init (DeepinWMBackground *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_WM_BACKGROUND, DeepinWMBackgroundPrivate);

    DeepinWMBackgroundPrivate* priv = self->priv;
    priv->workspaces = NULL;
}

static void deepin_wm_background_dispose (GObject *object)
{
    DeepinWMBackground* self = DEEPIN_WM_BACKGROUND(object);
    DeepinWMBackgroundPrivate* priv = self->priv;

    if (!priv->disposed) {
        priv->disposed = TRUE;
        g_list_free(priv->workspaces);
        g_list_free(priv->workspace_thumbs);

        if (priv->idle_id > 0) {
            g_source_remove(priv->idle_id);
        }
    }

    G_OBJECT_CLASS (deepin_wm_background_parent_class)->dispose (object);
}

static void deepin_wm_background_finalize (GObject *object)
{
    /* TODO: Add deinitalization code here */

    G_OBJECT_CLASS (deepin_wm_background_parent_class)->finalize (object);
}

static gboolean deepin_wm_background_real_draw(GtkWidget *widget, cairo_t* cr)
{
    DeepinWMBackgroundPrivate* priv = DEEPIN_WM_BACKGROUND(widget)->priv;

    GtkAllocation req;
    gtk_widget_get_allocation(widget, &req);
    GtkStyleContext* context = gtk_widget_get_style_context(widget);
    gtk_render_background(context, cr, 0, 0, req.width, req.height);

    GdkRectangle geom;
    gint monitor_index = gdk_screen_get_primary_monitor(priv->gscreen);
    gdk_screen_get_monitor_geometry(priv->gscreen, monitor_index, &geom);

    cairo_rectangle(cr, geom.x, geom.y, geom.width, geom.height);
    cairo_clip(cr);

    return GTK_WIDGET_CLASS(deepin_wm_background_parent_class)->draw(widget, cr);
}

static void deepin_wm_background_class_init (DeepinWMBackgroundClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS (klass);

    g_type_class_add_private (klass, sizeof (DeepinWMBackgroundPrivate));
    widget_class->draw = deepin_wm_background_real_draw;

    object_class->finalize = deepin_wm_background_finalize;
    object_class->dispose = deepin_wm_background_dispose;
}

void deepin_wm_background_switch_workspace(DeepinWMBackground* self, 
        MetaWorkspace* next)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    DeepinShadowWorkspace* next_ws = _find_workspace(priv->workspaces, next);
    DeepinShadowWorkspace* next_thumb = _find_workspace(priv->workspace_thumbs, next);
    if (priv->active_workspace == next_ws) return;

    DeepinShadowWorkspace* current_thumb = _find_workspace(priv->workspace_thumbs,
                deepin_shadow_workspace_get_workspace(priv->active_workspace));
    DeepinShadowWorkspace* current = priv->active_workspace;

    deepin_shadow_workspace_set_current(current, FALSE);
    deepin_shadow_workspace_set_current(current_thumb, FALSE);

    deepin_shadow_workspace_set_current(next_ws, TRUE);
    deepin_shadow_workspace_set_current(next_thumb, TRUE);

    priv->active_workspace = next_ws;

    GdkRectangle geom;
    gint monitor_index = gdk_screen_get_primary_monitor(priv->gscreen);
    gdk_screen_get_monitor_geometry(priv->gscreen, monitor_index, &geom);

    gint i = 0,
         current_pos = g_list_index(priv->workspaces, next_ws),
         pad = FLOW_CLONE_DISTANCE_PERCENT * geom.width;

    GList* l = priv->workspaces;
    while (l) {
        gint x = (geom.width - priv->width) / 2 +  (i - current_pos) * (priv->width + pad);

        deepin_fixed_move(DEEPIN_FIXED(priv->fixed), (GtkWidget*)l->data,
                x + priv->width/2, priv->top_offset + priv->height/2, 
                FALSE);

        i++;
        l = l->next;
    }

    MetaWindow* focus_window = meta_stack_get_default_focus_window(
            priv->screen->stack, next, NULL);
    meta_verbose("%s: focus window %s\n", __func__, focus_window?focus_window->desc:NULL);
    meta_workspace_activate_with_focus(next, focus_window,
            gtk_get_current_event_time());
}

static void reorder_workspace(DeepinWMBackground *self, MetaWorkspace *ws, int new_index)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    meta_verbose("%s: from #%d -> #%d\n", __func__, meta_workspace_index(ws), new_index);
    /*fprintf(stderr, "%s: from #%d -> #%d\n", __func__, meta_workspace_index(ws), new_index);*/

    int old_index = meta_workspace_index(ws);
    DeepinShadowWorkspace *dsw_dragging = g_list_nth_data(priv->workspaces, old_index);
    priv->workspaces = g_list_remove(priv->workspaces, dsw_dragging);
    priv->workspaces = g_list_insert(priv->workspaces, dsw_dragging, new_index);

    meta_screen_reorder_workspace (ws->screen, ws, new_index);

    relayout(self);
}

struct ReorderData {
    DeepinWMBackground *wm;
    DeepinShadowWorkspace *dragging;
};

static gboolean on_idle_operate_workspace(struct ReorderData *data)
{
    DeepinWMBackgroundPrivate* priv = data->wm->priv;

    switch (priv->current_op) {
        case DRAG_TO_REMOVE: {
            int index = g_list_index(priv->workspace_thumbs, data->dragging);
            DeepinShadowWorkspace* ws = g_list_nth(priv->workspaces, index)->data;
            _delete_workspace(data->wm, ws);
            break;
        }

        case DRAG_TO_SWITCH: {
            int new_index = g_list_index(priv->workspace_thumbs, data->dragging);
            MetaWorkspace *ws = deepin_shadow_workspace_get_workspace(data->dragging);
            if (new_index != meta_workspace_index(ws))
                reorder_workspace(data->wm, ws, new_index);
            break;
        }
    }
    drag_to_remove_box = (GdkRectangle){0, 0, -1, -1};
    priv->current_op = DRAG_OP_NONE;

    g_free(data);
    return G_SOURCE_REMOVE;
}

void deepin_wm_background_request_workspace_drop_operation(DeepinWMBackground* self,
        DeepinShadowWorkspace *dsw_dragging)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    // this makes reordering/removing happens right after drag finished
    struct ReorderData *data = g_new0(struct ReorderData, 1);
    data->wm = self;
    data->dragging = dsw_dragging;
    g_idle_add(on_idle_operate_workspace, data);
}

static void on_deepin_wm_background_drag_data_received(GtkWidget* widget, GdkDragContext* context,
        gint x, gint y, GtkSelectionData *data, guint info,
        guint time, gpointer user_data)
{
    DeepinWMBackground* self = DEEPIN_WM_BACKGROUND(widget);
    DeepinWMBackgroundPrivate* priv = self->priv;

    meta_verbose("%s: x %d, y %d\n", __func__, x, y);
    /*fprintf(stderr, "%s: x %d, y %d\n", __func__, x, y);*/

    const guchar* raw_data = gtk_selection_data_get_data(data);
    if (raw_data) {
        gpointer p = (gpointer)atol(raw_data);
        DeepinShadowWorkspace* dsw_dragging = DEEPIN_SHADOW_WORKSPACE(p);
        deepin_wm_background_request_workspace_drop_operation(self, dsw_dragging);

        gtk_drag_finish(context, TRUE, FALSE, time);

    } else 
        gtk_drag_finish(context, FALSE, FALSE, time);
}

static gboolean on_deepin_wm_background_drag_motion(GtkWidget* widget, GdkDragContext* context,
               gint x, gint y, guint time, gpointer user_data)
{
    DeepinWMBackground* self = DEEPIN_WM_BACKGROUND(widget);
    _handle_drag_motion(widget, context, x, y, time, self);
    return TRUE;
}
    
static gboolean on_deepin_wm_background_drag_drop(GtkWidget* widget, GdkDragContext* context,
               gint x, gint y, guint time, gpointer user_data)
{
    DeepinWMBackground* self = DEEPIN_WM_BACKGROUND(widget);
    DeepinWMBackgroundPrivate* priv = self->priv;

    drag_to_remove_box = (GdkRectangle){0, 0, -1, -1};

    meta_verbose("%s\n", __func__);
    return FALSE;
}

void deepin_wm_background_setup(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    GdkRectangle geom;

    gint monitor_index = gdk_screen_get_primary_monitor(priv->gscreen);
    gdk_screen_get_monitor_geometry(priv->gscreen, monitor_index, &geom);
            
    GtkWidget* bin = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(self), bin);

    priv->fixed = deepin_fixed_new();
    gtk_widget_set_size_request(priv->fixed, geom.width, geom.height);
    gtk_fixed_put(GTK_FIXED(bin), priv->fixed, geom.x, geom.y);

    priv->top_offset = (int)(geom.height * FLOW_CLONE_TOP_OFFSET_PERCENT);
    priv->bottom_offset = (int)(geom.height * HORIZONTAL_OFFSET_PERCENT);
    float scale = (float)(geom.height - priv->top_offset - priv->bottom_offset) / geom.height;

    priv->scale = scale;
    priv->width = geom.width * scale;
    priv->height = geom.height * scale;

    // calculate monitor width height ratio
    float monitor_whr = (float)geom.height / geom.width;
    priv->thumb_width = geom.width * WORKSPACE_WIDTH_PERCENT;
    priv->thumb_height = priv->thumb_width * monitor_whr;

    GList *l = priv->screen->workspaces;
    gint current = 0;

    while (l) {
        MetaWorkspace* ws = (MetaWorkspace*)l->data;
        {
            DeepinShadowWorkspace* dsw = 
                (DeepinShadowWorkspace*)deepin_shadow_workspace_new();
            deepin_shadow_workspace_set_enable_drag(dsw, TRUE);
            deepin_shadow_workspace_set_scale(dsw, scale);
            deepin_shadow_workspace_populate(dsw, ws);

            if (ws == priv->screen->active_workspace) {
                current = g_list_index(priv->screen->workspaces, l->data);
                priv->active_workspace = dsw;
                deepin_shadow_workspace_set_current(dsw, TRUE);
            }

            g_object_connect(G_OBJECT(dsw), 
                    "signal::button-release-event", on_workspace_released, self,
                    NULL);

            priv->workspaces = g_list_append(priv->workspaces, dsw);
        }

        /* for top workspace thumbnail */
        {
            DeepinShadowWorkspace* dsw = 
                (DeepinShadowWorkspace*)deepin_shadow_workspace_new();
            deepin_shadow_workspace_set_enable_drag(dsw, TRUE);
            deepin_shadow_workspace_set_thumb_mode(dsw, TRUE);
            deepin_shadow_workspace_set_scale(dsw, WORKSPACE_WIDTH_PERCENT);
            deepin_shadow_workspace_populate(dsw, ws);

            if (ws == priv->screen->active_workspace) {
                deepin_shadow_workspace_set_current(dsw, TRUE);
            }

            g_object_connect(G_OBJECT(dsw), 
                    "signal::enter-notify-event", on_workspace_thumb_entered, self,
                    "signal::leave-notify-event", on_workspace_thumb_leaved, self,
                    "signal::button-release-event", on_workspace_thumb_released, self,
                    "signal::drag-motion", on_workspace_thumb_drag_motion, self, 
                    NULL);

            priv->workspace_thumbs = g_list_append(priv->workspace_thumbs, dsw);
        }

        l = l->next;
    }

    g_object_connect(G_OBJECT(self), "signal::scroll-event", on_background_scrolled, NULL, NULL);

    if (_show_adder(priv->screen) && !priv->adder) {
        _create_adder(self);
    }

    gint i = 0, pad = FLOW_CLONE_DISTANCE_PERCENT * geom.width;
    l = priv->workspaces;
    while (l) {
        gint x = (geom.width - priv->width) / 2 +  (i - current) * (priv->width + pad);
        deepin_fixed_put(DEEPIN_FIXED(priv->fixed), (GtkWidget*)l->data,
                x + priv->width/2, priv->top_offset + priv->height/2);

        i++;
        l = l->next;
    }

    i = 0;
    l = priv->workspace_thumbs;
    int thumb_spacing = geom.width * SPACING_PERCENT;
    
    gint count = g_list_length(priv->workspace_thumbs) + _show_adder(priv->screen);
    int thumb_y = (int)(geom.height * HORIZONTAL_OFFSET_PERCENT);
    int thumb_x = (geom.width - count * (priv->thumb_width + thumb_spacing))/2;

    while (l) {
        gtk_widget_show((GtkWidget*)l->data);

        int x = thumb_x + i * (priv->thumb_width + thumb_spacing);
        deepin_fixed_put(DEEPIN_FIXED(priv->fixed), (GtkWidget*)l->data,
                x + priv->thumb_width/2,
                thumb_y + priv->thumb_height/2);

        i++;
        l = l->next;
    }

    if (priv->adder) {
        int x = thumb_x + i * (priv->thumb_width + thumb_spacing);
        deepin_fixed_put(DEEPIN_FIXED(priv->fixed), (GtkWidget*)priv->adder,
                x + priv->thumb_width/2,
                thumb_y + priv->thumb_height/2);
    }

    _create_close_button(self);

    static GtkTargetEntry targets[] = {
        {(char*)"workspace", GTK_TARGET_OTHER_WIDGET, DRAG_TARGET_WORKSPACE},
    };

    gtk_drag_dest_set(GTK_WIDGET(self),
            GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
            targets, 1, GDK_ACTION_COPY);

    g_object_connect(G_OBJECT(self),
            "signal::drag-data-received", on_deepin_wm_background_drag_data_received, NULL, 
            "signal::drag-drop", on_deepin_wm_background_drag_drop, NULL, 
            "signal::drag-motion", on_deepin_wm_background_drag_motion, NULL, 
            NULL);

    gtk_window_move(GTK_WINDOW(self), geom.x, geom.y);
}

static gboolean on_deepin_wm_background_event(DeepinWMBackground* self,
        GdkEvent* ev, gpointer data)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    switch(ev->type) {
        case GDK_BUTTON_PRESS:
            break;
        case GDK_BUTTON_RELEASE:
            break;

        case GDK_KEY_PRESS:
        case GDK_KEY_RELEASE:
            return TRUE;

        default: break;
    }

    return FALSE;
}

GtkWidget* deepin_wm_background_new(MetaScreen* screen)
{
    GtkWidget* widget = (GtkWidget*)g_object_new(DEEPIN_TYPE_WM_BACKGROUND,
            "type", GTK_WINDOW_POPUP, NULL);
    deepin_setup_style_class(widget, "deepin-window-manager"); 

    DeepinWMBackground* self = DEEPIN_WM_BACKGROUND(widget);

    self->priv->screen = screen;
    MetaDisplay* display = meta_get_display();
    GdkDisplay* gdisplay = gdk_x11_lookup_xdisplay(display->xdisplay);
    self->priv->gscreen = gdk_display_get_default_screen(gdisplay);

    gtk_widget_realize (widget);

    gint w = gdk_screen_get_width(self->priv->gscreen),
         h = gdk_screen_get_height(self->priv->gscreen);
    gtk_window_set_default_size(GTK_WINDOW(widget), w, h);

    gtk_window_set_keep_above(GTK_WINDOW(widget), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(widget), FALSE);

    g_object_connect(G_OBJECT(widget),
            "signal::event", on_deepin_wm_background_event, NULL,
            NULL);

    return widget;
}

static void _delete_workspace(DeepinWMBackground* self,
        DeepinShadowWorkspace* ws)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    if (meta_screen_get_n_workspaces(priv->screen) <= 1) {
        return;
    }

    meta_verbose("%s\n", __func__);
    gboolean need_switch_active = FALSE;
    if (priv->workspace_changing) return;

    priv->workspace_changing = TRUE;

    _hide_close_button(self);

    MetaWorkspace* workspace = deepin_shadow_workspace_get_workspace(ws);
    DeepinShadowWorkspace* ws_thumb = _find_workspace(priv->workspace_thumbs, workspace);
    g_assert(ws_thumb != NULL);
    if (priv->hover_ws) priv->hover_ws = NULL;

    need_switch_active = (ws == priv->active_workspace);

    priv->workspaces = g_list_remove(priv->workspaces, ws);
    priv->workspace_thumbs = g_list_remove(priv->workspace_thumbs, ws_thumb);

    gtk_container_remove(GTK_CONTAINER(priv->fixed), (GtkWidget*)ws_thumb);
    gtk_container_remove(GTK_CONTAINER(priv->fixed), (GtkWidget*)ws);

    meta_screen_remove_workspace(priv->screen, workspace);

    if (need_switch_active) {
        MetaWorkspace* next_ws = priv->screen->active_workspace;
        priv->active_workspace = _find_workspace(priv->workspaces, next_ws);
        DeepinShadowWorkspace* current_thumb = _find_workspace(
                priv->workspace_thumbs,
                deepin_shadow_workspace_get_workspace(priv->active_workspace));

        deepin_shadow_workspace_set_current(priv->active_workspace, TRUE);
        deepin_shadow_workspace_set_current(current_thumb, TRUE);

        MetaWindow* focus_window = meta_stack_get_default_focus_window(
                priv->screen->stack, next_ws, NULL);
        meta_window_focus(focus_window, gtk_get_current_event_time());
        meta_window_raise(focus_window);
    }

    relayout(self);

    if (priv->hover_ws && _show_closer(priv->screen)) {
        _move_close_button_for(self, priv->hover_ws);
        gtk_widget_set_opacity(priv->close_button, 1.0);
    }

    priv->workspace_changing = FALSE;
}

static void _create_workspace(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    meta_verbose("%s\n", __func__);
    if (meta_screen_get_n_workspaces(priv->screen) >= MAX_WORKSPACE_NUM) return;

    if (priv->workspace_changing) return;
    priv->workspace_changing = TRUE;

    GdkRectangle geom;
    gint monitor_index = gdk_screen_get_primary_monitor(priv->gscreen);
    gdk_screen_get_monitor_geometry(priv->gscreen, monitor_index, &geom);

    MetaWorkspace* new_ws = meta_screen_new_workspace(priv->screen);
    gint i = g_list_length(priv->workspaces)-1,
         current = g_list_index(priv->workspaces, priv->active_workspace),
         pad = FLOW_CLONE_DISTANCE_PERCENT * geom.width;

    {
        DeepinShadowWorkspace* dsw = 
            (DeepinShadowWorkspace*)deepin_shadow_workspace_new();
        deepin_shadow_workspace_set_enable_drag(dsw, TRUE);
        deepin_shadow_workspace_set_scale(dsw, priv->scale);
        deepin_shadow_workspace_populate(dsw, new_ws);
        gtk_widget_show(GTK_WIDGET(dsw));

        priv->workspaces = g_list_append(priv->workspaces, dsw);

        gint x = (geom.width - priv->width) / 2 +  (i - current) * (priv->width + pad);
        deepin_fixed_put(DEEPIN_FIXED(priv->fixed), (GtkWidget*)dsw,
                x + priv->width/2, priv->top_offset + priv->height/2);
    }

    /* for top workspace thumbnail */
    {
        DeepinShadowWorkspace* dsw = 
            (DeepinShadowWorkspace*)deepin_shadow_workspace_new();
        deepin_shadow_workspace_set_enable_drag(dsw, TRUE);
        deepin_shadow_workspace_set_thumb_mode(dsw, TRUE);
        deepin_shadow_workspace_set_scale(dsw, WORKSPACE_WIDTH_PERCENT);
        deepin_shadow_workspace_populate(dsw, new_ws);

        g_object_connect(G_OBJECT(dsw), 
                "signal::enter-notify-event", on_workspace_thumb_entered, self,
                "signal::leave-notify-event", on_workspace_thumb_leaved, self,
                "signal::button-release-event", on_workspace_thumb_released, self,
                "signal::drag-motion", on_workspace_thumb_drag_motion, self, 
                NULL);

        gtk_widget_show(GTK_WIDGET(dsw));
        priv->workspace_thumbs = g_list_append(priv->workspace_thumbs, dsw);

        int thumb_spacing = geom.width * SPACING_PERCENT;
        gint count = g_list_length(priv->workspace_thumbs) + 1;
        int thumb_y = (int)(geom.height * HORIZONTAL_OFFSET_PERCENT);
        int thumb_x = (geom.width - count * (priv->thumb_width + thumb_spacing))/2;

        int x = thumb_x + i * (priv->thumb_width + thumb_spacing);
        deepin_fixed_put(DEEPIN_FIXED(priv->fixed), (GtkWidget*)dsw,
                x + priv->thumb_width/2, 
                thumb_y + priv->thumb_height/2);
    }

    priv->hover_ws = NULL;
    relayout(self);
    
    if (priv->hover_ws && _show_closer(priv->screen)) {
        _move_close_button_for(self, priv->hover_ws);
        gtk_widget_set_opacity(priv->close_button, 1.0);
    }

    meta_workspace_activate(new_ws, gtk_get_current_event_time());
    deepin_wm_background_switch_workspace(self, new_ws);

    priv->workspace_changing = FALSE;
}

static void _handle_workspace_creation(DeepinWMBackground* self,
        XIDeviceEvent* event, KeySym keysym, MetaKeyBindingAction action)
{
    _create_workspace(self);
}

static void _handle_workspace_deletion(DeepinWMBackground* self,
        XIDeviceEvent* event, KeySym keysym, MetaKeyBindingAction action)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    static gint64 last_time = 0;

    g_assert(priv->active_workspace);
    if (!last_time && g_get_monotonic_time() - last_time < 800) {
        return;
    }
    last_time = g_get_monotonic_time();
    _delete_workspace(self, priv->active_workspace);
}

static void _handle_workspace_goto(DeepinWMBackground* self,
        XIDeviceEvent* event, KeySym keysym, MetaKeyBindingAction action)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    if (priv->workspace_changing) return;

    gint index = keysym - XK_1;
    MetaWorkspace* current = deepin_shadow_workspace_get_workspace(priv->active_workspace); 
    MetaWorkspace* next = meta_screen_get_workspace_by_index(current->screen, index);
    if (next) {
        if (next == current) {
            //bouncing
            return;
        }

        deepin_wm_background_switch_workspace(self, next);
    }
}

static void _handle_workspace_switch(DeepinWMBackground* self, XIDeviceEvent* event,
        KeySym keysym, MetaKeyBindingAction action)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    if (priv->workspace_changing) return;

    MetaMotionDirection dir = keysym == XK_Left ? META_MOTION_LEFT:META_MOTION_RIGHT;
    MetaWorkspace* current = deepin_shadow_workspace_get_workspace(priv->active_workspace); 
    MetaWorkspace* next = meta_workspace_get_neighbor(current, dir);
    if (next) {
        if (next == current) {
            //bouncing
            return;
        }

        deepin_wm_background_switch_workspace(self, next);
    }
}

static struct wm_event_dispatch_ 
{
    KeySym trigger;
    void (*handler)(DeepinWMBackground* self, XIDeviceEvent* event,
            KeySym keysym, MetaKeyBindingAction action);

} dispatcher[] = {
    {XK_Left,           _handle_workspace_switch},
    {XK_Right,          _handle_workspace_switch},
    {XK_plus,           _handle_workspace_creation},
    {XK_equal,          _handle_workspace_creation},
    {XK_minus,          _handle_workspace_deletion},
    {XK_underscore,     _handle_workspace_deletion},
    {XK_1,             _handle_workspace_goto},
    {XK_2,             _handle_workspace_goto},
    {XK_3,             _handle_workspace_goto},
    {XK_4,             _handle_workspace_goto},
    {XK_5,             _handle_workspace_goto},
    {XK_6,             _handle_workspace_goto},
    {XK_7,             _handle_workspace_goto},
};

void deepin_wm_background_handle_event(DeepinWMBackground* self, XIDeviceEvent* event, 
        KeySym keysym, MetaKeyBindingAction action)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    for (int i = 0, len = G_N_ELEMENTS(dispatcher); i < len; i++) {
        if (dispatcher[i].trigger == keysym) {
            dispatcher[i].handler(self, event, keysym, action);
            return;
        }
    }

    /* pass through to active workspace */
    deepin_shadow_workspace_handle_event(priv->active_workspace,
            event, keysym, action);
}

