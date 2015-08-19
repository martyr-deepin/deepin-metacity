/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-shadow-workspace.c
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * gtk-skeleton is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * gtk-skeleton is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <math.h>
#include <util.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <cairo-xlib.h>

#include "../core/workspace.h"
#include "deepin-cloned-widget.h"
#include "compositor.h"
#include "deepin-design.h"
#include "deepin-ease.h"
#include "deepin-shadow-workspace.h"
#include "deepin-background-cache.h"

/* TODO: handle live window add/remove events */

static const int SMOOTH_SCROLL_DELAY = 500;

struct _DeepinShadowWorkspacePrivate
{
    gint disposed: 1;
    gint dynamic: 1; /* if animatable */
    gint selected: 1; 
    gint freeze: 1; /* do not liveupdate when freezed */
    gint thumb_mode: 1;

    gint fixed_width, fixed_height;
    gdouble scale; 

    GPtrArray* clones;
    MetaWorkspace* workspace;

    MetaDeepinClonedWidget* window_need_focused;
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


static void on_window_placed(MetaDeepinClonedWidget* clone, gpointer data)
{
    ClonedPrivateInfo* info = clone_get_info(clone);

    GtkRequisition req;
    gtk_widget_get_preferred_size(GTK_WIDGET(clone), &req, NULL);
    req.width *= info->init_scale;
    req.height *= info->init_scale;

    g_message("%s: scale down to %f, %d, %d", __func__, info->init_scale,
            req.width, req.height);

    meta_deepin_cloned_widget_set_size(clone, req.width, req.height);
    g_signal_handlers_disconnect_by_func(clone, on_window_placed, data); 
}

G_DEFINE_TYPE (DeepinShadowWorkspace, deepin_shadow_workspace, DEEPIN_TYPE_FIXED);

static void place_window(DeepinShadowWorkspace* self,
        MetaDeepinClonedWidget* clone, MetaRectangle rect)
{
    GtkRequisition req;
    gtk_widget_get_preferred_size(GTK_WIDGET(clone), &req, NULL);

    float fscale = (float)rect.width / req.width;
    g_debug("%s: bound: %d,%d,%d,%d, scale %f", __func__,
            rect.x, rect.y, rect.width, rect.height, fscale);
    ClonedPrivateInfo* info = clone_get_info(clone);
    info->init_scale = fscale;

    deepin_fixed_move(DEEPIN_FIXED(self), GTK_WIDGET(clone),
            rect.x + req.width * fscale /2, rect.y + req.height * fscale /2,
            self->priv->dynamic);

    g_signal_connect(G_OBJECT(clone), "transition-finished", 
            (GCallback)on_window_placed, info);

    if (self->priv->dynamic) {
        meta_deepin_cloned_widget_set_scale(clone, 1.0, 1.0);
        meta_deepin_cloned_widget_push_state(clone);
        meta_deepin_cloned_widget_set_scale(clone, fscale, fscale);
        meta_deepin_cloned_widget_pop_state(clone);
    } else {
        meta_deepin_cloned_widget_set_scale(clone, fscale, fscale);
        on_window_placed(clone, info);
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

static void natural_placement (DeepinShadowWorkspace* self, MetaRectangle area)
{
    /*g_debug("%s: geom: %d,%d,%d,%d", __func__, area.x, area.y, area.width, area.height);*/
    DeepinShadowWorkspacePrivate* priv = self->priv;
    GPtrArray* clones = priv->clones;

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
        /*g_debug("%s: frame: %d,%d,%d,%d", __func__, rect.x, rect.y, rect.width, rect.height);*/

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
    GPtrArray* clones = priv->clones;
    if (priv->clones->len) {
        /*g_ptr_array_sort(clones, window_compare);*/

        MetaRectangle area = {
            padding_top, padding_left, 
            priv->fixed_width - padding_left - padding_right,
            priv->fixed_height - padding_top - padding_bottom
        };

        natural_placement(self, area);
    }
}


static void deepin_shadow_workspace_init (DeepinShadowWorkspace *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_SHADOW_WORKSPACE, DeepinShadowWorkspacePrivate);

    self->priv->scale = 1.0;
}

static void deepin_shadow_workspace_finalize (GObject *object)
{
    DeepinShadowWorkspacePrivate* priv = DEEPIN_SHADOW_WORKSPACE(object)->priv;
    if (priv->clones) {
        g_ptr_array_free(priv->clones, FALSE);
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

    cairo_close_path(cr);
}

/* FIXME: no need to draw when do moving animation, just a snapshot */
static gboolean deepin_shadow_workspace_draw (GtkWidget *widget,
        cairo_t *cr)
{
    DeepinShadowWorkspace *fixed = DEEPIN_SHADOW_WORKSPACE (widget);
    DeepinShadowWorkspacePrivate *priv = fixed->priv;

    /*GdkRectangle r;*/
    /*gdk_cairo_get_clip_rectangle(cr, &r);*/
    /*g_message("%s: clip %d, %d, %d, %d", __func__, r.x, r.y, r.width, r.height);*/

    GtkAllocation req;
    gtk_widget_get_allocation(widget, &req);
    /*g_message("%s: (%d, %d, %d, %d)", __func__, req.x, req.y, req.width, req.height);*/

    GtkStyleContext* context = gtk_widget_get_style_context(widget);

    if (priv->thumb_mode) {

        /* FIXME: why can not get borders */
        /*GtkBorder borders;*/
        /*_style_get_borders(context, &borders);*/

        int b = 2;
        gtk_render_background(context, cr, -b, -b, req.width+2*b, req.height+2*b);

        _draw_round_box(cr, req.width, req.height, 4.0);
        cairo_clip(cr);

    } else {
        gtk_render_background(context, cr, 0, 0, req.width, req.height);
    }

    cairo_set_source_surface(cr,
            deepin_background_cache_get_surface(priv->scale), 0, 0);
    cairo_paint(cr);

    return GTK_WIDGET_CLASS(deepin_shadow_workspace_parent_class)->draw(widget, cr);
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

    _gtk_widget_set_simple_clip(widget, NULL);
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

    object_class->finalize = deepin_shadow_workspace_finalize;
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
            GtkWidget* widget = meta_deepin_cloned_widget_new(win);
            g_ptr_array_add(priv->clones, widget);

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
        }


        l = l->next;
    }
    g_list_free(ls);

    gtk_widget_queue_resize(GTK_WIDGET(self));
}

static gboolean on_idle(DeepinShadowWorkspace* self)
{
    if (!self->priv->thumb_mode) calculate_places(self);
    return G_SOURCE_REMOVE;
}

static void on_deepin_shadow_workspace_show(DeepinShadowWorkspace* self, gpointer data)
{
    g_idle_add((GSourceFunc)on_idle, self);
}

GtkWidget* deepin_shadow_workspace_new(void)
{
    DeepinShadowWorkspace* self = (DeepinShadowWorkspace*)g_object_new(
            DEEPIN_TYPE_SHADOW_WORKSPACE, NULL);

    GdkScreen* screen = gdk_screen_get_default();
    self->priv->fixed_width = gdk_screen_get_width(screen);
    self->priv->fixed_height = gdk_screen_get_height(screen);
    self->priv->scale = 1.0;

    GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(self));
    gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
    deepin_setup_style_class(GTK_WIDGET(self), "deepin-workspace-clone"); 

    g_signal_connect(G_OBJECT(self), "show",
            (GCallback)on_deepin_shadow_workspace_show, NULL);

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

void deepin_shadow_workspace_set_presentation(DeepinShadowWorkspace* self,
        gboolean val)
{
    self->priv->dynamic = val;
}

void deepin_shadow_workspace_set_current(DeepinShadowWorkspace* self,
        gboolean val)
{
    self->priv->selected = val;

    GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(self));
    if (self->priv->selected) {
        gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);
    } else {
        gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
    }
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void deepin_shadow_workspace_set_thumb_mode(DeepinShadowWorkspace* self,
        gboolean val)
{
    self->priv->thumb_mode = val;
    if (val) {
        deepin_shadow_workspace_set_presentation(self, FALSE);
        GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(self));
        gtk_style_context_remove_class(context, "deepin-workspace-clone"); 
        deepin_setup_style_class(GTK_WIDGET(self), "deepin-workspace-thumb-clone");
    }
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
    } else {
        i = 0;
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
        }

        MetaDeepinClonedWidget* next = g_ptr_array_index(clones, i);
        double scale = 1.0;
        meta_deepin_cloned_widget_set_scale(next, scale, scale);
        meta_deepin_cloned_widget_push_state(next);
        scale *= SCALE_FACTOR;
        meta_deepin_cloned_widget_set_scale(next, scale, scale);
        meta_deepin_cloned_widget_select(next);

        priv->window_need_focused = next;
    }
}

MetaDeepinClonedWidget* deepin_shadow_workspace_get_focused(DeepinShadowWorkspace* self)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;
    return priv->window_need_focused;
}

void deepin_shadow_workspace_handle_event(DeepinShadowWorkspace* self,
        XEvent* event, KeySym keysym, MetaKeyBindingAction action)
{
    DeepinShadowWorkspacePrivate* priv = self->priv;

    gboolean backward = FALSE;
    if (keysym == XK_Tab
            || action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS
            || action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD) {
        g_message("tabbing inside expose windows");
        if (keysym == XK_Tab)
            backward = event->xkey.state & ShiftMask;
        else
            backward = action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD;
        deepin_shadow_workspace_focus_next(self, backward);

    } if (keysym == XK_Return) {
        meta_display_end_grab_op(priv->workspace->screen->display, event->xkey.time);

        MetaDeepinClonedWidget* clone = deepin_shadow_workspace_get_focused(self);
        if (!clone) {
            meta_workspace_focus_default_window(priv->workspace, NULL, event->xkey.time);
        } else {
            MetaWindow* mw = meta_deepin_cloned_widget_get_window(clone);
            if (mw) {
                meta_window_activate(mw, event->xkey.time);
            }
        }
    }
}

MetaWorkspace* deepin_shadow_workspace_get_workspace(DeepinShadowWorkspace* self)
{
    return self->priv->workspace;
}


