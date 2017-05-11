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
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <libbamf/libbamf.h>
#include "boxes.h"
#include "../core/window-private.h"
#include "../core/workspace.h"
#include "deepin-tab-widget.h"
#include "deepin-design.h"
#include "deepin-ease.h"
#include "deepin-window-surface-manager.h"
#include "deepin-background-cache.h"
#include "deepin-desktop-background.h"

typedef struct _MetaDeepinTabWidgetPrivate
{
  gboolean selected;

  cairo_surface_t* icon;

  MetaWindow* window;
  MetaRectangle outer_rect; /* cache of window outer rect */
  GdkRectangle mon_geom; /* primary monitor geometry */

  gint disposed: 1;
  gint render_thumb: 1; /* if size is too small, do not render it */

  GtkRequisition init_size;
  GtkRequisition real_size;

  GdkWindow* event_window;
} MetaDeepinTabWidgetPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaDeepinTabWidget, meta_deepin_tab_widget, GTK_TYPE_WIDGET);

static void meta_deepin_tab_widget_dispose(GObject *object)
{
    MetaDeepinTabWidget *self = META_DEEPIN_TAB_WIDGET(object);
    MetaDeepinTabWidgetPrivate* priv = self->priv;

    if (priv->disposed) return;
    priv->disposed = TRUE;

    g_clear_pointer(&priv->icon, cairo_surface_destroy);

    G_OBJECT_CLASS(meta_deepin_tab_widget_parent_class)->dispose(object);
}

static void meta_deepin_tab_widget_finalize(GObject *object)
{
    MetaDeepinTabWidget *head = META_DEEPIN_TAB_WIDGET(object);
    G_GNUC_UNUSED MetaDeepinTabWidgetPrivate* priv = head->priv;

    G_OBJECT_CLASS(meta_deepin_tab_widget_parent_class)->finalize(object);
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
  cairo_translate(cr, w2, h2);
  gtk_render_frame(context, cr, -w2, -h2, w, h);
  cairo_restore(cr);

  if (priv->window->type == META_WINDOW_DESKTOP) {
      MetaRectangle r = priv->outer_rect;
      double sx = RECT_PREFER_WIDTH / (double)r.width;
      double sy = RECT_PREFER_HEIGHT / (double)r.height;
      sx = MIN(sx, sy);

      int primary = gdk_screen_get_primary_monitor(gdk_screen_get_default());

      MetaScreen *screen = meta_get_display()->active_screen;
      int index = meta_workspace_index(screen->active_workspace);
      cairo_surface_t* ref = deepin_background_cache_get_surface(primary, index, sx);
      if (ref) {
          x = (w - cairo_image_surface_get_width(ref)) / 2.0,
            y = (h - cairo_image_surface_get_height(ref)) / 2.0;
          cairo_set_source_surface(cr, ref, x, y);
          cairo_paint(cr);
      }
  }

  if (priv->icon) {
      double iw = cairo_image_surface_get_width(priv->icon);
      double ih = cairo_image_surface_get_height(priv->icon);

      x = (w - iw) / 2.0;
      y = (h - ih) / 2.0;

      cairo_set_source_surface(cr, priv->icon, x, y);
      cairo_paint(cr);
  }

  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

  if (priv->selected && priv->window->title != NULL) {
      cairo_text_extents_t extents;
      double x, y;

      char text[16*8];
      g_utf8_strncpy(text, priv->window->title, 16);

      cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
              CAIRO_FONT_WEIGHT_NORMAL);
      cairo_set_font_size (cr, 12.0);

      cairo_text_extents (cr, text, &extents);
      x = (128.0-extents.width)/2 + extents.x_bearing;
      y = 128.0+20;

      cairo_move_to(cr, x, y);
      cairo_show_text(cr, text);
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
    expanded.width = allocation->width;
    expanded.height = allocation->height + 80;
    expanded.x = allocation->x - (expanded.width - allocation->width) / 2;
    expanded.y = allocation->y - (expanded.height - allocation->height) / 2;
    gtk_widget_set_clip(widget, &expanded);
}

static void meta_deepin_tab_widget_init (MetaDeepinTabWidget *self)
{
  self->priv = (MetaDeepinTabWidgetPrivate*)meta_deepin_tab_widget_get_instance_private (self);
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

  gobject_class->dispose = meta_deepin_tab_widget_dispose;
  gobject_class->finalize = meta_deepin_tab_widget_finalize;

}

GtkWidget * meta_deepin_tab_widget_new (MetaWindow* window)
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

      GdkPixbuf* pixbuf = meta_window_get_application_icon(window, RECT_PREFER_WIDTH);
      widget->priv->icon = gdk_cairo_surface_create_from_pixbuf(pixbuf, 1.0, NULL);
      g_object_unref(pixbuf);
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

MetaWindow* meta_deepin_tab_widget_get_meta_window(MetaDeepinTabWidget* self)
{
    return self->priv->window;
}

