/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */


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
#include <util.h>
#include <string.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include "deepin-window-surface-manager.h"
#include "deepin-cloned-widget.h"
#include "deepin-design.h"
#include "deepin-stackblur.h"
#include "deepin-ease.h"
#include "deepin-shadow-workspace.h"
#include "deepin-message-hub.h"
#include "deepin-shadow-workspace.h"

#define ICON_SIZE 64

typedef struct _MetaDeepinClonedWidgetPrivate
{
    gboolean selected;

    gdouble pivot_x, pivot_y; /* around which to scale and rotate */

    gdouble tx, ty;  /* translation */
    gdouble scale_x, scale_y;   /* scale */
    gdouble angle;   /* rotation, clockwise is negative */
    gdouble blur_radius;
    gdouble alpha;

    int render_background: 1;
    int render_frame: 1;
    int mouse_over: 1;
    int dragging: 1;
    int draggable: 1;
    int show_icon: 1;

    gdouble dx, dy; // press hold coord

    MetaWindow* meta_window;
    cairo_surface_t* snapshot;
    cairo_surface_t* icon;

    GtkRequisition real_size;

    GdkWindow* event_window;
} MetaDeepinClonedWidgetPrivate;

enum {
    PROP_0,
    PROP_ALPHA,
    PROP_SCALE_X,
    PROP_SCALE_Y,
    PROP_ROTATE,
    PROP_TRANSLATE_X,
    PROP_TRANSLATE_Y,
    PROP_SHOW_ICON,
    N_PROPERTIES
};

enum
{
    SIGNAL_TRANSITION_FINISHED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

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
            
        case PROP_SHOW_ICON:
            self->priv->show_icon = g_value_get_boolean(value);
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

        case PROP_SHOW_ICON:
            g_value_set_boolean(value, priv->show_icon); break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void meta_deepin_cloned_widget_dispose(GObject *object)
{
    MetaDeepinClonedWidget *self = META_DEEPIN_CLONED_WIDGET(object);
    MetaDeepinClonedWidgetPrivate* priv = self->priv;

    priv->meta_window = NULL;
    if (priv->snapshot) {
        g_clear_pointer(&priv->snapshot, cairo_surface_destroy);
    }

    if (priv->icon) {
        g_clear_pointer(&priv->icon, cairo_surface_destroy);
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
    MetaDeepinClonedWidget *self = META_DEEPIN_CLONED_WIDGET (widget);
    MetaDeepinClonedWidgetPrivate* priv = self->priv;

    GtkStyleContext* context = gtk_widget_get_style_context (widget);

    GtkRequisition req;
    gtk_widget_get_preferred_size(widget, &req, NULL);

    gdouble x = 0, y = 0, w = req.width, h = req.height;

    GtkBorder borders;
    _style_get_borders(context, &borders);
    /*w += borders.left + borders.right;*/

    if (priv->meta_window->type == META_WINDOW_DESKTOP) {
        return TRUE;
    }

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

    gdouble sx = priv->scale_x, sy = priv->scale_y;
    cairo_scale(cr, sx, sy);

    gdouble alpha = priv->alpha;

    x = w/2, y = h/2;
    if (priv->render_background) {
        gtk_render_background(context, cr, -x, -y, w, h);
    }

    if (priv->render_frame) {
        x += borders.left;
        y += borders.top;
        gdouble fw = w + borders.left + borders.right;
        gdouble fh = h + borders.top + borders.bottom;
        gtk_render_frame(context, cr, -x, -y, fw, fh);
    }

    if (priv->meta_window->unmanaging || !priv->snapshot) return TRUE;

    gdouble d = priv->blur_radius;
    if (d > 0.0) {
        x = cairo_image_surface_get_width(priv->snapshot) / 2.0,
          y = cairo_image_surface_get_height(priv->snapshot) / 2.0;

        cairo_surface_t* dest = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
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

    if (priv->icon) {
        x = -ICON_SIZE/2;
        y = (h/2 - ICON_SIZE * 0.85f);

        cairo_set_source_surface(cr, priv->icon, x, y);
        cairo_paint(cr);
    }
    return TRUE;
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

static void meta_deepin_cloned_widget_size_allocate(GtkWidget* widget, 
        GtkAllocation* allocation)
{
    MetaDeepinClonedWidgetPrivate* priv = META_DEEPIN_CLONED_WIDGET(widget)->priv;
    gtk_widget_set_allocation(widget, allocation);

    if (gtk_widget_get_realized (widget)) {
        /*
         * HACK: we make its event_window a child of workspace's event_window,
         * so adjust event window's position accordingly 
         */
        GtkWidget* parent = gtk_widget_get_parent(widget);
        GtkAllocation parent_alloc;
        gtk_widget_get_allocation(parent, &parent_alloc);

        if (!DEEPIN_IS_SHADOW_WORKSPACE(parent)) {
            parent_alloc.x = 0;
            parent_alloc.y = 0;
        }

        gdk_window_move_resize (priv->event_window,
                allocation->x - parent_alloc.x,
                allocation->y - parent_alloc.y,
                allocation->width,
                allocation->height);
    }

    /* FIXME: calculate expaned according to scale, translate, and rotate */
    GtkAllocation expanded;

    gdouble sx, sy;
    sx = MAX(priv->scale_x, 1.0);
    sy = MAX(priv->scale_y, 1.0);

    /* FIXME: dirty: need to dynamically adjust best clipping */
    GtkStyleContext* context = gtk_widget_get_style_context (widget);
    GtkBorder box;
    _style_get_borders(context, &box);

    sx = MAX(1.033, sx);
    sy = MAX(1.033, sy);

    expanded.width = fast_round(allocation->width * sx) + box.left + box.right;
    expanded.height = fast_round(allocation->height * sy) + box.top + box.bottom;
    expanded.x = allocation->x - allocation->width * (sx - 1.0) / 2.0 - box.left;
    expanded.y = allocation->y - allocation->height * (sy - 1.0) / 2.0 - box.top;
    gtk_widget_set_clip(widget, &expanded);
}

static void meta_deepin_cloned_widget_init (MetaDeepinClonedWidget *self)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv =
        (MetaDeepinClonedWidgetPrivate*) meta_deepin_cloned_widget_get_instance_private (self);
    priv->scale_x = 1.0;
    priv->scale_y = 1.0;
    priv->pivot_x = 0.5;
    priv->pivot_y = 0.5;
    priv->alpha = 1.0;

    gtk_style_context_set_state (gtk_widget_get_style_context(GTK_WIDGET(self)), 
            GTK_STATE_FLAG_NORMAL);

    gtk_widget_set_sensitive(GTK_WIDGET(self), TRUE);
    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);
    gtk_widget_set_app_paintable(GTK_WIDGET(self), TRUE);
}

static void meta_deepin_cloned_widget_realize (GtkWidget *widget)
{
    MetaDeepinClonedWidget *self = META_DEEPIN_CLONED_WIDGET (widget);
    MetaDeepinClonedWidgetPrivate *priv = self->priv;
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
            GDK_POINTER_MOTION_MASK |
            GDK_BUTTON_RELEASE_MASK |
            GDK_ENTER_NOTIFY_MASK |
            GDK_LEAVE_NOTIFY_MASK);

    attributes_mask = GDK_WA_X | GDK_WA_Y;

    window = gtk_widget_get_parent_window (widget);
    gtk_widget_set_window (widget, window);
    g_object_ref (window);

    GtkWidget* parent = gtk_widget_get_parent(widget);
    if (DEEPIN_IS_SHADOW_WORKSPACE(parent)) {
        window = deepin_shadow_workspace_get_event_window(parent);
    }

    priv->event_window = gdk_window_new (window,
            &attributes, attributes_mask);
    gtk_widget_register_window (widget, priv->event_window);
}

static void meta_deepin_cloned_widget_unrealize (GtkWidget *widget)
{
    MetaDeepinClonedWidget *self = META_DEEPIN_CLONED_WIDGET (widget);
    MetaDeepinClonedWidgetPrivate *priv = self->priv;

    if (priv->event_window) {
        gtk_widget_unregister_window (widget, priv->event_window);
        gdk_window_destroy (priv->event_window);
        priv->event_window = NULL;
    }

    GTK_WIDGET_CLASS (meta_deepin_cloned_widget_parent_class)->unrealize (widget);
}

static void meta_deepin_cloned_widget_map (GtkWidget *widget)
{
    MetaDeepinClonedWidget *self = META_DEEPIN_CLONED_WIDGET (widget);
    MetaDeepinClonedWidgetPrivate *priv = self->priv;

    GTK_WIDGET_CLASS (meta_deepin_cloned_widget_parent_class)->map (widget);

    if (priv->event_window)
        gdk_window_show (priv->event_window);
}

static void meta_deepin_cloned_widget_unmap (GtkWidget *widget)
{
    MetaDeepinClonedWidget *self = META_DEEPIN_CLONED_WIDGET (widget);
    MetaDeepinClonedWidgetPrivate *priv = self->priv;

    if (priv->event_window) {
        gdk_window_hide (priv->event_window);
    }

    GTK_WIDGET_CLASS (meta_deepin_cloned_widget_parent_class)->unmap (widget);
}

static void meta_deepin_cloned_widget_class_init (MetaDeepinClonedWidgetClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    widget_class->draw = meta_deepin_cloned_widget_draw;
    widget_class->get_preferred_width = meta_deepin_cloned_widget_get_preferred_width;
    widget_class->get_preferred_height = meta_deepin_cloned_widget_get_preferred_height;
    widget_class->size_allocate = meta_deepin_cloned_widget_size_allocate;
    widget_class->realize = meta_deepin_cloned_widget_realize;
    widget_class->unrealize = meta_deepin_cloned_widget_unrealize;
    widget_class->map = meta_deepin_cloned_widget_map;
    widget_class->unmap = meta_deepin_cloned_widget_unmap;

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

    property_specs[PROP_SHOW_ICON] = g_param_spec_boolean(
            "show-icon", "show icon", "show icon",
            FALSE,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, property_specs);


    signals[SIGNAL_TRANSITION_FINISHED] = g_signal_new ("transition-finished",
            META_TYPE_DEEPIN_CLONED_WIDGET,
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 0, NULL);
}

static void on_deepin_cloned_widget_drag_data_get(GtkWidget* widget, GdkDragContext* context,
        GtkSelectionData* data, guint info, guint time, gpointer user_data)
{
    static GdkAtom atom_window = GDK_NONE;
    
    if (atom_window == GDK_NONE) 
        atom_window = gdk_atom_intern("window", FALSE);
    g_assert(atom_window != GDK_NONE);

    gchar* raw_data = g_strdup_printf("%ld", widget);

    meta_verbose("%s: set data %x\n", __func__, widget);
    gtk_selection_data_set(data, atom_window, 8, raw_data, strlen(raw_data));
    g_free(raw_data);
}

static void on_deepin_cloned_widget_drag_begin(GtkWidget* widget, GdkDragContext *context,
               gpointer user_data)
{
    meta_verbose("%s\n", __func__);
    MetaDeepinClonedWidgetPrivate* priv = META_DEEPIN_CLONED_WIDGET(widget)->priv;

    gint w = cairo_image_surface_get_width(priv->snapshot); 
    gint h = cairo_image_surface_get_height(priv->snapshot); 

    float sx = RECT_PREFER_WIDTH / (float)w;
    w *= sx; 
    h *= sx;
    cairo_surface_t* dest = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);

    cairo_t* cr2 = cairo_create(dest);
    cairo_scale(cr2, sx, sx);
    cairo_set_source_surface(cr2, priv->snapshot, 0, 0);
    cairo_paint_with_alpha(cr2, 0.7);
    cairo_destroy(cr2);

    cairo_surface_set_device_offset(dest, -w/2 , -h/2);
    gtk_drag_set_icon_surface(context, dest);

    gtk_widget_set_opacity(widget, 0.0);
    cairo_surface_destroy(dest);

    priv->dragging = TRUE;
}

static void on_deepin_cloned_widget_drag_end(GtkWidget* widget, GdkDragContext *context,
               gpointer user_data)
{
    meta_verbose("%s", __func__);
    MetaDeepinClonedWidgetPrivate* priv = META_DEEPIN_CLONED_WIDGET(widget)->priv;
    gtk_widget_set_opacity(widget, 1.0);

    priv->dragging = FALSE;

    //HACK: drag broken the grab, need to restore here
    deepin_message_hub_drag_end();
}
    
static gboolean on_deepin_cloned_widget_drag_failed(GtkWidget      *widget,
               GdkDragContext *context, GtkDragResult   result,
               gpointer        user_data)
{
    /* cut off default processing (fail animation), which may 
     * case a confliction when we regrab pointer later */
    meta_verbose ("%s\n", __func__);
    return TRUE;
}

GtkWidget * meta_deepin_cloned_widget_new (MetaWindow* meta, gboolean show_icon)
{
    MetaDeepinClonedWidget* widget;

    widget = (MetaDeepinClonedWidget*)g_object_new (META_TYPE_DEEPIN_CLONED_WIDGET, "show-icon", show_icon, NULL);
    widget->priv->meta_window = meta;
    deepin_setup_style_class(GTK_WIDGET(widget), "deepin-window-clone");

    g_object_connect(G_OBJECT(widget), 
            "signal::drag-data-get", on_deepin_cloned_widget_drag_data_get, NULL,
            "signal::drag-begin", on_deepin_cloned_widget_drag_begin, NULL,
            "signal::drag-end", on_deepin_cloned_widget_drag_end, NULL,
            "signal::drag-failed", on_deepin_cloned_widget_drag_failed, NULL,
            NULL);

    if (show_icon) {
        GdkPixbuf* pixbuf = meta_window_get_application_icon(meta, ICON_SIZE);
        widget->priv->icon = gdk_cairo_surface_create_from_pixbuf(pixbuf, 1.0, NULL);
        g_object_unref(pixbuf);
    }

    return (GtkWidget*)widget;
}

void meta_deepin_cloned_widget_select (MetaDeepinClonedWidget *self)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    priv->selected = TRUE;
    gtk_style_context_set_state (gtk_widget_get_style_context(GTK_WIDGET(self)), 
            GTK_STATE_FLAG_SELECTED);
}

void meta_deepin_cloned_widget_unselect (MetaDeepinClonedWidget *self)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    priv->selected = FALSE;
    gtk_style_context_set_state (gtk_widget_get_style_context(GTK_WIDGET(self)), 
            GTK_STATE_FLAG_NORMAL);
}

void meta_deepin_cloned_widget_set_scale(MetaDeepinClonedWidget* self, 
        gdouble sx, gdouble sy)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    sx = MAX(sx, 0.0), sy = MAX(sy, 0.0);

    priv->scale_x = sx;
    priv->scale_y = sy;
    gtk_widget_queue_draw(GTK_WIDGET(self));
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
    MetaDeepinClonedWidgetPrivate* priv = self->priv;

    gdouble px = priv->scale_x;
    gdouble py = priv->scale_y;

    if (sx) *sx = px;
    if (sy) *sy = py;
}

void meta_deepin_cloned_widget_set_rotate(MetaDeepinClonedWidget* self, gdouble angle)
{
    self->priv->angle = angle;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

gdouble meta_deepin_cloned_widget_get_rotate(MetaDeepinClonedWidget* self)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    return priv->angle;
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
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    gdouble px = priv->tx;
    gdouble py = priv->ty;

    if (tx) *tx = px;
    if (ty) *ty = py;
}

void meta_deepin_cloned_widget_translate_x(MetaDeepinClonedWidget* self, gdouble tx)
{
    self->priv->tx = tx;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void meta_deepin_cloned_widget_translate_y(MetaDeepinClonedWidget* self, gdouble ty)
{
    self->priv->ty = ty;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void meta_deepin_cloned_widget_set_blur_radius(MetaDeepinClonedWidget* self, gdouble val)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    priv->blur_radius = MAX(val, 0.0);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void meta_deepin_cloned_widget_set_size(MetaDeepinClonedWidget* self,
        gdouble width, gdouble height)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;

    MetaRectangle r;
    meta_window_get_outer_rect(priv->meta_window, &r);

    priv->snapshot = deepin_window_surface_manager_get_surface(
            priv->meta_window, (double)width/r.width);
    if (priv->snapshot) cairo_surface_reference(priv->snapshot);

    priv->real_size.width = width;
    priv->real_size.height = height;

    /* reset */
    priv->scale_x = 1.0;
    priv->scale_y = 1.0;
    priv->alpha = 1.0;
    priv->angle = 0.0;
    priv->tx = 0;
    priv->ty = 0;
    priv->blur_radius = 0;

    gtk_widget_queue_resize(GTK_WIDGET(self));
}

void meta_deepin_cloned_widget_set_alpha(MetaDeepinClonedWidget* self, gdouble val)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;

    val = MIN(MAX(val, 0.0), 1.0);
    priv->alpha = val;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

gdouble meta_deepin_cloned_widget_get_alpha(MetaDeepinClonedWidget* self)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    return priv->alpha;
}

MetaWindow* meta_deepin_cloned_widget_get_window(MetaDeepinClonedWidget* self)
{
    return self->priv->meta_window;
}

void meta_deepin_cloned_widget_set_render_frame(MetaDeepinClonedWidget* self,
        gboolean val)
{
    gboolean old = self->priv->render_frame;
    self->priv->render_frame = val;
    if (old != val) gtk_widget_queue_draw(GTK_WIDGET(self));
}

void meta_deepin_cloned_widget_get_size(MetaDeepinClonedWidget* self,
        gdouble* w, gdouble* h)
{
    if (w) *w = self->priv->real_size.width;
    if (h) *h = self->priv->real_size.height;
}

gboolean meta_deepin_cloned_widget_is_dragging(MetaDeepinClonedWidget* self)
{
    return self->priv->dragging;
}

GdkWindow* meta_deepin_cloned_widget_get_event_window(MetaDeepinClonedWidget* self)
{
    return self->priv->event_window;
}

void meta_deepin_cloned_widget_set_enable_drag(MetaDeepinClonedWidget* self,
        gboolean val)
{
    MetaDeepinClonedWidgetPrivate* priv = self->priv;
    if (priv->draggable != val) {
        priv->draggable = val;
        static GtkTargetEntry targets[] = {
            {(char*)"window", GTK_TARGET_OTHER_WIDGET, DRAG_TARGET_WINDOW},
        };

        if (val) {
            gtk_drag_source_set(GTK_WIDGET(self), GDK_BUTTON1_MASK, targets, 
                    G_N_ELEMENTS(targets), GDK_ACTION_COPY);
        }
    }
}

