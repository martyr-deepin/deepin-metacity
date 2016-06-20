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
#include <gdk/gdk.h>
#include "boxes.h"
#include "../core/window-private.h"
#include "deepin-tab-widget.h"
#include "deepin-design.h"
#include "deepin-ease.h"
#include "deepin-window-surface-manager.h"
#include "deepin-background-cache.h"
#include "deepin-desktop-background.h"

#define ICON_SIZE 48

typedef struct _MetaDeepinTabWidgetPrivate
{
  gboolean selected;
  gdouble scale; /* this scale does not used for scaling animation, it only means
                    that if thumbnail gets shrinked or expanded */

  cairo_surface_t* icon;
  cairo_surface_t* snapshot;

  MetaWindow* window;
  MetaRectangle outer_rect; /* cache of window outer rect */
  GdkRectangle mon_geom; /* primary monitor geometry */

  gint disposed: 1;
  gint render_thumb: 1; /* if size is too small, do not render it */

  GtkRequisition init_size;
  GtkRequisition real_size;

  GdkWindow* event_window;
} MetaDeepinTabWidgetPrivate;

enum {
    PROP_0,
    PROP_SCALE,
    N_PROPERTIES
};

static GParamSpec* property_specs[N_PROPERTIES] = {NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (MetaDeepinTabWidget, meta_deepin_tab_widget, GTK_TYPE_WIDGET);

static void meta_deepin_tab_widget_set_property(GObject *object, guint property_id,
        const GValue *value, GParamSpec *pspec)
{
    MetaDeepinTabWidget* self = META_DEEPIN_TAB_WIDGET(object);

    switch (property_id)
    {
        case PROP_SCALE:
            meta_deepin_tab_widget_set_scale(self, g_value_get_double(value)); 
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void meta_deepin_tab_widget_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    MetaDeepinTabWidget* self = META_DEEPIN_TAB_WIDGET(object);
    MetaDeepinTabWidgetPrivate* priv = self->priv;

    switch (property_id)
    {
        case PROP_SCALE:
            g_value_set_double(value, priv->scale); break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void meta_deepin_tab_widget_dispose(GObject *object)
{
    MetaDeepinTabWidget *self = META_DEEPIN_TAB_WIDGET(object);
    MetaDeepinTabWidgetPrivate* priv = self->priv;

    if (priv->disposed) return;
    priv->disposed = TRUE;

    if (priv->icon) {
        g_clear_pointer(&priv->icon, cairo_surface_destroy);
    }

    if (priv->snapshot) {
        g_clear_pointer(&priv->snapshot, cairo_surface_destroy);
    }

    G_OBJECT_CLASS(meta_deepin_tab_widget_parent_class)->dispose(object);
}

static void meta_deepin_tab_widget_finalize(GObject *object)
{
    MetaDeepinTabWidget *head = META_DEEPIN_TAB_WIDGET(object);
    G_GNUC_UNUSED MetaDeepinTabWidgetPrivate* priv = head->priv;

    G_OBJECT_CLASS(meta_deepin_tab_widget_parent_class)->finalize(object);
}

static void _do_clip(MetaDeepinTabWidget* self, cairo_t* cr)
{
    MetaDeepinTabWidgetPrivate* priv = self->priv;

    GtkRequisition req;
    gtk_widget_get_preferred_size(GTK_WIDGET(self), &req, NULL);

    int mw = req.width * 1.033, mh = req.height * 1.033;
    cairo_rectangle_int_t r = {-(mw - req.width)/2, -(mh - req.height)/2, mw, mh};
    cairo_region_t* reg = cairo_region_create_rectangle(&r);

    r.width = cairo_image_surface_get_width(priv->snapshot);
    r.height = cairo_image_surface_get_height(priv->snapshot);
    r.x = (req.width - r.width) / 2.0;
    r.y = (req.height - r.height) / 2.0;
    cairo_region_subtract_rectangle(reg, &r);

    gdk_cairo_region(cr, reg);
    cairo_clip(cr);
    cairo_region_destroy(reg);
}

static gboolean meta_deepin_tab_widget_draw (GtkWidget *widget, cairo_t* cr)
{
  MetaDeepinTabWidget *self = META_DEEPIN_TAB_WIDGET (widget);
  MetaDeepinTabWidgetPrivate* priv = self->priv;

  GtkStyleContext* context = gtk_widget_get_style_context (widget);

  GtkRequisition req;
  gtk_widget_get_preferred_size(widget, &req, NULL);

  gdouble x, y, w = req.width, h = req.height;

  gdouble w2 = w / 2.0, h2 = h / 2.0;

  cairo_save(cr);

  /*if (priv->render_thumb && priv->snapshot) _do_clip(self, cr);*/

  cairo_translate(cr, w2, h2);

  if (priv->selected) {
      cairo_scale(cr, 1.0 + 0.033, 1.0 + 0.033);
  } else {
      cairo_scale(cr, 1.033 - 0.033, 1.033 - 0.033);
  }

  gtk_render_background(context, cr, -w2, -h2, w, h);
  cairo_restore(cr);

  MetaRectangle r = priv->outer_rect;

  int primary = gdk_screen_get_primary_monitor(gdk_screen_get_default());

  double sx = RECT_PREFER_WIDTH / (double)r.width;
  double sy = RECT_PREFER_HEIGHT / (double)r.height;
  sx = MIN(sx, sy) * priv->scale;

  if (priv->window->type == META_WINDOW_DESKTOP) {
      cairo_surface_t* ref = deepin_background_cache_get_surface(primary, sx);
      if (ref) {
          x = (w - cairo_image_surface_get_width(ref)) / 2.0,
            y = (h - cairo_image_surface_get_height(ref)) / 2.0;
          cairo_set_source_surface(cr, ref, x, y);
          cairo_paint(cr);
      }
  }

  if (priv->render_thumb && priv->snapshot && priv->window->type != META_WINDOW_DESKTOP) {
      cairo_save(cr);
      cairo_rectangle_int_t r;
      r.width = RECT_PREFER_WIDTH;
      r.height = RECT_PREFER_HEIGHT;

      r.x = (w - r.width) / 2.0;
      r.y = (h - r.height) / 2.0;

      cairo_region_t* reg = cairo_region_create_rectangle(&r);
      gdk_cairo_region(cr, reg);
      cairo_clip(cr);
      cairo_region_destroy(reg);

      x = (w - cairo_image_surface_get_width(priv->snapshot)) / 2.0;
      y = (h - cairo_image_surface_get_height(priv->snapshot)) / 2.0;
      if (priv->window->type == META_WINDOW_DESKTOP) {
          x -= priv->mon_geom.x;
          y -= priv->mon_geom.y;
      }

      cairo_set_source_surface(cr, priv->snapshot, x, y);
      cairo_paint(cr);
      cairo_restore(cr);
  }
  
  if (priv->icon) {
      double iw = cairo_image_surface_get_width(priv->icon);
      double ih = cairo_image_surface_get_height(priv->icon);

      x = (w - iw) / 2.0;
      y = (h - ih);

      if (w - SWITCHER_ITEM_SHAPE_PADDING*2 < ICON_SIZE) {
          sx = MIN(w / ICON_SIZE, h / ICON_SIZE);
          cairo_scale(cr, sx, sx);
          x = (w - iw * sx) / 2.0;
          y = h - ih * sx;
      }

      cairo_set_source_surface(cr, priv->icon, x, y);
      cairo_paint(cr);
  }

  return TRUE;
}

static inline gint fast_round(double x) 
{
    return (gint)(x + 0.5);
}

static void meta_deepin_tab_widget_get_preferred_width (GtkWidget *widget,
                                       gint      *minimum_width,
                                       gint      *natural_width)
{
  GTK_WIDGET_CLASS (meta_deepin_tab_widget_parent_class)->get_preferred_width (
          widget, minimum_width, natural_width); 
  
  MetaDeepinTabWidgetPrivate* priv = META_DEEPIN_TAB_WIDGET(widget)->priv;
  *minimum_width = priv->real_size.width;
  *natural_width = priv->real_size.width;
}

static void meta_deepin_tab_widget_get_preferred_height_for_width(GtkWidget *widget,
        gint width, gint* minimum_height_out, gint* natural_height_out)
{
    MetaDeepinTabWidgetPrivate* priv = META_DEEPIN_TAB_WIDGET(widget)->priv;

    GTK_WIDGET_CLASS(meta_deepin_tab_widget_parent_class)->get_preferred_height_for_width(
            widget, width, minimum_height_out, natural_height_out);
}

static void meta_deepin_tab_widget_get_preferred_height (GtkWidget *widget,
                                        gint      *minimum_height,
                                        gint      *natural_height)
{
  GTK_WIDGET_CLASS (meta_deepin_tab_widget_parent_class)->get_preferred_height (
          widget, minimum_height, natural_height);

  MetaDeepinTabWidgetPrivate* priv = META_DEEPIN_TAB_WIDGET(widget)->priv;
  *minimum_height = priv->real_size.height;
  *natural_height = priv->real_size.height;
}

static void meta_deepin_tab_widget_get_preferred_width_for_height(GtkWidget *widget,
        gint height, gint* minimum_width_out, gint* natural_width_out)
{
    GTK_WIDGET_CLASS(meta_deepin_tab_widget_parent_class)->get_preferred_width_for_height(
            widget, height, minimum_width_out, natural_width_out);
}

static void meta_deepin_tab_widget_size_allocate(GtkWidget* widget, 
        GtkAllocation* allocation)
{
    MetaDeepinTabWidgetPrivate* priv = META_DEEPIN_TAB_WIDGET(widget)->priv;
    gtk_widget_set_allocation(widget, allocation);

    if (gtk_widget_get_realized (widget)) {
        gdk_window_move_resize (priv->event_window,
                allocation->x,
                allocation->y,
                allocation->width,
                allocation->height);
    }

    GtkAllocation expanded;
    expanded.width = fast_round(allocation->width * 1.033);
    expanded.height = fast_round(allocation->height * 1.033);
    expanded.x = allocation->x - (expanded.width - allocation->width) / 2;
    expanded.y = allocation->y - (expanded.height - allocation->height) / 2;
    gtk_widget_set_clip(widget, &expanded);
}

static void meta_deepin_tab_widget_init (MetaDeepinTabWidget *self)
{
  self->priv = (MetaDeepinTabWidgetPrivate*)meta_deepin_tab_widget_get_instance_private (self);
  self->priv->scale = 1.0;
  self->priv->render_thumb = TRUE;
  self->priv->real_size.width = SWITCHER_ITEM_PREFER_WIDTH;
  self->priv->real_size.height = SWITCHER_ITEM_PREFER_HEIGHT;
  self->priv->init_size = self->priv->real_size;
  gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);
}

static void meta_deepin_tab_widget_realize (GtkWidget *widget)
{
    MetaDeepinTabWidget *self = META_DEEPIN_TAB_WIDGET (widget);
    MetaDeepinTabWidgetPrivate *priv = self->priv;
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
            GDK_LEAVE_NOTIFY_MASK);

    attributes_mask = GDK_WA_X | GDK_WA_Y;

    window = gtk_widget_get_parent_window (widget);
    gtk_widget_set_window (widget, window);
    g_object_ref (window);

    priv->event_window = gdk_window_new (window,
            &attributes, attributes_mask);
    gtk_widget_register_window (widget, priv->event_window);
}

static void meta_deepin_tab_widget_unrealize (GtkWidget *widget)
{
    MetaDeepinTabWidget *self = META_DEEPIN_TAB_WIDGET (widget);
    MetaDeepinTabWidgetPrivate *priv = self->priv;

    if (priv->event_window) {
        gtk_widget_unregister_window (widget, priv->event_window);
        gdk_window_destroy (priv->event_window);
        priv->event_window = NULL;
    }

    GTK_WIDGET_CLASS (meta_deepin_tab_widget_parent_class)->unrealize (widget);
}

static void meta_deepin_tab_widget_map (GtkWidget *widget)
{
    MetaDeepinTabWidget *self = META_DEEPIN_TAB_WIDGET (widget);
    MetaDeepinTabWidgetPrivate *priv = self->priv;

    GTK_WIDGET_CLASS (meta_deepin_tab_widget_parent_class)->map (widget);

    if (priv->event_window)
        gdk_window_show (priv->event_window);
}

static void meta_deepin_tab_widget_unmap (GtkWidget *widget)
{
    MetaDeepinTabWidget *self = META_DEEPIN_TAB_WIDGET (widget);
    MetaDeepinTabWidgetPrivate *priv = self->priv;

    if (priv->event_window) {
        gdk_window_hide (priv->event_window);
    }

    GTK_WIDGET_CLASS (meta_deepin_tab_widget_parent_class)->unmap (widget);
}

static void meta_deepin_tab_widget_class_init (MetaDeepinTabWidgetClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);


  widget_class->draw = meta_deepin_tab_widget_draw;
  widget_class->get_preferred_width = meta_deepin_tab_widget_get_preferred_width;
  widget_class->get_preferred_height_for_width = meta_deepin_tab_widget_get_preferred_height_for_width;
  widget_class->get_preferred_height = meta_deepin_tab_widget_get_preferred_height;
  widget_class->get_preferred_width_for_height = meta_deepin_tab_widget_get_preferred_width_for_height;
  widget_class->size_allocate = meta_deepin_tab_widget_size_allocate;

  widget_class->realize = meta_deepin_tab_widget_realize;
  widget_class->unrealize = meta_deepin_tab_widget_unrealize;
  widget_class->map = meta_deepin_tab_widget_map;
  widget_class->unmap = meta_deepin_tab_widget_unmap;

  gobject_class->set_property = meta_deepin_tab_widget_set_property;
  gobject_class->get_property = meta_deepin_tab_widget_get_property;
  gobject_class->dispose = meta_deepin_tab_widget_dispose;
  gobject_class->finalize = meta_deepin_tab_widget_finalize;

  property_specs[PROP_SCALE] = g_param_spec_double(
          "scale", "scale", "scale",
          0.0, 1.0, 1.0,
          G_PARAM_READWRITE);

  g_object_class_install_properties(gobject_class, N_PROPERTIES, property_specs);

}

GtkWidget *
meta_deepin_tab_widget_new (MetaWindow* window)
{
  MetaDeepinTabWidget* widget;

  widget = (MetaDeepinTabWidget*)g_object_new (META_TYPE_DEEPIN_TAB_WIDGET, NULL);

  widget->priv->window = window;
  meta_window_get_outer_rect(window, &widget->priv->outer_rect);

  GdkRectangle mon_geom;
  int primary = gdk_screen_get_primary_monitor(gdk_screen_get_default());
  gdk_screen_get_monitor_geometry(gdk_screen_get_default(), primary,
          &mon_geom);
  widget->priv->mon_geom = mon_geom;

  if (window->type == META_WINDOW_DESKTOP) {
      /* clip out part that resides on primary */
      widget->priv->outer_rect = *(MetaRectangle*)&mon_geom;

  } else {
      meta_verbose("WM_CLASS: %s, %s", window->res_name, window->res_class);

      /* try to load icon from res_class first, cause window->icon may
       * contain a broken one 
       **/
      GError* error = NULL;
      char* icon_name = g_ascii_strdown(window->res_class, -1);
      GdkPixbuf* icon = gtk_icon_theme_load_icon(
              gtk_icon_theme_get_default(),
              icon_name, ICON_SIZE, GTK_ICON_LOOKUP_USE_BUILTIN, &error);
      g_free(icon_name);

      if (icon == NULL) {
          if (error) {
              meta_verbose("%s", error->message);
              g_error_free(error);
          }

          icon = window->icon;
      }

      GdkPixbuf* scaled = gdk_pixbuf_scale_simple (icon,
              ICON_SIZE, ICON_SIZE, GDK_INTERP_BILINEAR);
      widget->priv->icon = gdk_cairo_surface_create_from_pixbuf(scaled,
                1.0, NULL);
      g_object_unref(scaled);
  }

  return (GtkWidget*)widget;
}

void meta_deepin_tab_widget_select (MetaDeepinTabWidget *self)
{
    MetaDeepinTabWidgetPrivate* priv = self->priv;
    priv->selected = TRUE;
    GtkStyleContext* context = gtk_widget_get_style_context (self);
    gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);

    gtk_widget_queue_draw (GTK_WIDGET (self));
}

void meta_deepin_tab_widget_unselect (MetaDeepinTabWidget *self)
{
    MetaDeepinTabWidgetPrivate* priv = self->priv;
    priv->selected = FALSE;
    GtkStyleContext* context = gtk_widget_get_style_context (self);
    gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);

    gtk_widget_queue_draw (GTK_WIDGET (self));
}

void meta_deepin_tab_widget_set_scale(MetaDeepinTabWidget* self, gdouble val)
{
    MetaDeepinTabWidgetPrivate* priv = self->priv;
    val = MIN(MAX(val, 0.0), 1.0);

    gdouble p = val / priv->scale;
    priv->scale = val;
    priv->real_size.width *= p;
    priv->real_size.height *= p;

    priv->render_thumb = (priv->real_size.width > ICON_SIZE * 1.75f);

    if (priv->render_thumb) {
        if (priv->snapshot) {
            g_clear_pointer(&priv->snapshot, cairo_surface_destroy);
        }

        MetaRectangle r = priv->outer_rect;

        double sx = RECT_PREFER_WIDTH / (double)r.width;
        double sy = RECT_PREFER_HEIGHT / (double)r.height;

        sx = MIN(sx, sy) * priv->scale;
        priv->snapshot = deepin_window_surface_manager_get_surface(
                priv->window, sx);
        if (priv->snapshot) cairo_surface_reference(priv->snapshot);
    }

    gtk_widget_queue_resize(GTK_WIDGET(self));
}

gdouble meta_deepin_tab_widget_get_scale(MetaDeepinTabWidget* self)
{
    return self->priv->scale;
}

MetaWindow* meta_deepin_tab_widget_get_meta_window(MetaDeepinTabWidget* self)
{
    return self->priv->window;
}

