/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <math.h>
#include <util.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <cairo-xlib.h>
#include "compositor.h"
#include "deepin-cloned-widget.h"
#include "deepin-design.h"
#include "deepin-stackblur.h"
#include "deepin-ease.h"

/* target values for animation */
typedef struct _AnimationInfo 
{
    gdouble tx, ty;  /* translation */
    gdouble scale_x, scale_y;   /* scale */
    gdouble angle;   /* rotation, clockwise is negative */
    gdouble blur_radius;
    gdouble alpha; 
} AnimationInfo;

typedef struct _MetaDeepinClonedWidgetPrivate
{
    gboolean selected;

    gdouble pivot_x, pivot_y; /* around which to scale and rotate */

    AnimationInfo ai;
    gint animation_stack; /* > 0, animation info needs pushed */

    gdouble tx, ty;  /* translation */
    gdouble scale_x, scale_y;   /* scale */
    gdouble angle;   /* rotation, clockwise is negative */
    gdouble blur_radius;
    gdouble alpha;

    gboolean animation; /* in animation */

    gdouble current_pos;
    gdouble target_pos;

    gint64 start_time;
    gint64 last_time;
    gint64 end_time;

    guint tick_id;
    int  animation_duration;

    int render_background: 1;
    int render_frame: 1;

    MetaWindow* meta_window;
    cairo_surface_t* snapshot;

    GtkRequisition real_size;
} MetaDeepinClonedWidgetPrivate;

enum {
    PROP_0,
    PROP_ALPHA,
    PROP_SCALE_X,
    PROP_SCALE_Y,
    PROP_ROTATE,
    PROP_TRANSLATE_X,
    PROP_TRANSLATE_Y,
    N_PROPERTIES
};

static GParamSpec* property_specs[N_PROPERTIES] = {NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (MetaDeepinClonedWidget, meta_deepin_cloned_widget, GTK_TYPE_WIDGET);

static void meta_deepin_cloned_widget_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    MetaDeepinClonedWidget* self = META_DEEPIN_CLONED_WIDGET(object);

    switch (property_id)
    {
        case PROP_ALPHA:
            meta_deepin_cloned_widget_set_alpha(self, g_value_get_double(value));
            break;

        case PROP_SCALE_X:
            meta_deepin_cloned_widget_set_scale_x(self, g_value_get_double(value));
            break;

        case PROP_SCALE_Y:
            meta_deepin_cloned_widget_set_scale_y(self, g_value_get_double(value));
            break;

        case PROP_ROTATE:
            meta_deepin_cloned_widget_set_rotate(self, g_value_get_double(value)); 
            break;

        case PROP_TRANSLATE_X:
            meta_deepin_cloned_widget_translate_x(self, g_value_get_double(value)); 
            break;

        case PROP_TRANSLATE_Y:
            meta_deepin_cloned_widget_translate_y(self, g_value_get_double(value)); 
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void meta_deepin_cloned_widget_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    MetaDeepinClonedWidget* self = META_DEEPIN_CLONED_WIDGET(object);
    MetaDeepinClonedWidgetPrivate* priv = self->priv;

    switch (property_id)
    {
        case PROP_ALPHA:
            g_value_set_double(value, priv->alpha); break;

        case PROP_SCALE_X:
            g_value_set_double(value, priv->scale_x); break;

        case PROP_SCALE_Y:
            g_value_set_double(value, priv->scale_y); break;

        case PROP_ROTATE:
            g_value_set_double(value, priv->angle); break;

        case PROP_TRANSLATE_X:
            g_value_set_double(value, priv->tx); break;

        case PROP_TRANSLATE_Y:
            g_value_set_double(value, priv->ty); break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void meta_deepin_cloned_widget_dispose(GObject *object)
{
    MetaDeepinClonedWidget *self = META_DEEPIN_CLONED_WIDGET(object);
    MetaDeepinClonedWidgetPrivate* priv = self->priv;

    if (priv->snapshot) {
        g_clear_pointer(&priv->snapshot, cairo_surface_destroy);
    }

    G_OBJECT_CLASS(meta_deepin_cloned_widget_parent_class)->dispose(object);
}

static void meta_deepin_cloned_widget_finalize(GObject *object)
{
    MetaDeepinClonedWidget *head = META_DEEPIN_CLONED_WIDGET(object);
    G_GNUC_UNUSED MetaDeepinClonedWidgetPrivate* priv = head->priv;

    G_OBJECT_CLASS(meta_deepin_cloned_widget_parent_class)->finalize(object);
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

static gboolean meta_deepin_cloned_widget_draw (GtkWidget *widget, cairo_t* cr)
{
    if (!gtk_widget_get_visible(widget)) return FALSE;

    MetaDeepinClonedWidget *self = META_DEEPIN_CLONED_WIDGET (widget);
    MetaDeepinClonedWidgetPrivate* priv = self->priv;

    GtkStyleContext* context = gtk_widget_get_style_context (widget);

    GtkAllocation clip;
    gtk_widget_get_clip(widget, &clip);

    GtkRequisition req;
    gtk_widget_get_preferred_size(widget, &req, NULL);
    /*g_message("----- req(%d, %d), clip(%d, %d, %d, %d)", req.width, req.height, */
    /*clip.x, clip.y, clip.width, clip.height);*/

    gdouble x = 0, y = 0,
            w = req.width, h = req.height,
            cw = clip.width, ch = clip.height;

    GtkBorder borders;

    if (priv->selected) {
        gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);
    } else {
        gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
    }
    _style_get_borders(context, &borders);
    /*w += borders.left + borders.right;*/

/*#define META_UI_DEBUG*/
#ifdef META_UI_DEBUG
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_rectangle(cr, -(cw-w)/2, -(ch-h)/2, cw, ch);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0, 1, 0);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_fill(cr);
#endif


    gdouble w2 = w * priv->pivot_x, h2 = h * priv->pivot_y;
    cairo_translate(cr, w2, h2);

    gdouble pos = priv->animation ? priv->current_pos : 1.0;
    gdouble sx = priv->ai.scale_x * pos + priv->scale_x * (1.0 - pos),
            sy = priv->ai.scale_y * pos + priv->scale_y * (1.0 - pos);
    if (!priv->animation) {
        sx = priv->scale_x, sy = priv->scale_y;
    }
    cairo_scale(cr, sx, sy);

    gdouble alpha = priv->ai.alpha * pos + priv->alpha * (1.0 - pos);

    x = w/2, y = h/2;
    if (priv->render_background) {
        gtk_render_background(context, cr, -x, -y, w, h);
    }

    if (priv->render_frame) {
        x += borders.left;
        y += borders.top;
        gtk_render_frame(context, cr, -x, -y, w, h);
    }

    gdouble d = priv->ai.blur_radius * pos + priv->blur_radius * (1.0 - pos);
    if (!priv->animation) d = priv->blur_radius;
    if (d > 0.0) {
        x = cairo_image_surface_get_width(priv->snapshot) / 2.0,
          y = cairo_image_surface_get_height(priv->snapshot) / 2.0;

        cairo_surface_t* dest = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                cairo_image_surface_get_width(priv->snapshot),
                cairo_image_surface_get_height(priv->snapshot));

        cairo_t* cr2 = cairo_create(dest);
        cairo_set_source_surface(cr2, priv->snapshot, 0, 0);
        cairo_paint(cr2);
        cairo_destroy(cr2);

        stack_blur_surface(dest, d);

        cairo_set_source_surface(cr, dest, -x, -y);
        cairo_paint_with_alpha(cr, alpha);
        cairo_surface_destroy(dest);

    } else {
        x = cairo_image_surface_get_width(priv->snapshot) / 2.0,
          y = cairo_image_surface_get_height(priv->snapshot) / 2.0;
        cairo_set_source_surface(cr, priv->snapshot, -x, -y);
        cairo_paint_with_alpha(cr, alpha);
    }

    return TRUE;
}

static void meta_deepin_cloned_widget_end_animation(MetaDeepinClonedWidget* self)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    g_assert(priv->animation == TRUE);
    g_assert(priv->tick_id != 0);

    gtk_widget_remove_tick_callback(GTK_WIDGET(self), priv->tick_id);
    priv->tick_id = 0;
    priv->animation = FALSE;
    priv->current_pos = priv->target_pos = 0;

    priv->tx = priv->ai.tx;
    priv->ty = priv->ai.ty;
    priv->scale_x = priv->ai.scale_x;
    priv->scale_y = priv->ai.scale_y;
    priv->angle = priv->ai.angle;
    priv->blur_radius = priv->ai.blur_radius;

    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static gboolean on_tick_callback(MetaDeepinClonedWidget* self, GdkFrameClock* clock, 
        gpointer data)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;

    gint64 now = gdk_frame_clock_get_frame_time(clock);

    gdouble duration = (now - priv->last_time) / 1000000.0;
    if (priv->last_time != priv->start_time && duration < 0.03) return G_SOURCE_CONTINUE;
    priv->last_time = now;

    gdouble t = 1.0;
    if (now < priv->end_time) {
        t = (now - priv->start_time) / (gdouble)(priv->end_time - priv->start_time);
    }
    t = ease_in_out_quad(t);
    priv->current_pos = t * priv->target_pos;
    if (priv->current_pos > priv->target_pos) priv->current_pos = priv->target_pos;
    gtk_widget_queue_draw(GTK_WIDGET(self));

    if (priv->current_pos >= priv->target_pos) {
        meta_deepin_cloned_widget_end_animation(self);
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

static inline gint fast_round(double x) 
{
    return (gint)(x + 0.5);
}

static void meta_deepin_cloned_widget_get_preferred_width (GtkWidget *widget,
        gint      *minimum_width,
        gint      *natural_width)
{
    GTK_WIDGET_CLASS (meta_deepin_cloned_widget_parent_class)->get_preferred_width (
            widget, minimum_width, natural_width); 

    MetaDeepinClonedWidgetPrivate* priv = META_DEEPIN_CLONED_WIDGET(widget)->priv;
    *minimum_width = priv->real_size.width;
    *natural_width = priv->real_size.width;
}

static void meta_deepin_cloned_widget_get_preferred_height_for_width(GtkWidget *widget,
        gint width, gint* minimum_height_out, gint* natural_height_out)
{
    MetaDeepinClonedWidgetPrivate* priv = META_DEEPIN_CLONED_WIDGET(widget)->priv;

    GTK_WIDGET_CLASS(meta_deepin_cloned_widget_parent_class)->get_preferred_height_for_width(
            widget, width, minimum_height_out, natural_height_out);
}

static void meta_deepin_cloned_widget_get_preferred_height (GtkWidget *widget,
        gint      *minimum_height,
        gint      *natural_height)
{
    GTK_WIDGET_CLASS (meta_deepin_cloned_widget_parent_class)->get_preferred_height (
            widget, minimum_height, natural_height);

    MetaDeepinClonedWidgetPrivate* priv = META_DEEPIN_CLONED_WIDGET(widget)->priv;
    *minimum_height = priv->real_size.height;
    *natural_height = priv->real_size.height;
}

static void meta_deepin_cloned_widget_get_preferred_width_for_height(GtkWidget *widget,
        gint height, gint* minimum_width_out, gint* natural_width_out)
{
    GTK_WIDGET_CLASS(meta_deepin_cloned_widget_parent_class)->get_preferred_width_for_height(
            widget, height, minimum_width_out, natural_width_out);
}

static cairo_surface_t* get_window_surface(MetaWindow* window)
{
    Pixmap pixmap;

    pixmap = meta_compositor_get_window_pixmap (window->display->compositor,
            window);

    if (pixmap == None) return NULL;

    Display *display;
    Window root;
    int x, y;
    unsigned int width, height, border, depth;
    GdkVisual *visual;
    cairo_surface_t *surface;

    display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    if (!XGetGeometry(display, pixmap, &root, &x, &y, &width, &height, &border, &depth))
        return NULL;

    visual = gdk_screen_get_rgba_visual (gdk_screen_get_default());
    surface = cairo_xlib_surface_create (display, pixmap,
            GDK_VISUAL_XVISUAL (visual), width, height);

    return surface;
}

static void meta_deepin_cloned_widget_size_allocate(GtkWidget* widget, 
        GtkAllocation* allocation)
{
    MetaDeepinClonedWidgetPrivate* priv = META_DEEPIN_CLONED_WIDGET(widget)->priv;
    gtk_widget_set_allocation(widget, allocation);

    /* FIXME: calculate expaned according to scale, translate, and rotate */
    GtkAllocation expanded;

    gdouble sx, sy;
    if (priv->animation) {
        sx = MAX(priv->ai.scale_x, 1.0), sy = MAX(priv->ai.scale_y, 1.0);
    } else sx = MAX(priv->scale_x, 1.0), sy = MAX(priv->scale_y, 1.0);

    expanded.width = fast_round(allocation->width * sx);
    expanded.height = fast_round(allocation->height * sy);
    expanded.x = allocation->x - allocation->width * (sx - 1.0) / 2.0;
    expanded.y = allocation->y - allocation->height * (sy - 1.0) / 2.0;
    gtk_widget_set_clip(widget, &expanded);
}

static void meta_deepin_cloned_widget_init (MetaDeepinClonedWidget *self)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv =
        (MetaDeepinClonedWidgetPrivate*) meta_deepin_cloned_widget_get_instance_private (self);
    priv->animation_duration = SWITCHER_PREVIEW_DURATION;
    priv->scale_x = 1.0;
    priv->scale_y = 1.0;
    priv->pivot_x = 0.5;
    priv->pivot_y = 0.5;

    priv->ai.scale_x = priv->scale_x;
    priv->ai.scale_y = priv->scale_y;

    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);
}

static void meta_deepin_cloned_widget_class_init (MetaDeepinClonedWidgetClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    widget_class->draw = meta_deepin_cloned_widget_draw;
    widget_class->get_preferred_width = meta_deepin_cloned_widget_get_preferred_width;
    widget_class->get_preferred_height_for_width = meta_deepin_cloned_widget_get_preferred_height_for_width;
    widget_class->get_preferred_height = meta_deepin_cloned_widget_get_preferred_height;
    widget_class->get_preferred_width_for_height = meta_deepin_cloned_widget_get_preferred_width_for_height;
    widget_class->size_allocate = meta_deepin_cloned_widget_size_allocate;

    gobject_class->set_property = meta_deepin_cloned_widget_set_property;
    gobject_class->get_property = meta_deepin_cloned_widget_get_property;
    gobject_class->dispose = meta_deepin_cloned_widget_dispose;
    gobject_class->finalize = meta_deepin_cloned_widget_finalize;

    property_specs[PROP_ALPHA] = g_param_spec_double(
            "alpha", "alpha", "alpha", 
            0.0, 1.0, 1.0,
            G_PARAM_READWRITE);

    property_specs[PROP_SCALE_X] = g_param_spec_double(
            "scale-x", "scale of x", "scale of x",
            0.0, 1.0, 1.0,
            G_PARAM_READWRITE);

    property_specs[PROP_SCALE_Y] = g_param_spec_double(
            "scale-y", "scale of y", "scale of y",
            0.0, 1.0, 1.0,
            G_PARAM_READWRITE);

    property_specs[PROP_ROTATE] = g_param_spec_double(
            "angle", "rotation", "rotation",
            -DBL_MAX, DBL_MAX, 0.0,
            G_PARAM_READWRITE);

    property_specs[PROP_TRANSLATE_X] = g_param_spec_double(
            "translate-x", "translate of x", "translate of x",
            -DBL_MAX, DBL_MAX, 0.0,
            G_PARAM_READWRITE);

    property_specs[PROP_TRANSLATE_Y] = g_param_spec_double(
            "translate-y", "translate of y", "translate of y",
            -DBL_MAX, DBL_MAX, 0.0,
            G_PARAM_READWRITE);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, property_specs);
}

GtkWidget * meta_deepin_cloned_widget_new (MetaWindow* meta)
{
    MetaDeepinClonedWidget* widget;

    widget = (MetaDeepinClonedWidget*)g_object_new (META_TYPE_DEEPIN_CLONED_WIDGET, NULL);
    widget->priv->meta_window = meta;
    deepin_setup_style_class(GTK_WIDGET(widget), "deepin-window-clone");

    return (GtkWidget*)widget;
}

static void meta_deepin_cloned_widget_prepare_animation(MetaDeepinClonedWidget* self) 
{
    if (!gtk_widget_get_realized(GTK_WIDGET(self))) {
        gtk_widget_realize(GTK_WIDGET(self));
    }

    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    if (priv->tick_id) {
        meta_deepin_cloned_widget_end_animation(self);
    }

    priv->target_pos = 1.0;
    priv->current_pos = 0.0;

    priv->start_time = gdk_frame_clock_get_frame_time(
            gtk_widget_get_frame_clock(GTK_WIDGET(self)));
    priv->last_time = priv->start_time;
    priv->end_time = priv->start_time + (priv->animation_duration * 1000);

    priv->tick_id = gtk_widget_add_tick_callback(GTK_WIDGET(self),
            (GtkTickCallback)on_tick_callback, 0, 0);

    priv->animation = TRUE;
}

void meta_deepin_cloned_widget_select (MetaDeepinClonedWidget *self)
{
    if (self->priv->animation) {
        meta_deepin_cloned_widget_end_animation(self);
    }
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    priv->selected = TRUE;
    meta_deepin_cloned_widget_pop_state(self);
}

void meta_deepin_cloned_widget_unselect (MetaDeepinClonedWidget *self)
{
    if (self->priv->animation) {
        meta_deepin_cloned_widget_end_animation(self);
    }
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    priv->selected = FALSE;
    meta_deepin_cloned_widget_pop_state(self);
}

void meta_deepin_cloned_widget_set_scale(MetaDeepinClonedWidget* self, 
        gdouble sx, gdouble sy)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    sx = MAX(sx, 0.0), sy = MAX(sy, 0.0);

    if (priv->animation) {
        meta_deepin_cloned_widget_end_animation(self);
    }

    if (priv->animation_stack) {
        priv->ai.scale_x = sx;
        priv->ai.scale_y = sy;
    } else {
        priv->scale_x = sx;
        priv->scale_y = sy;
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
}

void meta_deepin_cloned_widget_set_scale_x(MetaDeepinClonedWidget* self, gdouble val)
{
    meta_deepin_cloned_widget_set_scale(self, val, self->priv->scale_y);
}

void meta_deepin_cloned_widget_set_scale_y(MetaDeepinClonedWidget* self, gdouble val)
{
    meta_deepin_cloned_widget_set_scale(self, self->priv->scale_x, val);
}

void meta_deepin_cloned_widget_get_scale(MetaDeepinClonedWidget* self,
        gdouble* sx, gdouble* sy)
{
    if (sx) *sx = self->priv->scale_x;
    if (sy) *sy = self->priv->scale_y;
}

void meta_deepin_cloned_widget_set_rotate(MetaDeepinClonedWidget* self, gdouble angle)
{
    if (self->priv->animation) {
        meta_deepin_cloned_widget_end_animation(self);
    }
    
    if (self->priv->animation_stack) 
        self->priv->ai.angle = angle;
    else {
        self->priv->angle = angle;
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
}

gdouble meta_deepin_cloned_widget_get_rotate(MetaDeepinClonedWidget* self)
{
    return self->priv->angle;
}

void meta_deepin_cloned_widget_translate(MetaDeepinClonedWidget* self,
        gdouble tx, gdouble ty)
{
    meta_deepin_cloned_widget_translate_x(self, tx);
    meta_deepin_cloned_widget_translate_y(self, ty);
}

void meta_deepin_cloned_widget_get_translate(MetaDeepinClonedWidget* self,
        gdouble* tx, gdouble* ty)
{
    if (tx) *tx = self->priv->tx;
    if (ty) *ty = self->priv->ty;
}

void meta_deepin_cloned_widget_translate_x(MetaDeepinClonedWidget* self, gdouble tx)
{
    if (self->priv->animation) {
        meta_deepin_cloned_widget_end_animation(self);
    }

    if (self->priv->animation_stack) 
        self->priv->ai.tx = tx;
    else {
        self->priv->tx = tx;
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
}

void meta_deepin_cloned_widget_translate_y(MetaDeepinClonedWidget* self, gdouble ty)
{
    if (self->priv->animation) {
        meta_deepin_cloned_widget_end_animation(self);
    }
    if (self->priv->animation_stack) 
        self->priv->ai.ty = ty;
    else {
        self->priv->ty = ty;
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
}

void meta_deepin_cloned_widget_set_blur_radius(MetaDeepinClonedWidget* self, gdouble val)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    if (priv->animation) {
        meta_deepin_cloned_widget_end_animation(self);
    }

    if (priv->animation_stack) {
        priv->ai.blur_radius = MAX(val, 0.0);
    } else {
        priv->blur_radius = MAX(val, 0.0);
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
}

void meta_deepin_cloned_widget_set_size(MetaDeepinClonedWidget* self,
        gdouble width, gdouble height)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;

    if (priv->snapshot) {
        g_clear_pointer(&priv->snapshot, cairo_surface_destroy);
    }

    cairo_surface_t* ws = get_window_surface(priv->meta_window);
    MetaRectangle r;
    meta_window_get_outer_rect(priv->meta_window, &r);

    priv->snapshot = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            width, height);
    cairo_t* cr = cairo_create(priv->snapshot);
    cairo_scale(cr, (double)width/r.width, (double)height/r.height);
    cairo_set_source_surface(cr, ws, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(ws);

    priv->real_size.width = width;
    priv->real_size.height = height;
}

void meta_deepin_cloned_widget_push_state(MetaDeepinClonedWidget* self)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    priv->animation_stack++;

    priv->ai.tx = priv->tx;
    priv->ai.ty = priv->ty;
    priv->ai.scale_x = priv->scale_x;
    priv->ai.scale_y = priv->scale_y;
    priv->ai.angle = priv->angle;
    priv->ai.blur_radius = priv->blur_radius;
}

void meta_deepin_cloned_widget_pop_state(MetaDeepinClonedWidget* self)
{
    if (self->priv->animation_stack <= 0) {
        if (self->priv->animation) {
            meta_deepin_cloned_widget_end_animation(self);
        } else {
            gtk_widget_queue_draw(GTK_WIDGET(self));
        }
        return;
    } 
    self->priv->animation_stack--;
    if (self->priv->animation_stack == 0) {
        meta_deepin_cloned_widget_prepare_animation(self);
    }
}

cairo_surface_t* meta_deepin_cloned_widget_get_snapshot(MetaDeepinClonedWidget* self)
{
    return self->priv->snapshot;
}

void meta_deepin_cloned_widget_set_alpha(MetaDeepinClonedWidget* self, gdouble val)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    if (priv->animation) {
        meta_deepin_cloned_widget_end_animation(self);
    }

    val = MIN(MAX(val, 0.0), 1.0);
    if (priv->animation_stack) {
        priv->ai.alpha = val;
    } else {
        priv->alpha = val;
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
}

gdouble meta_deepin_cloned_widget_get_alpha(MetaDeepinClonedWidget* self)
{
    return self->priv->alpha;
}

