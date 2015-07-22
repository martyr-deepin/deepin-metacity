/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2005 Elijah Newren
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
#include "select-image.h"
#include "deepin-design.h"

typedef struct _MetaSelectImagePrivate
{
  gboolean selected;

  gboolean animation; /* in animation */

  gdouble current_pos;
  gdouble target_pos;

  gint64 start_time;
  gint64 end_time;

  GtkRequisition old_req;
  GtkRequisition dest_req;

  guint tick_id;
  int  animation_duration;
} MetaSelectImagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaSelectImage, meta_select_image, GTK_TYPE_IMAGE);

static gboolean
meta_select_image_draw (GtkWidget *widget,
                        cairo_t   *cr)
{
  MetaSelectImage *image = META_SELECT_IMAGE (widget);
  MetaSelectImagePrivate* priv = image->priv;
  GtkRequisition requisition;
  GtkStyleContext *context;
  int x, y;

  gtk_widget_get_preferred_size (widget, &requisition, 0);

  x = 0, y = 0;

  context = gtk_widget_get_style_context (widget);

  if (priv->selected) {
      gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);
  } else {
      gtk_style_context_set_state (context, gtk_widget_get_state_flags (widget));
  }

  if (priv->animation) {
      gint d = (priv->dest_req.width - priv->old_req.width) * priv->current_pos;
      requisition.width = priv->old_req.width + d;

      d = (priv->dest_req.height - priv->old_req.height) * priv->current_pos;
      requisition.height = priv->old_req.height + d;
  }

  /*gtk_render_background(context, cr, x, y, requisition.width, requisition.height);*/
  /*gtk_render_frame(context, cr, x, y, requisition.width, requisition.height);*/

  return GTK_WIDGET_CLASS (meta_select_image_parent_class)->draw (widget, cr);
}

static void meta_select_image_end_animation(MetaSelectImage* image)
{
    MetaSelectImagePrivate* priv = image->priv;
    g_assert(priv->animation == TRUE);
    g_assert(priv->tick_id != 0);
    
    gtk_widget_remove_tick_callback(GTK_WIDGET(image), priv->tick_id);
    priv->tick_id = 0;
    priv->animation = FALSE;
    priv->current_pos = priv->target_pos = 0;

    gtk_widget_queue_draw(GTK_WIDGET(image));
}

/* From clutter-easing.c, based on Robert Penner's
 *  * infamous easing equations, MIT license.
 *   */
static double ease_out_cubic (double t)
{
  double p = t - 1;
  return p * p * p + 1;
}

static gboolean on_tick_callback(MetaSelectImage* image, GdkFrameClock* clock, 
        gpointer data)
{
    MetaSelectImage* self = META_SELECT_IMAGE(image);
    MetaSelectImagePrivate* priv = self->priv;

    gint64 now = gdk_frame_clock_get_frame_time(clock);
    gdouble t = 1.0;
    if (now < priv->end_time) {
        t = (now - priv->start_time) / (gdouble)(priv->end_time - priv->start_time);
    }
    t = ease_out_cubic(t);
    priv->current_pos += t * priv->target_pos;
    if (priv->current_pos > priv->target_pos) priv->current_pos = priv->target_pos;
    gtk_widget_queue_draw(GTK_WIDGET(image));
    g_print("%s: current %f\n", __func__, priv->current_pos);

    if (priv->current_pos >= priv->target_pos) {
        meta_select_image_end_animation(image);
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

static void
meta_select_image_get_preferred_width (GtkWidget *widget,
                                       gint      *minimum_width,
                                       gint      *natural_width)
{
  GTK_WIDGET_CLASS (meta_select_image_parent_class)->get_preferred_width (widget,
                                                                          minimum_width,
                                                                          natural_width);

  /*FIXME: need to take care of scale */
  /**minimum_width = SWITCHER_ITEM_PREFER_WIDTH;*/
  /**natural_width = SWITCHER_ITEM_PREFER_WIDTH;*/
}

static void
meta_select_image_get_preferred_height (GtkWidget *widget,
                                        gint      *minimum_height,
                                        gint      *natural_height)
{
  GTK_WIDGET_CLASS (meta_select_image_parent_class)->get_preferred_height (widget,
                                                                           minimum_height,
                                                                           natural_height);

  /**minimum_height = SWITCHER_ITEM_PREFER_HEIGHT;*/
  /**natural_height = SWITCHER_ITEM_PREFER_HEIGHT;*/
}

static void
meta_select_image_init (MetaSelectImage *image)
{
  image->priv = meta_select_image_get_instance_private (image);
  image->priv->animation_duration = 1280;
}

static void
meta_select_image_class_init (MetaSelectImageClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (class);

  widget_class->draw = meta_select_image_draw;
  widget_class->get_preferred_width = meta_select_image_get_preferred_width;
  widget_class->get_preferred_height = meta_select_image_get_preferred_height;
}

GtkWidget *
meta_select_image_new (GdkPixbuf *pixbuf)
{
  GtkWidget *widget;

  widget = g_object_new (META_TYPE_SELECT_IMAGE, NULL);
  gtk_image_set_from_pixbuf (GTK_IMAGE (widget), pixbuf);

  return widget;
}

static void meta_select_image_prepare_animation(MetaSelectImage* image, 
        gboolean select)
{
    if (!gtk_widget_get_realized(GTK_WIDGET(image))) {
        meta_topic(META_DEBUG_UI, "tab item is not realized");
        g_print("%s: %s\n", __func__, "tab item is not realized");
        return;
    }

    MetaSelectImagePrivate* priv = image->priv;
    if (priv->tick_id) {
        meta_select_image_end_animation(image);
    }

    priv->target_pos = 1.0;
    priv->current_pos = 0.0;

    priv->start_time = gdk_frame_clock_get_frame_time(
            gtk_widget_get_frame_clock(GTK_WIDGET(image)));
    priv->end_time = priv->start_time + (priv->animation_duration * 1000);

    priv->tick_id = gtk_widget_add_tick_callback(GTK_WIDGET(image),
            (GtkTickCallback)on_tick_callback, 0, 0);

    if (select) {
        gtk_widget_get_preferred_size (GTK_WIDGET(image), &priv->old_req, 0);
        priv->dest_req.width = priv->old_req.width * 1.033;
        priv->dest_req.height = priv->old_req.height * 1.033;
    } else {
        gtk_widget_get_preferred_size (GTK_WIDGET(image), &priv->dest_req, 0);
        priv->old_req.width = priv->dest_req.width * 1.033;
        priv->old_req.height = priv->dest_req.height * 1.033;
    }

    g_print("%s: start %lld, end %lld, req(%d, %d)\n", __func__,
            priv->start_time, priv->end_time, priv->old_req.width, 
            priv->dest_req.width);
    priv->animation = TRUE;
}

void
meta_select_image_select (MetaSelectImage *image)
{
    MetaSelectImagePrivate* priv = image->priv;
    priv->selected = TRUE;
    meta_select_image_prepare_animation(image, priv->selected);
    gtk_widget_queue_draw (GTK_WIDGET (image));
}

void
meta_select_image_unselect (MetaSelectImage *image)
{
    MetaSelectImagePrivate* priv = image->priv;
    priv->selected = FALSE;
    meta_select_image_prepare_animation(image, priv->selected);
    gtk_widget_queue_draw (GTK_WIDGET (image));
}
