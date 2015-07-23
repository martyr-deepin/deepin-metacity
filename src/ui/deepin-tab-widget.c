/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2005 Elijah Newren
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
#include <util.h>
#include <gdk/gdk.h>
#include "deepin-tab-widget.h"
#include "deepin-design.h"

typedef struct _MetaDeepinTabWidgetPrivate
{
  gboolean selected;
  gdouble scale;

  gboolean animation; /* in animation */

  gdouble current_pos;
  gdouble target_pos;

  gint64 start_time;
  gint64 end_time;

  GtkRequisition old_req;
  GtkRequisition dest_req;

  guint tick_id;
  int  animation_duration;

  GdkPixbuf* orig_thumb;
  GdkPixbuf* scaled;

  GtkRequisition init_size;
  GtkRequisition real_size;
} MetaDeepinTabWidgetPrivate;

enum {
    PROP_0,
    PROP_SCALE,
    N_PROPERTIES
};

static GParamSpec* property_specs[N_PROPERTIES] = {NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (MetaDeepinTabWidget, meta_deepin_tab_widget, GTK_TYPE_IMAGE);

static void meta_deepin_tab_widget_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
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

    if (priv->scaled) {
        g_clear_pointer(&priv->scaled, g_object_unref);
        g_clear_pointer(&priv->orig_thumb, g_object_unref);
    }

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
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  if (priv->selected) {
      gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);
  } else {
      gtk_style_context_set_state (context, gtk_widget_get_state_flags (widget));
  }

  return GTK_WIDGET_CLASS (meta_deepin_tab_widget_parent_class)->draw (widget, cr);
}

static void meta_deepin_tab_widget_end_animation(MetaDeepinTabWidget* self)
{
    MetaDeepinTabWidgetPrivate* priv = self->priv;
    g_assert(priv->animation == TRUE);
    g_assert(priv->tick_id != 0);
    
    gtk_widget_remove_tick_callback(GTK_WIDGET(self), priv->tick_id);
    priv->tick_id = 0;
    priv->animation = FALSE;
    priv->current_pos = priv->target_pos = 0;

    priv->real_size = priv->dest_req;

    gtk_widget_queue_draw(GTK_WIDGET(self));
}

/* From clutter-easing.c, based on Robert Penner's
 *  * infamous easing equations, MIT license.
 *   */
static double ease_out_cubic (double t)
{
  double p = t - 1;
  return p * p * p + 1;
}

static double ease_in_out_quad (double t)
{
    double p = t * 2.0;

    if (p < 1)
        return 0.5 * p * p;

    p -= 1;

    return -0.5 * (p * (p - 2) - 1);
}

static gboolean on_tick_callback(MetaDeepinTabWidget* self, GdkFrameClock* clock, 
        gpointer data)
{
    MetaDeepinTabWidgetPrivate* priv = self->priv;

    gint64 now = gdk_frame_clock_get_frame_time(clock);
    gdouble t = 1.0;
    if (now < priv->end_time) {
        t = (now - priv->start_time) / (gdouble)(priv->end_time - priv->start_time);
    }
    t = ease_out_cubic(t);
    priv->current_pos = t * priv->target_pos;
    if (priv->current_pos > priv->target_pos) priv->current_pos = priv->target_pos;
    gtk_widget_queue_resize(GTK_WIDGET(self));
    /*g_print("%s: current %f\n", __func__, priv->current_pos);*/

    if (priv->current_pos >= priv->target_pos) {
        meta_deepin_tab_widget_end_animation(self);
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
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

  if (priv->animation) {
      gint d = (priv->dest_req.width - priv->old_req.width) * priv->current_pos;
      *minimum_width = *natural_width = priv->old_req.width + d;
  }
}

static void meta_deepin_tab_widget_get_preferred_height_for_width(GtkWidget *widget,
        gint width, gint* minimum_height_out, gint* natural_height_out)
{
    MetaDeepinTabWidgetPrivate* priv = META_DEEPIN_TAB_WIDGET(widget)->priv;

    GTK_WIDGET_CLASS(meta_deepin_tab_widget_parent_class)->get_preferred_height_for_width(
            widget, width, minimum_height_out, natural_height_out);

    if (gtk_widget_get_mapped(widget) && priv->animation) {
        gint d = (priv->dest_req.height - priv->old_req.height) * priv->current_pos;
        *minimum_height_out = *natural_height_out = priv->old_req.height + d;
    }
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

  if (gtk_widget_get_mapped(widget) && priv->animation) {
      gint d = (priv->dest_req.height - priv->old_req.height) * priv->current_pos;
      *minimum_height = *natural_height = priv->old_req.height + d;
  }
}

static void meta_deepin_tab_widget_get_preferred_width_for_height(GtkWidget *widget,
        gint height, gint* minimum_width_out, gint* natural_width_out)
{
    GTK_WIDGET_CLASS(meta_deepin_tab_widget_parent_class)->get_preferred_width_for_height(
            widget, height, minimum_width_out, natural_width_out);

    MetaDeepinTabWidgetPrivate* priv = META_DEEPIN_TAB_WIDGET(widget)->priv;
    if (gtk_widget_get_mapped(widget) && priv->animation) {
        gint d = (priv->dest_req.width - priv->old_req.width) * priv->current_pos;
        *minimum_width_out = *natural_width_out = priv->old_req.width + d;
    }
}

static void meta_deepin_tab_widget_update_image(MetaDeepinTabWidget* self)
{
    MetaDeepinTabWidgetPrivate* priv = self->priv;

    GtkRequisition req;
    req.width = priv->scale * RECT_PREFER_WIDTH;
    req.height = priv->scale * RECT_PREFER_HEIGHT;

    if (priv->orig_thumb) {
        if (priv->scaled) {
            g_object_unref(priv->scaled);
        }

        priv->scaled = gdk_pixbuf_scale_simple(priv->orig_thumb,
                req.width, req.height, GDK_INTERP_BILINEAR);
        gtk_image_set_from_pixbuf(GTK_IMAGE(self), priv->scaled);
    }
}

static void meta_deepin_tab_widget_size_allocate(GtkWidget* widget, 
        GtkAllocation* allocation)
{
    gtk_widget_set_allocation(widget, allocation);
}

static void meta_deepin_tab_widget_init (MetaDeepinTabWidget *self)
{
  self->priv = (MetaDeepinTabWidgetPrivate*)meta_deepin_tab_widget_get_instance_private (self);
  self->priv->animation_duration = SWITCHER_SELECT_ANIMATION_DURATION;
  self->priv->real_size.width = SWITCHER_ITEM_PREFER_WIDTH;
  self->priv->real_size.height = SWITCHER_ITEM_PREFER_HEIGHT;
  self->priv->init_size = self->priv->real_size;
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
meta_deepin_tab_widget_new (GdkPixbuf *pixbuf)
{
  MetaDeepinTabWidget* widget;

  widget = (MetaDeepinTabWidget*)g_object_new (META_TYPE_DEEPIN_TAB_WIDGET, NULL);
  widget->priv->orig_thumb = pixbuf;
  g_object_ref(pixbuf);

  return (GtkWidget*)widget;
}

static void meta_deepin_tab_widget_prepare_animation(MetaDeepinTabWidget* self, 
        gboolean select)
{
    if (!gtk_widget_get_realized(GTK_WIDGET(self))) {
        meta_topic(META_DEBUG_UI, "tab item is not realized");
        gtk_widget_realize(GTK_WIDGET(self));
    }

    MetaDeepinTabWidgetPrivate* priv = self->priv;
    if (priv->tick_id) {
        meta_deepin_tab_widget_end_animation(self);
    }

    priv->target_pos = 1.0;
    priv->current_pos = 0.0;

    priv->start_time = gdk_frame_clock_get_frame_time(
            gtk_widget_get_frame_clock(GTK_WIDGET(self)));
    priv->end_time = priv->start_time + (priv->animation_duration * 1000);

    priv->tick_id = gtk_widget_add_tick_callback(GTK_WIDGET(self),
            (GtkTickCallback)on_tick_callback, 0, 0);

    if (select) {
        priv->old_req = priv->init_size; 
        priv->dest_req.width = priv->old_req.width * 1.033;
        priv->dest_req.height = priv->old_req.height * 1.033;
    } else {
        priv->dest_req = priv->init_size; 
        priv->old_req.width = priv->dest_req.width * 1.033;
        priv->old_req.height = priv->dest_req.height * 1.033;
    }

    meta_topic(META_DEBUG_UI, "%s: start %ld, end %ld, req(%d, %d) -> (%d, %d)\n",
            __func__, priv->start_time, priv->end_time,
            priv->old_req.width, priv->old_req.height,
            priv->dest_req.width, priv->dest_req.height);
    priv->animation = TRUE;
}

void meta_deepin_tab_widget_select (MetaDeepinTabWidget *self)
{
    MetaDeepinTabWidgetPrivate* priv = self->priv;
    priv->selected = TRUE;
    meta_deepin_tab_widget_prepare_animation(self, priv->selected);
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

void meta_deepin_tab_widget_unselect (MetaDeepinTabWidget *self)
{
    MetaDeepinTabWidgetPrivate* priv = self->priv;
    priv->selected = FALSE;
    meta_deepin_tab_widget_prepare_animation(self, priv->selected);
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

void meta_deepin_tab_widget_set_scale(MetaDeepinTabWidget* self, gdouble val)
{
    MetaDeepinTabWidgetPrivate* priv = self->priv;
    val = MIN(MAX(val, 0.0), 1.0);
    priv->scale = val;
    /*FIXME: do animation and queue draw*/
    if (priv->animation) {
        meta_deepin_tab_widget_end_animation(self);
    }
    priv->init_size.width *= val;
    priv->init_size.height *= val;
    priv->real_size.width *= val;
    priv->real_size.height *= val;

    meta_deepin_tab_widget_update_image(self);
    gtk_widget_queue_resize(GTK_WIDGET(self));
}

gdouble meta_deepin_tab_widget_get_scale(MetaDeepinTabWidget* self)
{
    return self->priv->scale;
}

