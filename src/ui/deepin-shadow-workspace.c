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

#include "../core/workspace.h"
#include "boxes.h"
#include "deepin-cloned-widget.h"
#include "compositor.h"
#include "deepin-design.h"
#include "deepin-ease.h"
#include "deepin-shadow-workspace.h"
#include "deepin-background-cache.h"
#include "deepin-message-hub.h"

#define SET_STATE(w, state)  \
    gtk_style_context_set_state(gtk_widget_get_style_context(GTK_WIDGET(w)), (state))

/* TODO: handle live window add/remove events */

struct _DeepinShadowWorkspacePrivate
{
    gint disposed: 1;
    gint selected: 1; 
    gint thumb_mode: 1; /* show name and no presentation */
    gint ready: 1; /* if dynamic, this is set after presentation finished, 
                      else, set when window placements are done */
    gint draggable: 1;
    gint dragging: 1;
    gint in_drag: 1; /* determine if mouse drag starts */

    gint drag_start_x, drag_start_y;

    gint primary; /* primary monitor # */
    GdkRectangle mon_geom; /* primary monitor size */

    gint fixed_width, fixed_height;
    gdouble scale; 

    GPtrArray* clones;
    MetaDeepinClonedWidget* hovered_clone;
    MetaWorkspace* workspace;

    cairo_surface_t *background;

    int placement_count;

    GdkWindow* event_window;

    GtkWidget* close_button; /* for focused clone */
    MetaDeepinClonedWidget* window_need_focused;

    void (*close_fnished)(GtkWidget*);
    gpointer close_fnished_data;

    guint idle_id;
};

typedef struct _ClonedPrivateInfo
{
    gdouble init_scale; /* init scale when doing placement or in thumb mode */
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

static void _hide_close_button(DeepinShadowWorkspace* self)
{
    if (self->priv->close_button) {
        gtk_widget_set_opacity(self->priv->close_button, 0.0);
        deepin_fixed_move(DEEPIN_FIXED(self), self->priv->close_button,
                -100, -100,
                FALSE);
    }
}

static void _move_close_button_for(DeepinShadowWorkspace* self,
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

static gboolean on_idle_finish_close(DeepinShadowWorkspace* self)
{
    if (self->priv->close_fnished) {
        self->priv->close_fnished(self->priv->close_fnished_data);
    }
    return G_SOURCE_REMOVE;
}

static void on_window_placed(MetaDeepinClonedWidget* clone, gpointer data)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;
    DeepinShadowWorkspacePrivate* priv = self->priv;

    GtkRequisition req;
    gtk_widget_get_preferred_size(GTK_WIDGET(clone), &req, NULL);

    ClonedPrivateInfo* info = clone_get_info(clone);
    req.width *= info->init_scale;
    req.height *= info->init_scale;

    meta_verbose("%s: scale down to %f, %d, %d\n", __func__,
            info->init_scale, req.width, req.height);

    meta_deepin_cloned_widget_set_size(clone, req.width, req.height);

    if (++priv->placement_count >= priv->clones->len) {
        priv->ready = TRUE;
        priv->placement_count = 0;

        if (priv->hovered_clone) {
            _move_close_button_for(self, priv->hovered_clone);
            gtk_widget_set_opacity(priv->close_button, 1.0);
        }
    }
}

G_DEFINE_TYPE (DeepinShadowWorkspace, deepin_shadow_workspace, DEEPIN_TYPE_FIXED);

static void place_window(DeepinShadowWorkspace* self,
        MetaDeepinClonedWidget* clone, MetaRectangle rect)
{
    GtkRequisition req;
    gtk_widget_get_preferred_size(GTK_WIDGET(clone), &req, NULL);

    float fscale = (float)rect.width / req.width;
    ClonedPrivateInfo* info = clone_get_info(clone);
    info->init_scale = fscale;

    deepin_fixed_move(DEEPIN_FIXED(self), GTK_WIDGET(clone),
            rect.x + req.width * fscale /2, rect.y + req.height * fscale /2,
            FALSE);

    meta_deepin_cloned_widget_set_scale(clone, fscale, fscale);
    on_window_placed(clone, self);
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

static void grid_placement ( DeepinShadowWorkspace* self, 
        MetaRectangle area, gboolean closest)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    GPtrArray* clones = priv->clones;
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

static void natural_placement (DeepinShadowWorkspace* self, MetaRectangle area)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    GPtrArray* clones = priv->clones;
    if (!clones || clones->len == 0) return;

    MetaRectangle bounds = {area.x, area.y, area.width, area.height};

    int direction = 0;
    int* directions = (int*)g_malloc(sizeof(int)*clones->len);
    MetaRectangle* rects = (MetaRectangle*)g_malloc(sizeof(MetaRectangle)*clones->len);

    for (int i = 0; i < clones->len; i++) {
        // save rectangles into 4-dimensional arrays representing two corners of the rectangular: [left_x, top_y, right_x, bottom_y]
        MetaRectangle rect;
        MetaDeepinClonedWidget* clone = g_ptr_array_index(clones, i);
        MetaWindow* win = meta_deepin_cloned_widget_get_window(clone);

        meta_window_get_input_rect(win, &rect);
        rect = rect_adjusted(rect, -GAPS, -GAPS, GAPS, GAPS);
        rects[i] = rect;
        /*meta_verbose("%s: frame: %d,%d,%d,%d", __func__, rect.x, rect.y, rect.width, rect.height);*/

        meta_rectangle_union(&bounds, &rect, &bounds);

        // This is used when the window is on the edge of the screen to try to use as much screen real estate as possible.
        directions[i] = direction;
        direction++;
        if (direction == 4)
            direction = 0;
    }

    int loop_counter = 0;
    gboolean overlap = FALSE;
    do {
        overlap = FALSE;
        for (int i = 0; i < clones->len; i++) {
            for (int j = 0; j < clones->len; j++) {
                if (i == j)
                    continue;

                MetaRectangle rect = rects[i];
                MetaRectangle comp = rects[j];

                if (!meta_rectangle_overlap(&rect, &comp))
                    continue;

                loop_counter ++;
                overlap = TRUE;

                // Determine pushing direction
                GdkPoint i_center = rect_center (rect);
                GdkPoint j_center = rect_center (comp);
                GdkPoint diff = {j_center.x - i_center.x, j_center.y - i_center.y};

                // Prevent dividing by zero and non-movement
                if (diff.x == 0 && diff.y == 0)
                    diff.x = 1;

                // Approximate a vector of between 10px and 20px in magnitude in the same direction
                float length = sqrtf (diff.x * diff.x + diff.y * diff.y);
                diff.x = (int)floorf (diff.x * ACCURACY / length);
                diff.y = (int)floorf (diff.y * ACCURACY / length);
                // Move both windows apart
                rect.x += -diff.x;
                rect.y += -diff.y;
                comp.x += diff.x;
                comp.y += diff.y;

                // Try to keep the bounding rect the same aspect as the screen so that more
                // screen real estate is utilised. We do this by splitting the screen into nine
                // equal sections, if the window center is in any of the corner sections pull the
                // window towards the outer corner. If it is in any of the other edge sections
                // alternate between each corner on that edge. We don't want to determine it
                // randomly as it will not produce consistant locations when using the filter.
                // Only move one window so we don't cause large amounts of unnecessary zooming
                // in some situations. We need to do this even when expanding later just in case
                // all windows are the same size.
                // (We are using an old bounding rect for this, hopefully it doesn't matter)
                int x_section = (int)roundf ((rect.x - bounds.x) / (bounds.width / 3.0f));
                int y_section = (int)roundf ((comp.y - bounds.y) / (bounds.height / 3.0f));

                i_center = rect_center (rect);
                diff.x = 0;
                diff.y = 0;
                if (x_section != 1 || y_section != 1) { // Remove this if you want the center to pull as well
                    if (x_section == 1)
                        x_section = (directions[i] / 2 == 1 ? 2 : 0);
                    if (y_section == 1)
                        y_section = (directions[i] % 2 == 1 ? 2 : 0);
                }
                if (x_section == 0 && y_section == 0) {
                    diff.x = bounds.x - i_center.x;
                    diff.y = bounds.y - i_center.y;
                }
                if (x_section == 2 && y_section == 0) {
                    diff.x = bounds.x + bounds.width - i_center.x;
                    diff.y = bounds.y - i_center.y;
                }
                if (x_section == 2 && y_section == 2) {
                    diff.x = bounds.x + bounds.width - i_center.x;
                    diff.y = bounds.y + bounds.height - i_center.y;
                }
                if (x_section == 0 && y_section == 2) {
                    diff.x = bounds.x - i_center.x;
                    diff.y = bounds.y + bounds.height - i_center.y;
                }
                if (diff.x != 0 || diff.y != 0) {
                    length = sqrtf (diff.x * diff.x + diff.y * diff.y);
                    diff.x *= (int)floorf (ACCURACY / length / 2.0f);
                    diff.y *= (int)floorf (ACCURACY / length / 2.0f);
                    rect.x += diff.x;
                    rect.y += diff.y;
                }

                // Update bounding rect
                meta_rectangle_union(&bounds, &rect, &bounds);
                meta_rectangle_union(&bounds, &comp, &bounds);

                //we took copies from the rects from our list so we need to reassign them
                rects[i] = rect;
                rects[j] = comp;
            }
        }
    } while (overlap && loop_counter < MAX_TRANSLATIONS);

    // Work out scaling by getting the most top-left and most bottom-right window coords.
    float scale = fminf (fminf (area.width / (float)bounds.width,
                area.height / (float)bounds.height), 1.0f);

    // Make bounding rect fill the screen size for later steps
    bounds.x = (int)floorf (bounds.x - (area.width - bounds.width * scale) / 2);
    bounds.y = (int)floorf (bounds.y - (area.height - bounds.height * scale) / 2);
    bounds.width = (int)floorf (area.width / scale);
    bounds.height = (int)floorf (area.height / scale);

    // Move all windows back onto the screen and set their scale
    int index = 0;
    for (; index < clones->len; index++) {
        MetaRectangle rect = rects[index];
        rects[index] = (MetaRectangle){
            (int)floorf ((rect.x - bounds.x) * scale + area.x),
                (int)floorf ((rect.y - bounds.y) * scale + area.y),
                (int)floorf (rect.width * scale),
                (int)floorf (rect.height * scale)
        };
    }

    // fill gaps by enlarging windows
    gboolean moved = FALSE;
    MetaRectangle border = area;
    do {
        moved = FALSE;

        index = 0;
        for (; index < clones->len; index++) {
            MetaRectangle rect = rects[index];

            int width_diff = ACCURACY;
            int height_diff = (int)floorf ((((rect.width + width_diff) - rect.height) /
                        (float)rect.width) * rect.height);
            int x_diff = width_diff / 2;
            int y_diff = height_diff / 2;

            //top right
            MetaRectangle old = rect;
            rect = (MetaRectangle){ rect.x + x_diff, rect.y - y_diff - height_diff, rect.width + width_diff, rect.height + width_diff };
            if (rect_is_overlapping_any (rect, rects, clones->len, border))
                rect = old;
            else moved = TRUE;

            //bottom right
            old = rect;
            rect = (MetaRectangle){rect.x + x_diff, rect.y + y_diff, rect.width + width_diff, rect.height + width_diff};
            if (rect_is_overlapping_any (rect, rects, clones->len, border))
                rect = old;
            else moved = TRUE;

            //bottom left
            old = rect;
            rect = (MetaRectangle){rect.x - x_diff, rect.y + y_diff, rect.width + width_diff, rect.height + width_diff};
            if (rect_is_overlapping_any (rect, rects, clones->len, border))
                rect = old;
            else moved = TRUE;

            //top left
            old = rect;
            rect = (MetaRectangle){rect.x - x_diff, rect.y - y_diff - height_diff, rect.width + width_diff, rect.height + width_diff};
            if (rect_is_overlapping_any (rect, rects, clones->len, border))
                rect = old;
            else moved = TRUE;

            rects[index] = rect;
        }
    } while (moved);

    index = 0;
    for (; index < clones->len; index++) {
        MetaRectangle rect = rects[index];

        MetaDeepinClonedWidget* clone = (MetaDeepinClonedWidget*)g_ptr_array_index(clones, index);
        MetaWindow* window = meta_deepin_cloned_widget_get_window(clone);

        MetaRectangle window_rect;
        meta_window_get_input_rect(window, &window_rect);


        rect = rect_adjusted(rect, GAPS, GAPS, -GAPS, -GAPS);
        scale = rect.width / (float)window_rect.width;

        if (scale > 2.0 || (scale > 1.0 && (window_rect.width > 300 || window_rect.height > 300))) {
            scale = (window_rect.width > 300 || window_rect.height > 300) ? 1.0f : 2.0f;
            rect = (MetaRectangle){rect_center (rect).x - (int)floorf (window_rect.width * scale) / 2,
                rect_center (rect).y - (int)floorf (window_rect.height * scale) / 2,
                (int)floorf (window_rect.width * scale),
                (int)floorf (window_rect.height * scale)};
        }

        place_window(self, clone, rect);
    }

    g_free(directions);
    g_free(rects);
}

static int padding_top  = 12;
static int padding_left  = 12;
static int padding_right  = 12;
static int padding_bottom  = 12;
static void calculate_places(DeepinShadowWorkspace* self)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;

    if (priv->clones && priv->clones->len) {

        MetaRectangle area = {
            padding_top, padding_left, 
            priv->fixed_width - padding_left - padding_right,
            priv->fixed_height - padding_top - padding_bottom
        };

        /*natural_placement(self, area);*/
        grid_placement(self, area, FALSE);

    } else {
        priv->ready = TRUE; // no window at all
    }
}

static gboolean on_idle(DeepinShadowWorkspace* self)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    if (priv->disposed) return G_SOURCE_REMOVE;

    if (priv->thumb_mode) {
        priv->ready = TRUE;

    } else {
        if (priv->close_button) {
            deepin_fixed_raise(DEEPIN_FIXED(self), priv->close_button);
            _hide_close_button(self);
        }
        calculate_places(self);
    }

    priv->idle_id = 0;
    return G_SOURCE_REMOVE;
}

static void _remove_cloned_widget(DeepinShadowWorkspace* self, 
        MetaDeepinClonedWidget* clone)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    
    if (priv->window_need_focused == clone) {
        deepin_shadow_workspace_focus_next(self, FALSE);
    }

    if (priv->hovered_clone == clone) {
        priv->hovered_clone = NULL;
        _hide_close_button(self);
    }

    MetaWindow* window = meta_deepin_cloned_widget_get_window(clone);
    meta_verbose("%s remove clone for %s\n", __func__, window->desc);
    g_ptr_array_remove(priv->clones, clone);
    gtk_container_remove(GTK_CONTAINER(self), (GtkWidget*)clone);


    if (priv->ready) on_idle(self);
}

static void on_window_removed(DeepinMessageHub* hub, MetaWindow* window, 
        gpointer data)
{
    DeepinShadowWorkspace* self = DEEPIN_SHADOW_WORKSPACE(data);
    DeepinShadowWorkspacePrivate* priv = self->priv;
    if (!priv->clones) return;

    for (gint i = 0; i < priv->clones->len; i++) {
        MetaDeepinClonedWidget* clone = g_ptr_array_index(priv->clones, i);
        if (meta_deepin_cloned_widget_get_window(clone) == window) {
            _remove_cloned_widget(self, clone);
            break;
        }
    }
}

static void on_desktop_changed(DeepinMessageHub* hub, gpointer data)
{
    DeepinShadowWorkspace* self = DEEPIN_SHADOW_WORKSPACE(data);
    DeepinShadowWorkspacePrivate* priv = self->priv;

    if (priv->background) {
        g_clear_pointer(&priv->background, cairo_surface_destroy);
    }

    int index = meta_workspace_index(priv->workspace);
    priv->background = deepin_background_cache_get_surface(
            priv->primary, index, priv->scale);
    cairo_surface_reference(priv->background);
    
    if (gtk_widget_is_visible(self)) {
        gtk_widget_queue_draw(self);
    }
}

static void deepin_shadow_workspace_init (DeepinShadowWorkspace *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_SHADOW_WORKSPACE, DeepinShadowWorkspacePrivate);
    memset(self->priv, 0, sizeof *self->priv);

    self->priv->scale = 1.0;
    gtk_widget_set_sensitive(GTK_WIDGET(self), TRUE);
    gtk_widget_set_app_paintable(GTK_WIDGET(self), TRUE);
    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);
}

static void deepin_shadow_workspace_dispose (GObject *object)
{
    DeepinShadowWorkspace* self = DEEPIN_SHADOW_WORKSPACE(object);
    DeepinShadowWorkspacePrivate* priv = self->priv;
    if (priv->disposed) return;

    priv->disposed = TRUE;
    g_signal_handlers_disconnect_by_data(G_OBJECT(deepin_message_hub_get()), self);
    g_signal_handlers_disconnect_by_data(self, NULL);
    g_idle_remove_by_data(self);

    if (priv->idle_id) {
        g_source_remove(priv->idle_id);
        priv->idle_id = 0;
    }

    if (priv->clones) {
        g_ptr_array_free(priv->clones, FALSE);
        priv->clones = NULL;
    }

    G_OBJECT_CLASS (deepin_shadow_workspace_parent_class)->dispose (object);
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

static void deepin_shadow_workspace_get_preferred_width (GtkWidget *widget,
        gint *minimum, gint *natural)
{
    DeepinShadowWorkspace *self = DEEPIN_SHADOW_WORKSPACE (widget);

    *minimum = *natural = self->priv->fixed_width;
}

static void deepin_shadow_workspace_get_preferred_height (GtkWidget *widget,
        gint *minimum, gint *natural)
{
    DeepinShadowWorkspace *self = DEEPIN_SHADOW_WORKSPACE (widget);

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

static gboolean deepin_shadow_workspace_draw (GtkWidget *widget,
        cairo_t *cr)
{
    DeepinShadowWorkspace *fixed = DEEPIN_SHADOW_WORKSPACE (widget);
    DeepinShadowWorkspacePrivate *priv = fixed->priv;

    GdkRectangle r;
    gdk_cairo_get_clip_rectangle(cr, &r);

    GtkAllocation req;
    gtk_widget_get_allocation(widget, &req);

    GtkStyleContext* context = gtk_widget_get_style_context(widget);

    cairo_save(cr);

    if (priv->thumb_mode) {

        /* FIXME: why can not get borders */
        /*GtkBorder borders;*/
        /*_style_get_borders(context, &borders);*/

        int b = 2;
        gtk_render_background(context, cr, -b, -b, req.width+2*b, 
                priv->fixed_height+2*b);

        _draw_round_box(cr, req.width, priv->fixed_height, 4.0);
        cairo_clip(cr);

    } else {
        gtk_render_background(context, cr, 0, 0, req.width, req.height);
    }

    if (priv->background != NULL) {
        cairo_set_source_surface(cr, priv->background, 0, 0);
    }

    cairo_paint(cr);

    GTK_WIDGET_CLASS(deepin_shadow_workspace_parent_class)->draw(widget, cr);
    cairo_restore(cr);
    
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

static void deepin_shadow_workspace_size_allocate(GtkWidget* widget, 
        GtkAllocation* allocation)
{
    GTK_WIDGET_CLASS(deepin_shadow_workspace_parent_class)->size_allocate(
            widget, allocation);

    DeepinShadowWorkspace *self = DEEPIN_SHADOW_WORKSPACE (widget);
    if (gtk_widget_get_realized (widget))
        gdk_window_move_resize (self->priv->event_window,
                allocation->x,
                allocation->y,
                allocation->width,
                allocation->height);

    _gtk_widget_set_simple_clip(widget, NULL);
}

static void deepin_shadow_workspace_realize (GtkWidget *widget)
{
    DeepinShadowWorkspace *self = DEEPIN_SHADOW_WORKSPACE (widget);
    DeepinShadowWorkspacePrivate *priv = self->priv;
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
            GDK_POINTER_MOTION_MASK |
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

static void deepin_shadow_workspace_unrealize (GtkWidget *widget)
{
    DeepinShadowWorkspace *self = DEEPIN_SHADOW_WORKSPACE (widget);
    DeepinShadowWorkspacePrivate *priv = self->priv;

    if (priv->event_window) {
        gtk_widget_unregister_window (widget, priv->event_window);
        gdk_window_destroy (priv->event_window);
        priv->event_window = NULL;
    }

    GTK_WIDGET_CLASS (deepin_shadow_workspace_parent_class)->unrealize (widget);
}

static void deepin_shadow_workspace_map (GtkWidget *widget)
{
    DeepinShadowWorkspace *self = DEEPIN_SHADOW_WORKSPACE (widget);
    DeepinShadowWorkspacePrivate *priv = self->priv;

    GTK_WIDGET_CLASS (deepin_shadow_workspace_parent_class)->map (widget);

    if (priv->event_window)
        gdk_window_show_unraised(priv->event_window);
}

static void deepin_shadow_workspace_unmap (GtkWidget *widget)
{
    DeepinShadowWorkspace *self = DEEPIN_SHADOW_WORKSPACE (widget);
    DeepinShadowWorkspacePrivate *priv = self->priv;

    if (priv->event_window) {
        gdk_window_hide (priv->event_window);
    }

    GTK_WIDGET_CLASS (deepin_shadow_workspace_parent_class)->unmap (widget);
}

static void deepin_shadow_workspace_class_init (DeepinShadowWorkspaceClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass* widget_class = (GtkWidgetClass*) klass;

    g_type_class_add_private (klass, sizeof (DeepinShadowWorkspacePrivate));
    widget_class->get_preferred_width = deepin_shadow_workspace_get_preferred_width;
    widget_class->get_preferred_height = deepin_shadow_workspace_get_preferred_height;
    widget_class->size_allocate = deepin_shadow_workspace_size_allocate;
    widget_class->draw = deepin_shadow_workspace_draw;

    widget_class->realize = deepin_shadow_workspace_realize;
    widget_class->unrealize = deepin_shadow_workspace_unrealize;
    widget_class->map = deepin_shadow_workspace_map;
    widget_class->unmap = deepin_shadow_workspace_unmap;

    object_class->dispose = deepin_shadow_workspace_dispose;
}

// propagate from cloned
static gboolean on_deepin_cloned_widget_leaved(MetaDeepinClonedWidget* cloned,
               GdkEvent* event, gpointer data)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;
    if (!self->priv->ready) return FALSE;

    meta_verbose ("%s\n", __func__);
    if (self->priv->thumb_mode) {
        self->priv->hovered_clone = NULL;
        return FALSE;
    }

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
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;
    DeepinShadowWorkspacePrivate* priv = self->priv;

    if (!priv->ready) return FALSE;
    meta_verbose ("%s\n", __func__);

    priv->hovered_clone = cloned;

    if (!priv->thumb_mode) {
        if (priv->ready) {
            _move_close_button_for(self, cloned);
            gtk_widget_set_opacity(priv->close_button, 1.0);
        }
    }
    return TRUE;
}

static gboolean on_idle_end_grab(guint timestamp)
{
    meta_verbose ("%s\n", __func__);
    meta_display_end_grab_op(meta_get_display(), timestamp);
    return G_SOURCE_REMOVE;
}

static void close_window(DeepinShadowWorkspace *self, MetaDeepinClonedWidget *clone)
{
    meta_verbose("%s\n", __func__);
    DeepinShadowWorkspacePrivate* priv = self->priv;

    if (!priv->ready) return FALSE;

    if (priv->window_need_focused == clone) {
        deepin_shadow_workspace_focus_next(self, FALSE);
    }

    if (priv->hovered_clone == clone) {
        priv->hovered_clone = NULL;
        _hide_close_button(self);
    }

    MetaWindow* window = meta_deepin_cloned_widget_get_window(clone);
    g_ptr_array_remove(priv->clones, clone);
    gtk_container_remove(GTK_CONTAINER(self), (GtkWidget*)clone);

    meta_window_delete(window, CurrentTime);

    on_idle(self);
}


static gboolean on_deepin_cloned_widget_released(MetaDeepinClonedWidget* cloned,
               GdkEvent* event, gpointer data)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;
    DeepinShadowWorkspacePrivate *priv = self->priv; 
    meta_verbose("%s\n", __func__);
    if (!priv->ready) return FALSE;

    if (meta_deepin_cloned_widget_is_dragging(cloned)) {
        // stop here, so workspace won't get event
        return TRUE;
    }

    if (priv->thumb_mode) {
        /* pass to parent workspace */
        return FALSE;
    }

    switch (event->button.button) {
        case GDK_BUTTON_PRIMARY: {
            MetaWindow* mw = meta_deepin_cloned_widget_get_window(cloned);
            if (mw->workspace && mw->workspace != mw->screen->active_workspace) {
                meta_workspace_activate(mw->workspace, gdk_event_get_time(event));
            }
            meta_window_activate(mw, gdk_event_get_time(event));
            g_idle_add((GSourceFunc)on_idle_end_grab, GUINT_TO_POINTER(gdk_event_get_time(event)));
            break;
        }

        case 2:
            close_window(self, cloned);
            break;
    }
    return TRUE;
}

static gboolean on_deepin_cloned_widget_motion(MetaDeepinClonedWidget* cloned,
               GdkEvent* event, gpointer data)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;
    DeepinShadowWorkspacePrivate* priv = self->priv;
    if (!priv->ready) return FALSE;

    if (!priv->thumb_mode && priv->hovered_clone == NULL) {
        meta_verbose("%s\n", __func__);
        priv->hovered_clone = cloned;
        _move_close_button_for(self, priv->hovered_clone);
        gtk_widget_set_opacity(priv->close_button, 1.0);
    }

    return TRUE;
}

static void on_deepin_cloned_widget_drag_begin(GtkWidget* widget,
        GdkDragContext *context, gpointer data)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;
    DeepinShadowWorkspacePrivate* priv = self->priv;

    meta_verbose("%s\n", __func__);
    if (!priv->ready) return;

    if (!priv->thumb_mode && priv->hovered_clone == widget) {
        _hide_close_button(self);
    }
}

static void on_deepin_cloned_widget_drag_end(GtkWidget* widget,
        GdkDragContext *context, gpointer data)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;
    DeepinShadowWorkspacePrivate* priv = self->priv;

    if (!priv->ready) return;

    if (!priv->thumb_mode && priv->hovered_clone == widget) {
        GdkDisplay* display = gdk_display_get_default();
        GdkSeat* seat = gdk_display_get_default_seat(display);
        g_assert(seat != NULL);
        GdkDevice* device = gdk_seat_get_pointer(seat);

        gint x, y;
        gdk_device_get_position(device, NULL, &x, &y);

        GtkAllocation alloc;
        gtk_widget_get_allocation(widget, &alloc);

        GdkRectangle r = {alloc.x, alloc.y, alloc.width, alloc.height};
        if (x > r.x && x < r.x + r.width && y > r.y && y < r.y + r.height) {
            _move_close_button_for(self, priv->hovered_clone);
            gtk_widget_set_opacity(priv->close_button, 1.0);
            return;
        }

        priv->hovered_clone = NULL;
    }
}

static gboolean on_close_button_clicked(GtkWidget* widget,
               GdkEvent* event, gpointer data)
{
    meta_verbose("%s\n", __func__);
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;
    DeepinShadowWorkspacePrivate* priv = self->priv;

    close_window(self, priv->hovered_clone);
    return TRUE;
}

static gboolean on_close_button_leaved(GtkWidget* widget,
               GdkEvent* event, gpointer data)
{
    meta_verbose("%s\n", __func__);
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;

    if (!self->priv->hovered_clone) return FALSE;
    // redirect to hover window
    return on_deepin_cloned_widget_leaved(self->priv->hovered_clone, event, data);
}

void deepin_shadow_workspace_populate(DeepinShadowWorkspace* self,
        MetaWorkspace* ws)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    priv->workspace = ws;

    if (!priv->clones) priv->clones = g_ptr_array_new();

    GList* ls = meta_stack_list_windows(ws->screen->stack, ws);
    GList* l = ls;
    while (l) {
        MetaWindow* win = (MetaWindow*)l->data;
        if (win->type == META_WINDOW_NORMAL) {
            GtkWidget* widget = meta_deepin_cloned_widget_new(win, !priv->thumb_mode);
            g_ptr_array_add(priv->clones, widget);
            meta_deepin_cloned_widget_set_enable_drag(
                    META_DEEPIN_CLONED_WIDGET(widget), priv->draggable);

            MetaRectangle r;
            meta_window_get_outer_rect(win, &r);
            gint w = r.width * priv->scale, h = r.height * priv->scale;
            meta_deepin_cloned_widget_set_size(
                    META_DEEPIN_CLONED_WIDGET(widget), w, h);
            meta_deepin_cloned_widget_set_render_frame(
                    META_DEEPIN_CLONED_WIDGET(widget), TRUE);

            if (priv->thumb_mode) {
                if (r.x >= priv->mon_geom.x) r.x -= priv->mon_geom.x;
                if (r.y >= priv->mon_geom.y) r.y -= priv->mon_geom.y;
            }
            deepin_fixed_put(DEEPIN_FIXED(self), widget,
                    r.x * priv->scale + w/2,
                    r.y * priv->scale + h/2);

            g_object_connect(G_OBJECT(widget),
                    "signal::enter-notify-event", on_deepin_cloned_widget_entered, self,
                    "signal::leave-notify-event", on_deepin_cloned_widget_leaved, self,
                    "signal::motion-notify-event", on_deepin_cloned_widget_motion, self,
                    "signal::button-release-event", on_deepin_cloned_widget_released, self,
                    "signal::drag-begin", on_deepin_cloned_widget_drag_begin, self,
                    "signal::drag-end", on_deepin_cloned_widget_drag_end, self,
                    NULL);
        }


        l = l->next;
    }
    g_list_free(ls);

    if (!priv->thumb_mode) {
        priv->close_button = gtk_event_box_new();
        gtk_event_box_set_above_child(GTK_EVENT_BOX(priv->close_button), FALSE);
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(priv->close_button), FALSE);

        GtkWidget* image = gtk_image_new_from_file(METACITY_PKGDATADIR "/close.png");
        gtk_container_add(GTK_CONTAINER(priv->close_button), image);

        deepin_fixed_put(DEEPIN_FIXED(self), priv->close_button, 0, 0);
        gtk_widget_set_opacity(self->priv->close_button, 0.0);
        
        g_object_connect(G_OBJECT(priv->close_button), 
                "signal::leave-notify-event", on_close_button_leaved, self,
                "signal::button-release-event", on_close_button_clicked, self,
                NULL);
    }

    int index = meta_workspace_index(priv->workspace);
    priv->background = deepin_background_cache_get_surface(
            priv->primary, index, priv->scale);
    cairo_surface_reference(priv->background);

    gtk_widget_queue_resize(GTK_WIDGET(self));
}

static void on_deepin_shadow_workspace_show(DeepinShadowWorkspace* self, gpointer data)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    if (priv->idle_id) {
        g_source_remove(priv->idle_id);
        priv->idle_id = 0;
    }
    priv->idle_id = g_idle_add((GSourceFunc)on_idle, self);
}

static gboolean on_deepin_shadow_workspace_pressed(DeepinShadowWorkspace* self,
               GdkEvent* event, gpointer user_data)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    meta_verbose("%s: ws%d(%s)\n", __func__, meta_workspace_index(priv->workspace),
            priv->thumb_mode ? "thumb":"normal");

    if (!priv->ready) {
        return TRUE;
    }

    if (priv->thumb_mode) {
        /**
         * NOTE: when a dragging about to start on the cloned,
         * button pressed event sends to parent shadow ws (WTF?),
         * so I need to do this hack 
         */
        if (priv->hovered_clone) {
            return FALSE;
        }

        if (event->button.button == GDK_BUTTON_PRIMARY) {
            priv->in_drag = TRUE;
            priv->drag_start_x = event->button.x;
            priv->drag_start_y = event->button.y;
        } else {
            priv->in_drag = FALSE;
        }
    }

    if (priv->hovered_clone) {
        return TRUE;
    }

    return FALSE;
}

static gboolean on_deepin_shadow_workspace_released(DeepinShadowWorkspace* self,
               GdkEvent* event, gpointer user_data)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    meta_verbose("%s: ws%d(%s)\n", __func__, meta_workspace_index(priv->workspace),
            priv->thumb_mode ? "thumb":"normal");

    if (priv->thumb_mode && event->button.button == GDK_BUTTON_PRIMARY) {
        priv->in_drag = FALSE;
        priv->dragging = FALSE;
        return FALSE;
    }

    if (!priv->selected) return FALSE;

    if (!priv->ready || priv->hovered_clone) return TRUE;

    if (!priv->thumb_mode && !priv->dragging) {
        g_idle_add((GSourceFunc)on_idle_end_grab, gdk_event_get_time(event));
        return TRUE;
    }
    

    //TODO: do workspace change if the pressed is not current
    //bubble up to parent to determine what to do
    return FALSE;
}

static gboolean on_deepin_shadow_workspace_motion(DeepinShadowWorkspace *self,
               GdkEvent* event, gpointer data)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    if (!priv->ready) return FALSE;

    if (priv->thumb_mode) {
        if (priv->hovered_clone) {
            return FALSE;
        }

        if (priv->in_drag) {
            if (meta_screen_get_n_workspaces(priv->workspace->screen) <= 1) {
                return FALSE;
            }

            if (gtk_drag_check_threshold(self,
                        priv->drag_start_x, priv->drag_start_y,
                        event->button.x, event->button.y)) {
                meta_verbose("%s initial dragging\n", __func__);

                GtkTargetEntry targets[] = {
                    {(char*)"workspace", GTK_TARGET_OTHER_WIDGET, DRAG_TARGET_WORKSPACE},
                };

                GtkTargetList *target_list = gtk_target_list_new(targets, G_N_ELEMENTS(targets));
                GdkDragContext * ctx = gtk_drag_begin_with_coordinates (GTK_WIDGET(self),
                        target_list,
                        GDK_ACTION_COPY,
                        1,
                        event,
                        priv->drag_start_x,
                        priv->drag_start_y);
                gtk_target_list_unref (target_list);
                priv->in_drag = 0;
            }
        }
    }

    return TRUE;
}

static void on_window_change_workspace(DeepinMessageHub* hub, MetaWindow* window,
        MetaWorkspace* new_workspace, gpointer user_data)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)user_data;
    DeepinShadowWorkspacePrivate* priv = self->priv;
    meta_verbose("%s: ws %d(%s)\n", __func__, meta_workspace_index(priv->workspace),
            priv->thumb_mode ? "thumb":"normal");

    if (!priv->ready || !priv->clones) return;
    
    if (priv->workspace == new_workspace) { // dest workspace
        if (window->type != META_WINDOW_NORMAL) return;
        meta_verbose("%s: add window\n", __func__);

        //add window
        GtkWidget* widget = meta_deepin_cloned_widget_new(window, !priv->thumb_mode);
        meta_deepin_cloned_widget_set_enable_drag(
                META_DEEPIN_CLONED_WIDGET(widget), priv->draggable);
        gtk_widget_set_sensitive(widget, TRUE);
        // FIXME: honor stack order
        g_ptr_array_add(priv->clones, widget);

        MetaRectangle r;
        meta_window_get_outer_rect(window, &r);
        gint w = r.width * priv->scale, h = r.height * priv->scale;
        meta_deepin_cloned_widget_set_size(
                META_DEEPIN_CLONED_WIDGET(widget), w, h);
        meta_deepin_cloned_widget_set_render_frame(
                META_DEEPIN_CLONED_WIDGET(widget), TRUE);

        deepin_fixed_put(DEEPIN_FIXED(self), widget,
                r.x * priv->scale + w/2,
                r.y * priv->scale + h/2);

        g_object_connect(G_OBJECT(widget),
                "signal::enter-notify-event", on_deepin_cloned_widget_entered, self,
                "signal::leave-notify-event", on_deepin_cloned_widget_leaved, self,
                "signal::motion-notify-event", on_deepin_cloned_widget_motion, self,
                "signal::button-release-event", on_deepin_cloned_widget_released, self,
                "signal::drag-begin", on_deepin_cloned_widget_drag_begin, self,
                "signal::drag-end", on_deepin_cloned_widget_drag_end, self,
                NULL);
        gtk_widget_show(widget);
        if (priv->idle_id) {
            g_source_remove(priv->idle_id);
            priv->idle_id = 0;
        }
        priv->idle_id = g_idle_add((GSourceFunc)on_idle, self);

    } else if (window->workspace == priv->workspace) { // maybe source workspace
        on_window_removed(hub, window, user_data);
    }
}
    
static void on_deepin_shadow_workspace_drag_data_received(GtkWidget* widget, GdkDragContext* context,
        gint x, gint y, GtkSelectionData *data, guint info,
        guint time, gpointer user_data)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)widget;
    DeepinShadowWorkspacePrivate* priv = self->priv;
    meta_verbose("%s: x %d, y %d\n", __func__, x, y);

    const guchar* raw_data = gtk_selection_data_get_data(data);
    if (raw_data) {
        gpointer p = (gpointer)atol(raw_data);
        MetaDeepinClonedWidget* target_clone = META_DEEPIN_CLONED_WIDGET(p);
        MetaWindow* meta_win = meta_deepin_cloned_widget_get_window(target_clone);
        meta_verbose("%s: get %x\n", __func__, target_clone);
        if (meta_win->on_all_workspaces) {
            gtk_drag_finish(context, FALSE, FALSE, time);
            return;
        }

        if (!priv->ready || !priv->clones) return;

        for (gint i = 0; i < priv->clones->len; i++) {
            MetaDeepinClonedWidget* clone = g_ptr_array_index(priv->clones, i);

            if (meta_deepin_cloned_widget_get_window(clone) == meta_win) {
                meta_verbose("cancel drop on the same workspace\n");
                gtk_drag_finish(context, FALSE, FALSE, time);
                return;
            }
        }

        gtk_drag_finish(context, TRUE, FALSE, time);
        meta_window_change_workspace(meta_win, priv->workspace);

    } else 
        gtk_drag_finish(context, FALSE, FALSE, time);
}

static gboolean on_deepin_shadow_workspace_drag_drop(GtkWidget* widget, GdkDragContext* context,
               gint x, gint y, guint time, gpointer user_data)
{
    meta_verbose("%s\n", __func__);
    return FALSE;
}


static void on_deepin_shadow_workspace_drag_data_get(GtkWidget* widget, GdkDragContext* context,
        GtkSelectionData* data, guint info, guint time, gpointer user_data)
{
    static GdkAtom atom_ws = GDK_NONE;
    
    if (atom_ws == GDK_NONE) 
        atom_ws = gdk_atom_intern("deepin-workspace", FALSE);
    g_assert(atom_ws != GDK_NONE);

    gchar* raw_data = g_strdup_printf("%ld", widget);

    meta_verbose("%s: set data %x\n", __func__, widget);
    gtk_selection_data_set(data, atom_ws, 8, raw_data, strlen(raw_data));
    g_free(raw_data);
}

static void on_deepin_shadow_workspace_drag_begin(GtkWidget* widget, GdkDragContext *context,
               gpointer user_data)
{
    meta_verbose("%s\n", __func__);
    DeepinShadowWorkspacePrivate* priv = DEEPIN_SHADOW_WORKSPACE(widget)->priv;

    cairo_surface_t* dest = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            priv->fixed_width, priv->fixed_height);

    cairo_t* cr = cairo_create(dest);
    cairo_push_group(cr);
    gtk_widget_draw(widget, cr);
    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, 0.7);
    cairo_destroy(cr);

    cairo_surface_set_device_offset(dest, -priv->fixed_width/2 , -priv->fixed_height/2);
    gtk_drag_set_icon_surface(context, dest);

    gtk_widget_set_opacity(widget, 0.1);
    cairo_surface_destroy(dest);

    priv->dragging = TRUE;
}

static void on_deepin_shadow_workspace_drag_end(GtkWidget* widget, GdkDragContext *context,
               gpointer user_data)
{
    meta_verbose("%s\n", __func__);
    DeepinShadowWorkspace* self = DEEPIN_SHADOW_WORKSPACE(widget);
    gtk_widget_set_opacity(widget, 1.0);

    self->priv->dragging = FALSE;

    //HACK: drag broken the grab, need to restore here
    deepin_message_hub_drag_end();
}
    
static gboolean on_deepin_shadow_workspace_drag_failed(GtkWidget      *widget,
               GdkDragContext *context, GtkDragResult   result,
               gpointer        user_data)
{
    /* cut off default processing (fail animation), which may 
     * case a confliction when we regrab pointer later */
    return TRUE;
}

GtkWidget* deepin_shadow_workspace_new(void)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)g_object_new(
            DEEPIN_TYPE_SHADOW_WORKSPACE, NULL);

    GdkScreen* screen = gdk_screen_get_default();
    self->priv->primary = gdk_screen_get_primary_monitor(screen);
    gdk_screen_get_monitor_geometry(screen, self->priv->primary,
            &self->priv->mon_geom);

    self->priv->fixed_width = self->priv->mon_geom.width;
    self->priv->fixed_height = self->priv->mon_geom.height;
    self->priv->scale = 1.0;

    SET_STATE (self, GTK_STATE_FLAG_NORMAL);
    deepin_setup_style_class(GTK_WIDGET(self), "deepin-workspace-clone"); 
    

    deepin_shadow_workspace_set_enable_drag(self, TRUE);

    g_object_connect(G_OBJECT(self),
            "signal::show", on_deepin_shadow_workspace_show, NULL,
            "signal::button-release-event", on_deepin_shadow_workspace_released, NULL,
            "signal::button-press-event", on_deepin_shadow_workspace_pressed, NULL,
            "signal::motion-notify-event", on_deepin_shadow_workspace_motion, NULL,

            // for as drop site
            "signal::drag-data-received", on_deepin_shadow_workspace_drag_data_received, NULL, 
            "signal::drag-drop", on_deepin_shadow_workspace_drag_drop, NULL, 
            NULL);

    g_object_connect(G_OBJECT(deepin_message_hub_get()), 
            "signal::window-removed", on_window_removed, self,
            "signal::desktop-changed", on_desktop_changed, self,
            "signal::about-to-change-workspace", on_window_change_workspace, self,
            NULL);

    return (GtkWidget*)self;
}

void deepin_shadow_workspace_set_scale(DeepinShadowWorkspace* self, gdouble s)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;

    MetaDisplay* display = meta_get_display();
    priv->scale = s;
    priv->fixed_width = priv->mon_geom.width * s;
    priv->fixed_height = priv->mon_geom.height * s;

    /* FIXME: need to check if repopulate */

    gtk_widget_queue_resize(GTK_WIDGET(self));
}

gdouble deepin_shadow_workspace_get_scale(DeepinShadowWorkspace* self)
{
    return self->priv->scale;
}

void deepin_shadow_workspace_set_current(DeepinShadowWorkspace* self,
        gboolean val)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    priv->selected = val;

    GtkStateFlags state = priv->selected? GTK_STATE_FLAG_SELECTED: GTK_STATE_FLAG_NORMAL;
    
    SET_STATE (self, state);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void deepin_shadow_workspace_set_thumb_mode(DeepinShadowWorkspace* self,
        gboolean val)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    priv->thumb_mode = val;
    if (val) {
        GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(self));
        gtk_style_context_remove_class(context, "deepin-workspace-clone"); 
        deepin_setup_style_class(GTK_WIDGET(self), "deepin-workspace-thumb-clone");

        g_object_connect(G_OBJECT(self),
                // for as dragging target
                "signal::drag-data-get", on_deepin_shadow_workspace_drag_data_get, NULL,
                "signal::drag-begin", on_deepin_shadow_workspace_drag_begin, NULL,
                "signal::drag-end", on_deepin_shadow_workspace_drag_end, NULL,
                "signal::drag-failed", on_deepin_shadow_workspace_drag_failed, NULL,
                NULL);

    } else {
        GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(self));
        gtk_style_context_remove_class(context, "deepin-workspace-thumb-clone");
        deepin_setup_style_class(GTK_WIDGET(self), "deepin-workspace-clone"); 

        g_object_disconnect(G_OBJECT(self),
                "any_signal::drag-data-get", on_deepin_shadow_workspace_drag_data_get, NULL,
                "any_signal::drag-begin", on_deepin_shadow_workspace_drag_begin, NULL,
                "any_signal::drag-end", on_deepin_shadow_workspace_drag_end, NULL,
                "any_signal::drag-failed", on_deepin_shadow_workspace_drag_failed, NULL,
                NULL);
    }
}

void deepin_shadow_workspace_focus_next(DeepinShadowWorkspace* self,
        gboolean backward)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    GPtrArray* clones = priv->clones;

    if (!priv->clones || priv->clones->len == 0) {
        priv->window_need_focused = NULL;
        priv->hovered_clone = NULL;
        return;
    }

    // no next window at all
    if (priv->window_need_focused && priv->clones->len == 1) {
        priv->window_need_focused = NULL;
        priv->hovered_clone = NULL;
        return;
    }

    int i = 0;
    if (priv->window_need_focused) {
        for (i = 0; i < clones->len; i++) {
            MetaDeepinClonedWidget* clone = g_ptr_array_index(clones, i);
            if (clone == priv->window_need_focused) break;
        }

        if (i == clones->len) {
            i = 0;
        } else {
            i = backward ? (i - 1 + clones->len) % clones->len 
                : (i + 1) % clones->len;
        }
    }

    if (!priv->thumb_mode) {
#define SCALE_FACTOR 1.03
        if (priv->window_need_focused) {
            double scale = 1.0;
            meta_deepin_cloned_widget_set_scale(priv->window_need_focused, scale, scale);
            meta_deepin_cloned_widget_unselect(priv->window_need_focused);
            if (priv->hovered_clone == priv->window_need_focused) {
                _move_close_button_for(self, priv->window_need_focused);
            }
        }

        MetaDeepinClonedWidget* next = g_ptr_array_index(clones, i);
        double scale = SCALE_FACTOR;
        meta_deepin_cloned_widget_set_scale(next, scale, scale);
        meta_deepin_cloned_widget_select(next);

        if (priv->hovered_clone == next) {
            _move_close_button_for(self, next);
        }
        priv->window_need_focused = next;
    }
}

MetaDeepinClonedWidget* deepin_shadow_workspace_get_focused(DeepinShadowWorkspace* self)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    return priv->window_need_focused;
}

static guint _gdk_x11_device_xi2_translate_state(
        XIModifierState *mods_state,
        XIButtonState   *buttons_state,
        XIGroupState    *group_state)
{
  guint state = 0;

  if (mods_state)
    state = mods_state->effective;

  if (buttons_state)
    {
      gint len, i;

      /* We're only interested in the first 3 buttons */
      len = MIN (3, buttons_state->mask_len * 8);

      for (i = 1; i <= len; i++)
        {
          if (!XIMaskIsSet (buttons_state->mask, i))
            continue;

          switch (i)
            {
            case 1:
              state |= GDK_BUTTON1_MASK;
              break;
            case 2:
              state |= GDK_BUTTON2_MASK;
              break;
            case 3:
              state |= GDK_BUTTON3_MASK;
              break;
            default:
              break;
            }
        }
    }

  if (group_state)
    state |= (group_state->effective) << 13;

  return state;
}

void deepin_shadow_workspace_handle_event(DeepinShadowWorkspace* self,
        XIDeviceEvent* event, KeySym keysym, MetaKeyBindingAction action)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    if (!priv->ready) return;

    meta_verbose("%s: ws%d(%s)\n", __func__, meta_workspace_index(priv->workspace),
            (priv->thumb_mode ? "thumb": ""));

    gboolean backward = FALSE;
    if (keysym == XK_Tab
            || action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS
            || action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD) {
        meta_verbose("tabbing inside expose windows\n");
        if (keysym == XK_Tab)
            backward = event->mods.base & ShiftMask;
        else
            backward = action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD;
        deepin_shadow_workspace_focus_next(self, backward);

    } else if (keysym == XK_Return) {
        MetaDeepinClonedWidget* clone = deepin_shadow_workspace_get_focused(self);
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

        g_idle_add((GSourceFunc)on_idle_end_grab, GUINT_TO_POINTER(event->time));

    } else if (keysym == XK_BackSpace || keysym == XK_Delete || keysym == XK_KP_Delete) {
        MetaDeepinClonedWidget* clone = deepin_shadow_workspace_get_focused(self);
        if (clone) {
            close_window(self, clone);
        }
    }
}

MetaWorkspace* deepin_shadow_workspace_get_workspace(DeepinShadowWorkspace* self)
{
    return self->priv->workspace;
}

gboolean deepin_shadow_workspace_get_is_thumb_mode(DeepinShadowWorkspace* self)
{
    return self->priv->thumb_mode;
}

gboolean deepin_shadow_workspace_get_is_current(DeepinShadowWorkspace* self)
{
    return self->priv->selected;
}

GdkWindow* deepin_shadow_workspace_get_event_window(DeepinShadowWorkspace* self)
{
    return self->priv->event_window;
}

void deepin_shadow_workspace_set_enable_drag(DeepinShadowWorkspace* self, gboolean val)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    if (priv->draggable != val) {
        priv->draggable = val;
        /*
         * the first target is for dragging clonedwidget
         * the second is for dragging ws thumb to be deleted
         */
        static GtkTargetEntry targets[] = {
            {(char*)"window", GTK_TARGET_OTHER_WIDGET, DRAG_TARGET_WINDOW},
        };

        if (val) {
            // as drop target
            gtk_drag_dest_set(GTK_WIDGET(self),
                    GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                    targets, 1, GDK_ACTION_COPY);

        }
    }
}

gboolean deepin_shadow_workspace_is_dragging(DeepinShadowWorkspace* self)
{
    return self->priv->dragging;
}

