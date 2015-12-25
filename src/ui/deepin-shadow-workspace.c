/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-shadow-workspace.c
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * deepin metacity is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * deepin metacity is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#include "deepin-name-entry.h"
#include "deepin-message-hub.h"

#define SET_STATE(w, state)  \
    gtk_style_context_set_state(gtk_widget_get_style_context(GTK_WIDGET(w)), (state))

/* TODO: handle live window add/remove events */

static const int SMOOTH_SCROLL_DELAY = 500;

struct _DeepinShadowWorkspacePrivate
{
    gint disposed: 1;
    gint dynamic: 1; /* if animatable */
    gint selected: 1; 
    gint freeze: 1; /* do not liveupdate when freezed */
    gint thumb_mode: 1; /* show name and no presentation */
    gint ready: 1; /* if dynamic, this is set after presentation finished, 
                      else, set when window placements are done */
    gint animating: 1; /* placement animation is going on */
    gint all_window_mode: 1; // used for Super+a
    gint show_desktop: 1;
    gint closing: 1; 
    gint draggable: 1;

    gint fixed_width, fixed_height;
    gdouble scale; 

    GPtrArray* clones;
    MetaDeepinClonedWidget* hovered_clone;
    MetaWorkspace* workspace;

    int placement_count;

    GdkWindow* event_window;

    GtkWidget* name_box;
    GtkWidget* entry;
    GtkWidget* ws_num;

    GtkWidget* close_button; /* for focused clone */
    MetaDeepinClonedWidget* window_need_focused;

    cairo_pattern_t* snapshot;
    cairo_surface_t* desktop_surface;
    int dock_height; /* used only when in show_desktop */

    void (*close_fnished)(GtkWidget*);
    gpointer close_fnished_data;
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

    if (!priv->closing) {
        GtkRequisition req;
        gtk_widget_get_preferred_size(GTK_WIDGET(clone), &req, NULL);

        ClonedPrivateInfo* info = clone_get_info(clone);
        req.width *= info->init_scale;
        req.height *= info->init_scale;

        meta_verbose("%s: scale down to %f, %d, %d\n", __func__,
                info->init_scale, req.width, req.height);

        meta_deepin_cloned_widget_set_size(clone, req.width, req.height);
    }
    g_signal_handlers_disconnect_by_func(clone, (gpointer)on_window_placed, data); 

    if (++priv->placement_count >= priv->clones->len) {
        priv->animating = FALSE;
        priv->ready = TRUE;
        priv->placement_count = 0;
        if (priv->closing) {
            g_idle_add((GSourceFunc)on_idle_finish_close, self);
        }

        if (priv->hovered_clone && !priv->closing) {
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
            self->priv->dynamic);

    g_signal_connect(G_OBJECT(clone), "transition-finished", 
            (GCallback)on_window_placed, self);

    if (self->priv->dynamic) {
        meta_deepin_cloned_widget_set_scale(clone, 1.0, 1.0);
        meta_deepin_cloned_widget_push_state(clone);
        meta_deepin_cloned_widget_set_scale(clone, fscale, fscale);
        meta_deepin_cloned_widget_pop_state(clone);
    } else {
        meta_deepin_cloned_widget_set_scale(clone, fscale, fscale);
        on_window_placed(clone, self);
    }
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

    /* suppress pending calls */
    if (priv->animating) return;

    if (priv->clones && priv->clones->len) {
        /*g_ptr_array_sort(clones, window_compare);*/

        MetaRectangle area = {
            padding_top, padding_left, 
            priv->fixed_width - padding_left - padding_right - priv->dock_height,
            priv->fixed_height - padding_top - padding_bottom - priv->dock_height
        };

        priv->animating = TRUE;
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
    return G_SOURCE_REMOVE;
}

static void _remove_cloned_widget(DeepinShadowWorkspace* self, 
        MetaDeepinClonedWidget* clone)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    
    MetaWindow* window = meta_deepin_cloned_widget_get_window(clone);
    g_ptr_array_remove(priv->clones, clone);
    gtk_container_remove(GTK_CONTAINER(self), (GtkWidget*)clone);

    meta_verbose("%s remove clone for %s\n", __func__, window->desc);

    if (priv->hovered_clone == clone) {
        priv->hovered_clone = NULL;
        _hide_close_button(self);
    }

    if (!window->screen->closing && priv->ready)
        g_idle_add((GSourceFunc)on_idle, self);
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


static void deepin_shadow_workspace_init (DeepinShadowWorkspace *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_SHADOW_WORKSPACE, DeepinShadowWorkspacePrivate);
    memset(self->priv, 0, sizeof *self->priv);

    self->priv->scale = 1.0;
    gtk_widget_set_sensitive(GTK_WIDGET(self), TRUE);
    gtk_widget_set_app_paintable(GTK_WIDGET(self), TRUE);
    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);
}

static void deepin_shadow_workspace_finalize (GObject *object)
{
    DeepinShadowWorkspace* self = DEEPIN_SHADOW_WORKSPACE(object);
    DeepinShadowWorkspacePrivate* priv = self->priv;
    if (priv->disposed) return;

    priv->disposed = TRUE;

    g_signal_handlers_disconnect_by_data(G_OBJECT(deepin_message_hub_get()), 
            self);

    if (priv->clones) {
        g_ptr_array_free(priv->clones, FALSE);
        priv->clones = NULL;
    }

    if (priv->snapshot) {
        cairo_pattern_destroy(priv->snapshot);
        priv->snapshot = NULL;
    }

    if (priv->desktop_surface) {
        g_clear_pointer(&priv->desktop_surface, cairo_surface_destroy);
    }

    G_OBJECT_CLASS (deepin_shadow_workspace_parent_class)->finalize (object);
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
    if (self->priv->thumb_mode) {
        int entry_height = 0;
        gtk_widget_get_preferred_height(self->priv->entry, &entry_height, NULL);
        *minimum = *natural = self->priv->fixed_height + entry_height 
            + WORKSPACE_NAME_DISTANCE + NAME_SHAPE_PADDING;
    }
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

static void _collect_and_clipping(GtkWidget* widget, cairo_t* cr, 
        cairo_region_t** preg)
{
    cairo_reset_clip(cr);

    DeepinShadowWorkspace *fixed = DEEPIN_SHADOW_WORKSPACE (widget);
    DeepinShadowWorkspacePrivate *priv = fixed->priv;

    cairo_rectangle_int_t parent_alloc; 
    gtk_widget_get_allocation(GTK_WIDGET(widget), &parent_alloc);

    cairo_rectangle_int_t r = {0, 0, parent_alloc.width, parent_alloc.height};
    cairo_region_t* reg = cairo_region_create_rectangle(&r);

    for (int i = 0; i < priv->clones->len; i++) {
        MetaDeepinClonedWidget* clone = g_ptr_array_index(priv->clones, i);

        double sx = 1.0, sy = 1.0;
        gtk_widget_get_allocation(GTK_WIDGET(clone), &r);
        meta_deepin_cloned_widget_get_scale(GTK_WIDGET(clone), &sx, &sy);
        r.x += r.width * (1-sx)/2 - parent_alloc.x;
        r.y += r.height * (1-sy)/2 - parent_alloc.y;
        r.width *= sx; r.height *= sy;
        cairo_region_subtract_rectangle(reg, &r);
    }

    gdk_cairo_region(cr, reg);
    cairo_clip(cr);

    *preg = reg;
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

    /*meta_verbose("%s: ws(%s(%s)) clip (%d, %d, %d, %d)", __func__, */
            /*meta_workspace_get_name(priv->workspace),*/
            /*(priv->thumb_mode ? "thumb": ""),*/
            /*r.x, r.y, r.width, r.height);*/

    gboolean do_snapshot_draw = FALSE;

    if (priv->freeze) {
        MetaRectangle bound = {req.x, req.y, req.width, req.height};
        MetaRectangle clip = {r.x, r.y, r.width, r.height};

        meta_verbose("%s: ws(%s(%s)) frozen\n", __func__, 
                meta_workspace_get_name(priv->workspace),
                (priv->thumb_mode ? "thumb": ""));
        if (priv->snapshot) {
            cairo_set_source(cr, priv->snapshot);
            cairo_paint(cr);
            return TRUE; 

        } else if (meta_rectangle_could_fit_rect(&clip, &bound)) {
            meta_verbose("%s: frozen, do a full render\n", __func__);
            cairo_push_group(cr);
            do_snapshot_draw = TRUE;
        }
    }

    GtkStyleContext* context = gtk_widget_get_style_context(widget);

    cairo_save(cr);
    cairo_region_t* reg = NULL;

    if (priv->animating && !priv->ready) {
        _collect_and_clipping(widget, cr, &reg);
    }

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

    if (priv->show_desktop && priv->desktop_surface) {
        cairo_set_source_surface(cr, priv->desktop_surface, 0, 0);
        cairo_paint(cr);
    } else {
        cairo_surface_t* ref = deepin_background_cache_get_surface(
                priv->scale);
        if (ref != NULL) {
            cairo_set_source_surface(cr, ref, 0, 0);
        }
    }

    cairo_paint(cr);

    cairo_restore(cr);
    if (reg) cairo_region_destroy(reg);
    
    GTK_WIDGET_CLASS(deepin_shadow_workspace_parent_class)->draw(
            widget, cr);

    if (do_snapshot_draw) {
        priv->snapshot = cairo_pop_group(cr);
        cairo_set_source(cr, priv->snapshot);
        cairo_paint(cr);
    }
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

    object_class->finalize = deepin_shadow_workspace_finalize;
}

static gboolean on_idle_focus_out_entry(GtkWidget* entry)
{
    if (!GTK_IS_ENTRY(entry)) return G_SOURCE_REMOVE;

    meta_verbose("%s\n", __func__);
    GdkEvent *fevent = gdk_event_new (GDK_FOCUS_CHANGE);
    fevent->focus_change.type = GDK_FOCUS_CHANGE;
    fevent->focus_change.in = FALSE;
    fevent->focus_change.window = gtk_widget_get_window(entry);
    if (fevent->focus_change.window != NULL)
        g_object_ref (fevent->focus_change.window);

    gtk_widget_send_focus_change(entry, fevent);

    gdk_event_free (fevent);

    return G_SOURCE_REMOVE;
}

static gboolean on_entry_pressed(GtkWidget* entry,
               GdkEvent* event, gpointer user_data)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)user_data;
    DeepinShadowWorkspacePrivate* priv =self->priv;

    GdkEventButton evb = event->button;
    if (!gtk_widget_has_grab(entry)) {
        meta_verbose("%s: %p grab\n", __func__, GDK_WINDOW_XID(gtk_widget_get_window(entry)));
        gtk_grab_add(entry);
        gtk_entry_grab_focus_without_selecting(GTK_ENTRY(entry));

    } else {
        gint x = evb.x_root, y = evb.y_root;
        GtkAllocation alloc;
        gtk_widget_get_allocation(entry, &alloc);

        GdkRectangle r = {alloc.x, alloc.y, alloc.width, alloc.height};
        if (x <= r.x || x >= r.x + r.width || y <= r.y || y >= r.y + r.height) {
            meta_verbose("%s: ungrab and replay\n", __func__);
            /* hack: when click out side of entry, loose grab and replay click,
             * incase some other entry was clicked and cannot get event */
            gtk_editable_select_region(GTK_EDITABLE(entry), 0, 0);
            gtk_grab_remove(entry);
            g_idle_add((GSourceFunc)on_idle_focus_out_entry, entry);

            return TRUE;
        }
    }
    return FALSE;
}

static gboolean on_entry_key_pressed(GtkWidget* entry,
               GdkEvent* event, gpointer data)
{
    meta_verbose("%s\n", __func__);
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;

    if (event->key.keyval == GDK_KEY_Tab) 
        return TRUE;

    if (event->key.keyval == GDK_KEY_Return) {
        const char* orig = meta_workspace_get_name(self->priv->workspace); 
        const char* new_name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (!g_str_equal(new_name, orig)) {
            int id = meta_workspace_index(self->priv->workspace);
            meta_prefs_change_workspace_name(id, new_name);
        }

        if (gtk_widget_has_grab(entry)) {
            gtk_editable_select_region(GTK_EDITABLE(entry), 0, 0);
            gtk_grab_remove(entry);
            g_idle_add((GSourceFunc)on_idle_focus_out_entry, entry);
        }

        return TRUE;
    }

    /* pass through to make keys work */
    return FALSE;
}

static void on_entry_changed(GtkWidget* entry, gpointer data)
{
    DeepinShadowWorkspacePrivate* priv = DEEPIN_SHADOW_WORKSPACE(data)->priv;

    gtk_widget_queue_resize(entry);
    gtk_widget_queue_draw(priv->name_box);
}

static void on_entry_preedit_changed(GtkEntry *entry, gchar *preedit,
               gpointer data)
{
    on_entry_changed(entry, data);
}

static void _create_entry(DeepinShadowWorkspace* self)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    if (priv->entry) return;

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    priv->name_box = box;
    deepin_setup_style_class(box, "deepin-workspace-thumb-clone-name-box");
    
    char* num = g_strdup_printf("%d", meta_workspace_index(priv->workspace) + 1);
    priv->ws_num = gtk_label_new(num);
    deepin_setup_style_class(priv->ws_num, "deepin-workspace-thumb-clone-name");
    g_free(num);

    g_object_set(G_OBJECT(priv->ws_num), "margin-start", 6, NULL);
    gtk_box_pack_start(GTK_BOX(box), priv->ws_num, FALSE, FALSE, 0);

    priv->entry = deepin_name_entry_new();
    gtk_entry_set_text(GTK_ENTRY(priv->entry), meta_workspace_get_name(priv->workspace));
    gtk_entry_set_width_chars(GTK_ENTRY(priv->entry), 
            gtk_entry_get_text_length(GTK_ENTRY(priv->entry))+2);

    g_object_set(G_OBJECT(priv->entry), "margin-end", 6, NULL);
    gtk_box_pack_start(GTK_BOX(box), priv->entry, TRUE, TRUE, 0);

    GtkAllocation alloc;
    gtk_widget_get_allocation(box, &alloc);

    deepin_fixed_put(DEEPIN_FIXED(self), box,
            (priv->fixed_width - alloc.width)/2, 
            priv->fixed_height + WORKSPACE_NAME_DISTANCE + alloc.height/2);

    g_object_connect(G_OBJECT(priv->entry),
            "signal::button-press-event", on_entry_pressed, self,
            "signal::key-press-event", on_entry_key_pressed, self,
            "signal::changed", on_entry_changed, self,
            "signal::preedit-changed", on_entry_preedit_changed, self,
            NULL);
    gtk_widget_show_all(priv->name_box);
}

// propagate from cloned
static gboolean on_deepin_cloned_widget_leaved(MetaDeepinClonedWidget* cloned,
               GdkEvent* event, gpointer data)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;
    if (!self->priv->ready) return FALSE;

    if (self->priv->thumb_mode) {
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

    if (!priv->thumb_mode) {
        priv->hovered_clone = cloned;
        /* delay show up if is animating */
        if (!priv->animating && priv->ready) {
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

static gboolean on_deepin_cloned_widget_released(MetaDeepinClonedWidget* cloned,
               GdkEvent* event, gpointer data)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;
    meta_verbose("%s\n", __func__);
    if (!self->priv->ready) return FALSE;

    if (meta_deepin_cloned_widget_is_dragging(cloned)) {
        // stop here, so workspace won't get event
        return TRUE;
    }

    if (!self->priv->thumb_mode) {
        MetaWindow* mw = meta_deepin_cloned_widget_get_window(cloned);
        if (mw->workspace && mw->workspace != mw->screen->active_workspace) {
            meta_workspace_activate(mw->workspace, gdk_event_get_time(event));
        }
        meta_window_activate(mw, gdk_event_get_time(event));
        g_idle_add((GSourceFunc)on_idle_end_grab, gdk_event_get_time(event));
        return TRUE;
    }

    /* pass to parent workspace */
    return FALSE;
}

static gboolean on_close_button_clicked(GtkWidget* widget,
               GdkEvent* event, gpointer data)
{
    meta_verbose("%s\n", __func__);
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;
    DeepinShadowWorkspacePrivate* priv = self->priv;

    if (!priv->ready) return FALSE;

    MetaWindow* meta_window = meta_deepin_cloned_widget_get_window(priv->hovered_clone);
    GtkWidget* clone = (GtkWidget*)priv->hovered_clone;
    g_ptr_array_remove(priv->clones, clone);
    gtk_container_remove(GTK_CONTAINER(self), clone);

    meta_window_delete(meta_window, CurrentTime);

    priv->hovered_clone = NULL;
    _hide_close_button(self);

    g_idle_add((GSourceFunc)on_idle, self);
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

    GList* ls = meta_stack_list_windows(ws->screen->stack,
            priv->all_window_mode? NULL: ws);
    GList* l = ls;
    while (l) {
        MetaWindow* win = (MetaWindow*)l->data;
        if (win->type == META_WINDOW_NORMAL) {
            GtkWidget* widget = meta_deepin_cloned_widget_new(win);
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

            deepin_fixed_put(DEEPIN_FIXED(self), widget,
                    r.x * priv->scale + w/2,
                    r.y * priv->scale + h/2);

            g_object_connect(G_OBJECT(widget),
                    "signal::enter-notify-event", on_deepin_cloned_widget_entered, self,
                    "signal::leave-notify-event", on_deepin_cloned_widget_leaved, self,
                    "signal::button-release-event", on_deepin_cloned_widget_released, self,
                    NULL);
        }


        l = l->next;
    }
    g_list_free(ls);

    if (priv->thumb_mode && !priv->entry) {
        _create_entry(self);
    }

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

    if (priv->show_desktop) {
        if (priv->desktop_surface) {
            g_clear_pointer(&priv->desktop_surface, cairo_surface_destroy);
        }

        priv->dock_height = 0;

        MetaWindow *desktop_win = NULL, *dock_win = NULL;

        GList* windows = priv->workspace->mru_list;
        while (windows != NULL) {
            MetaWindow *w = (MetaWindow*)windows->data;
            if (w->type == META_WINDOW_DESKTOP) {
                desktop_win = w;
            }

            if (w->type == META_WINDOW_DOCK) {
                dock_win = w;
            }

            if (desktop_win && dock_win) break;
            windows = windows->next;
        }

        MetaRectangle r1 = {0, 0, 0, 0}, r2 = {0, 0, 0, 0};
        cairo_surface_t* aux1 = NULL, *aux2 = NULL;

        if (desktop_win) {
            meta_window_get_outer_rect(desktop_win, &r1);
            aux1 = deepin_window_surface_manager_get_surface(desktop_win, 1.0); 
        }

        if (dock_win) {
            meta_window_get_outer_rect(dock_win, &r2);
            aux2 = deepin_window_surface_manager_get_surface(dock_win, 1.0); 
            priv->dock_height = r2.height;
        }

        priv->desktop_surface = deepin_window_surface_manager_get_combined3(
                deepin_background_cache_get_surface(1.0), 
                aux1, r1.x, r1.y,
                aux2, r2.x, r2.y,
                1.0);
    }

    gtk_widget_queue_resize(GTK_WIDGET(self));
}

static void on_deepin_shadow_workspace_show(DeepinShadowWorkspace* self, gpointer data)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    g_idle_add((GSourceFunc)on_idle, self);
}

static gboolean on_deepin_shadow_workspace_released(DeepinShadowWorkspace* self,
               GdkEvent* event, gpointer user_data)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    meta_verbose("%s: ws %s(%s)\n", __func__, meta_workspace_get_name(priv->workspace),
            priv->thumb_mode ? "thumb":"normal");

    if (!priv->selected) return FALSE;

    if (!priv->ready || priv->hovered_clone) return TRUE;

    if (!priv->thumb_mode) {
        g_idle_add((GSourceFunc)on_idle_end_grab, gdk_event_get_time(event));
        return TRUE;
    }
    
    //TODO: do workspace change if the pressed is not current
    //bubble up to parent to determine what to do
    return FALSE;
}

static void on_window_change_workspace(DeepinMessageHub* hub, MetaWindow* window,
        MetaWorkspace* new_workspace, gpointer user_data)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)user_data;
    DeepinShadowWorkspacePrivate* priv = self->priv;
    meta_verbose("%s: ws %s(%s)\n", __func__, meta_workspace_get_name(priv->workspace),
            priv->thumb_mode ? "thumb":"normal");

    if (!priv->ready || !priv->clones) return;
    
    if (priv->workspace == new_workspace) { // dest workspace
        if (window->type != META_WINDOW_NORMAL) return;
        meta_verbose("%s: add window\n", __func__);

        //add window
        GtkWidget* widget = meta_deepin_cloned_widget_new(window);
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
                "signal::button-release-event", on_deepin_cloned_widget_released, self,
                NULL);
        gtk_widget_show(widget);
        g_idle_add((GSourceFunc)on_idle, self);

    } else if (window->workspace == priv->workspace) { // maybe source workspace
        on_window_removed(hub, window, user_data);
    }
}
    
static void on_drag_data_received(GtkWidget* widget, GdkDragContext* context,
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

static gboolean on_drag_drop(GtkWidget* widget, GdkDragContext* context,
               gint x, gint y, guint time, gpointer user_data)
{
    meta_verbose("%s\n", __func__);
    return FALSE;
}

static gboolean on_deepin_shadow_workspace_event(DeepinShadowWorkspace* self,
        GdkEvent* ev, gpointer data)
{
    /*meta_verbose("%s", __func__);*/
    return FALSE;
}

GtkWidget* deepin_shadow_workspace_new(void)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)g_object_new(
            DEEPIN_TYPE_SHADOW_WORKSPACE, NULL);

    GdkScreen* screen = gdk_screen_get_default();
    self->priv->fixed_width = gdk_screen_get_width(screen);
    self->priv->fixed_height = gdk_screen_get_height(screen);
    self->priv->scale = 1.0;

    SET_STATE (self, GTK_STATE_FLAG_NORMAL);
    deepin_setup_style_class(GTK_WIDGET(self), "deepin-workspace-clone"); 
    

    deepin_shadow_workspace_set_enable_drag(self, TRUE);

    g_object_connect(G_OBJECT(self),
            "signal::event", on_deepin_shadow_workspace_event, NULL,
            "signal::show", on_deepin_shadow_workspace_show, NULL,
            "signal::button-release-event", on_deepin_shadow_workspace_released, NULL,
            "signal::drag-data-received", on_drag_data_received, NULL, 
            "signal::drag-drop", on_drag_drop, NULL, 
            NULL);

    g_object_connect(G_OBJECT(deepin_message_hub_get()), 
            "signal::window-removed", on_window_removed, self,
            "signal::about-to-change-workspace", on_window_change_workspace, self,
            NULL);

    return (GtkWidget*)self;
}

void deepin_shadow_workspace_set_scale(DeepinShadowWorkspace* self, gdouble s)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;

    MetaDisplay* display = meta_get_display();
    MetaRectangle r = display->active_screen->rect;

    priv->scale = s;
    priv->fixed_width = r.width * s;
    priv->fixed_height = r.height * s;

    /* FIXME: need to check if repopulate */

    gtk_widget_queue_resize(GTK_WIDGET(self));
}

gdouble deepin_shadow_workspace_get_scale(DeepinShadowWorkspace* self)
{
    return self->priv->scale;
}

void deepin_shadow_workspace_set_presentation(DeepinShadowWorkspace* self,
        gboolean val)
{
    self->priv->dynamic = val;
}

void deepin_shadow_workspace_set_current(DeepinShadowWorkspace* self,
        gboolean val)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    priv->selected = val;

    GtkStateFlags state = priv->selected? GTK_STATE_FLAG_SELECTED: GTK_STATE_FLAG_NORMAL;
    
    SET_STATE (self, state);
    if (priv->name_box) {
        SET_STATE (priv->name_box, state);
        SET_STATE (priv->ws_num, state);
        SET_STATE (priv->entry, state);
        if (!val && gtk_editable_get_selection_bounds(GTK_EDITABLE(priv->entry), NULL, NULL)) {
            gtk_entry_reset_im_context(GTK_ENTRY(priv->entry));
            gtk_editable_select_region(GTK_EDITABLE(priv->entry), 0, 0);
        }
    }
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void deepin_shadow_workspace_set_thumb_mode(DeepinShadowWorkspace* self,
        gboolean val)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    priv->thumb_mode = val;
    if (val) {
        deepin_shadow_workspace_set_presentation(self, FALSE);
        GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(self));
        gtk_style_context_remove_class(context, "deepin-workspace-clone"); 
        deepin_setup_style_class(GTK_WIDGET(self), "deepin-workspace-thumb-clone");
        if (priv->workspace && !priv->entry) {
            _create_entry(self);
        }

    } else {
        GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(self));
        gtk_style_context_remove_class(context, "deepin-workspace-thumb-clone");
        deepin_setup_style_class(GTK_WIDGET(self), "deepin-workspace-clone"); 

        if (priv->name_box) gtk_widget_hide(priv->name_box);
    }
}

static void on_deepin_shadow_workspace_focus_finished(
        MetaDeepinClonedWidget* clone, gpointer data)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)data;
    _move_close_button_for(self, clone);

    g_signal_handlers_disconnect_by_func(clone, 
            on_deepin_shadow_workspace_focus_finished, data); 
}

void deepin_shadow_workspace_focus_next(DeepinShadowWorkspace* self,
        gboolean backward)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    GPtrArray* clones = priv->clones;

    if (!priv->clones || priv->clones->len == 0) return;

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
            double scale = SCALE_FACTOR;
            meta_deepin_cloned_widget_set_scale(priv->window_need_focused, scale, scale);
            meta_deepin_cloned_widget_push_state(priv->window_need_focused);
            scale = 1.0;
            meta_deepin_cloned_widget_set_scale(priv->window_need_focused, scale, scale);
            meta_deepin_cloned_widget_unselect(priv->window_need_focused);
            if (priv->hovered_clone == priv->window_need_focused) {
                g_signal_connect(G_OBJECT(priv->window_need_focused),
                        "transition-finished", 
                        (GCallback)on_deepin_shadow_workspace_focus_finished,
                        self);
            }
        }

        MetaDeepinClonedWidget* next = g_ptr_array_index(clones, i);
        double scale = 1.0;
        meta_deepin_cloned_widget_set_scale(next, scale, scale);
        meta_deepin_cloned_widget_push_state(next);
        scale *= SCALE_FACTOR;
        meta_deepin_cloned_widget_set_scale(next, scale, scale);
        meta_deepin_cloned_widget_select(next);

        if (priv->hovered_clone == next) {
            g_signal_connect(G_OBJECT(next), "transition-finished", 
                    (GCallback)on_deepin_shadow_workspace_focus_finished, self);
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

static void _entry_handle_event(GtkWidget* w, XIDeviceEvent* event)
{
    GdkEvent* kev = gdk_event_new(GDK_KEY_PRESS);

    GdkDisplay* display = gdk_display_get_default();
    GdkKeymap *keymap = gdk_keymap_get_for_display (display);
    GdkDeviceManager* dev_manger = gdk_display_get_device_manager(display);
    GdkDevice* client_pointer = gdk_device_manager_get_client_pointer(dev_manger);
    GdkModifierType consumed, state;
    GdkDevice *device, *source_device;

    kev->key.send_event = TRUE;

    kev->key.type = event->evtype == XI_KeyPress ? GDK_KEY_PRESS : GDK_KEY_RELEASE;

    kev->key.window = gtk_widget_get_window(w);
    g_object_ref(kev->key.window);

    kev->key.time = event->time;
    kev->key.state = _gdk_x11_device_xi2_translate_state (&event->mods,
            &event->buttons, &event->group);
    kev->key.group = event->group.effective;

    kev->key.hardware_keycode = event->detail;
    kev->key.is_modifier = gdk_x11_keymap_key_is_modifier (keymap, kev->key.hardware_keycode);

    device = gdk_device_get_associated_device(client_pointer);
    gdk_event_set_device (kev, device);

    source_device = device;
    gdk_event_set_source_device (kev, source_device);

    kev->key.keyval = GDK_KEY_VoidSymbol;

    gdk_keymap_translate_keyboard_state (keymap,
            kev->key.hardware_keycode,
            kev->key.state,
            kev->key.group,
            &kev->key.keyval,
            NULL, NULL, &consumed);

    state = kev->key.state & ~consumed;
    /*_gdk_x11_keymap_add_virt_mods (keymap, &state);*/
    kev->key.state |= state;

    kev->key.string = NULL;

    /*gtk_propagate_event(w, kev);*/
    gtk_widget_event(w, kev);
    gdk_event_free(kev);
}

void deepin_shadow_workspace_handle_event(DeepinShadowWorkspace* self,
        XIDeviceEvent* event, KeySym keysym, MetaKeyBindingAction action)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    if (!priv->ready) return;

    meta_verbose("%s: ws(%s(%s))\n", __func__, 
            meta_workspace_get_name(priv->workspace),
            (priv->thumb_mode ? "thumb": ""));

    GtkWidget* w = gtk_grab_get_current();
    if (w && GTK_IS_ENTRY(w)) {
        _entry_handle_event(w, event);
        return;
    }

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

        g_idle_add((GSourceFunc)on_idle_end_grab, event->time);

    } else if (keysym == XK_F2) {
        gtk_grab_add(priv->entry);
        gtk_widget_grab_focus(priv->entry);
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

void deepin_shadow_workspace_set_frozen(DeepinShadowWorkspace* self,
        gboolean val)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    gboolean old = priv->freeze;
    if (old == val) return;

    if (old && priv->snapshot) {
        cairo_pattern_destroy(priv->snapshot);
        priv->snapshot = NULL;
    }

    priv->freeze = val;
}

gboolean deepin_shadow_workspace_get_is_freezed(DeepinShadowWorkspace* self)
{
    return self->priv->freeze;
}

void deepin_shadow_workspace_set_show_all_windows(DeepinShadowWorkspace* self,
        gboolean val)
{
    self->priv->all_window_mode = val;
}

gboolean deepin_shadow_workspace_get_is_all_window_mode(DeepinShadowWorkspace* self)
{
    return self->priv->all_window_mode;
}

void deepin_shadow_workspace_set_show_desktop(DeepinShadowWorkspace* self, 
        gboolean val)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;

    if (priv->show_desktop != val) {
        priv->show_desktop = val;
    }
}

static MetaDeepinClonedWidget* _find_clone_by_meta(DeepinShadowWorkspacePrivate* priv, MetaWindow* window)
{
    for (gint i = 0; i < priv->clones->len; i++) {
        MetaDeepinClonedWidget* clone = g_ptr_array_index(priv->clones, i);
        if (meta_deepin_cloned_widget_get_window(clone) == window) {
            return clone;
        }
    }

    return NULL;
}

static void _rearrange_clones_with_stack(DeepinShadowWorkspace* self)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;

    GList* ls = meta_stack_list_windows(priv->workspace->screen->stack,
            priv->all_window_mode? NULL: priv->workspace);
    GList* l = ls;
    while (l) {
        MetaWindow* win = (MetaWindow*)l->data;
        if (win->type == META_WINDOW_NORMAL) {
            MetaDeepinClonedWidget* clone = _find_clone_by_meta(priv, win);
            g_assert(clone != NULL);

            deepin_fixed_raise(DEEPIN_FIXED(self), clone);
        }

        l = l->next;
    }
    g_list_free(ls);
}

void deepin_shadow_workspace_close(DeepinShadowWorkspace* self,
        gboolean animated, void (*finished)(GtkWidget*), gpointer user_data)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    priv->close_fnished = finished;
    priv->close_fnished_data = user_data;

    if (!animated || priv->clones->len == 0) {
        g_idle_add((GSourceFunc)on_idle_finish_close, self);
        return;
    }

    priv->closing = TRUE;
    priv->placement_count = 0;
    priv->animating = TRUE;

    _rearrange_clones_with_stack(self);

    gint index = 0;
    for (; index < priv->clones->len; index++) {
        MetaDeepinClonedWidget* clone =
            (MetaDeepinClonedWidget*)g_ptr_array_index(priv->clones, index);
        MetaWindow* window = meta_deepin_cloned_widget_get_window(clone);

        MetaRectangle window_rect;
        meta_window_get_input_rect(window, &window_rect);

        place_window(self, clone, window_rect);
    }
}

void deepin_shadow_workspace_declare_name(DeepinShadowWorkspace* self)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    if (priv->thumb_mode && priv->name_box) {
        int id = meta_workspace_index(priv->workspace);
        const char* name = gtk_entry_get_text(GTK_ENTRY(priv->entry));
        meta_prefs_change_workspace_name(id, name);

        char* num = g_strdup_printf("%d", id + 1);
        gtk_label_set_text(GTK_LABEL(priv->ws_num), num);
        g_free(num);
    }
}

void deepin_shadow_workspace_set_enable_drag(DeepinShadowWorkspace* self, gboolean val)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    if (priv->draggable != val) {
        priv->draggable = val;
        
        static GtkTargetEntry targets[] = {
            {(char*)"window", GTK_TARGET_OTHER_WIDGET, DRAG_TARGET_WINDOW},
        };

        if (val) {
            gtk_drag_dest_set(GTK_WIDGET(self),
                    GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                    targets, G_N_ELEMENTS(targets), GDK_ACTION_COPY);
        } else {
            gtk_drag_dest_set(NULL,
                    GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                    targets, G_N_ELEMENTS(targets), GDK_ACTION_COPY);
        }
    }
}

