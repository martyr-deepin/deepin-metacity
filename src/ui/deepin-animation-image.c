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
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <cairo.h>

#include "deepin-animation-image.h"

struct _DeepinAnimationImagePrivate
{
    guint disposed: 1;
    guint activated: 1;

    GPtrArray *frames;  // surfaces
    GPtrArray *frame_names; //strings

    int width;
    int height;

    int current_frame;
    int duration;
    guint animation_id;
};

enum
{
    PROP_0,
    PROP_FRAME_NAMES,
    PROP_DURATION,
    NUM_PROPERTIES
};

static GParamSpec *image_props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (DeepinAnimationImage, deepin_animation_image, GTK_TYPE_WIDGET)

static void deepin_animation_image_get_preferred_width (GtkWidget *widget,
        gint *minimum, gint *natural)
{
    DeepinAnimationImage *image = DEEPIN_ANIMATION_IMAGE (widget);
    DeepinAnimationImagePrivate *priv = image->priv;

    *minimum = *natural = priv->width;
}

static void deepin_animation_image_get_preferred_height (GtkWidget *widget,
        gint *minimum, gint *natural)
{
    DeepinAnimationImage *image = DEEPIN_ANIMATION_IMAGE (widget);
    DeepinAnimationImagePrivate *priv = image->priv;

    *minimum = *natural = priv->height;
}

static void deepin_animation_image_init (DeepinAnimationImage *image)
{
    DeepinAnimationImagePrivate *priv;

    image->priv = deepin_animation_image_get_instance_private (image);
    priv = image->priv;
    priv->width = priv->height = 38; //default

    priv->frames = NULL;
    priv->frame_names = NULL;
    priv->activated = FALSE;
}

static void deepin_animation_image_finalize (GObject *object)
{
    DeepinAnimationImage *image = DEEPIN_ANIMATION_IMAGE (object);
    DeepinAnimationImagePrivate *priv = image->priv;

    if (priv->disposed) return;

    priv->disposed = TRUE;
    g_clear_pointer (&priv->frames, g_ptr_array_unref);
    g_clear_pointer (&priv->frame_names, g_ptr_array_unref);

    G_OBJECT_CLASS (deepin_animation_image_parent_class)->finalize (object);
};

static void deepin_animation_image_set_from_files (DeepinAnimationImage *image, GPtrArray *names)
{
    DeepinAnimationImagePrivate *priv;
    gboolean update_size = TRUE;

    g_return_if_fail (DEEPIN_IS_ANIMATION_IMAGE (image));

    priv = image->priv;

    g_object_freeze_notify (G_OBJECT (image));
    
    g_clear_pointer (&priv->frame_names, g_ptr_array_unref);
    g_clear_pointer (&priv->frames, g_ptr_array_unref);

    priv->frame_names = g_ptr_array_ref (names);
    priv->frames = g_ptr_array_new_with_free_func ((GDestroyNotify)cairo_surface_destroy);

    for (int i = 0; i < names->len; i++) {
        GError *error = NULL;
        GdkPixbuf *pb = gdk_pixbuf_new_from_file (g_ptr_array_index (names, i), &error);
        if (pb == NULL) {
            g_warning ("%s\n", error->message);
            g_error_free (error);
            continue;
        }

        g_ptr_array_add (priv->frames, gdk_cairo_surface_create_from_pixbuf (pb, 1, NULL));
        g_object_unref (pb);

        if (update_size) {
            update_size = FALSE;
            priv->width = gdk_pixbuf_get_width (pb);
            priv->height = gdk_pixbuf_get_height (pb);
            gtk_widget_queue_resize (GTK_WIDGET(image));
        }

    }

    g_object_thaw_notify (G_OBJECT (image));
}

static void deepin_animation_image_set_property (GObject      *object,
        guint         prop_id,
        const GValue *value,
        GParamSpec   *pspec)
{
    DeepinAnimationImage *image = DEEPIN_ANIMATION_IMAGE (object);

    switch (prop_id)
    {
        case PROP_FRAME_NAMES:
            deepin_animation_image_set_from_files (image, g_value_get_pointer (value));
            break;

        case PROP_DURATION:
            image->priv->duration = g_value_get_int (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void deepin_animation_image_get_property (GObject     *object,
        guint        prop_id,
        GValue      *value,
        GParamSpec  *pspec)
{
    DeepinAnimationImage *image = DEEPIN_ANIMATION_IMAGE (object);
    DeepinAnimationImagePrivate *priv = image->priv;

    switch (prop_id)
    {
        case PROP_FRAME_NAMES:
            g_value_set_pointer (value, priv->frame_names);
            break;
        case PROP_DURATION:
            g_value_set_int (value, (int)priv->duration);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static gboolean deepin_animation_image_draw (GtkWidget *widget, cairo_t *cr)
{
    DeepinAnimationImage *image = DEEPIN_ANIMATION_IMAGE (widget);
    DeepinAnimationImagePrivate *priv = image->priv;

    g_return_val_if_fail (priv->frames != NULL && priv->frames->len != 0, FALSE);

    if (priv->current_frame >= 0 && priv->current_frame <= priv->frames->len) {
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface (cr, g_ptr_array_index (priv->frames, priv->current_frame), 0, 0);
        cairo_paint (cr);
    }

    return FALSE;
}

static void deepin_animation_image_class_init (DeepinAnimationImageClass *class)
{
    GObjectClass *gobject_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS (class);

    gobject_class->set_property = deepin_animation_image_set_property;
    gobject_class->get_property = deepin_animation_image_get_property;
    gobject_class->finalize = deepin_animation_image_finalize;

    widget_class = GTK_WIDGET_CLASS (class);
    widget_class->draw = deepin_animation_image_draw;
    widget_class->get_preferred_width = deepin_animation_image_get_preferred_width;
    widget_class->get_preferred_height = deepin_animation_image_get_preferred_height;

    image_props[PROP_FRAME_NAMES] = g_param_spec_pointer ("frame-names",
                ("frame names"),
                ("frame names"),
                G_PARAM_READWRITE);

    image_props[PROP_DURATION] = g_param_spec_int ("duration",
                ("Animation duration"),
                ("Animation duration"),
                0, G_MAXINT,
                280,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_install_properties (gobject_class, NUM_PROPERTIES, image_props);
}


GtkWidget* deepin_animation_image_new (GPtrArray *names)
{
    DeepinAnimationImage *dai;

    dai = g_object_new (DEEPIN_TYPE_ANIMATION_IMAGE, NULL);

    deepin_animation_image_set_from_files (dai, names);

    return GTK_WIDGET (dai);
}

static gboolean on_time_out (gpointer data)
{
    DeepinAnimationImagePrivate *priv = DEEPIN_ANIMATION_IMAGE(data)->priv;
    gtk_widget_queue_draw (GTK_WIDGET (data));
    priv->current_frame++;
    if (priv->current_frame == priv->frames->len) {
        priv->animation_id = 0;
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

static void deepin_animation_image_stop_animation (DeepinAnimationImage *self)
{
    DeepinAnimationImagePrivate *priv = self->priv;

    if (priv->animation_id != 0) {
        g_source_remove (priv->animation_id);
        priv->animation_id = 0;
    }

    priv->current_frame = -1;
    gtk_widget_queue_draw (GTK_WIDGET(self));
}

static void deepin_animation_image_start_animation (DeepinAnimationImage *self)
{
    DeepinAnimationImagePrivate *priv = self->priv;

    g_return_if_fail (priv->frames != NULL && priv->frames->len != 0);

    deepin_animation_image_stop_animation (self);
    priv->animation_id = g_timeout_add (priv->duration / priv->frames->len, on_time_out, self);
}

void deepin_animation_image_activate (DeepinAnimationImage *self)
{
    if (self->priv->activated == FALSE) {
        self->priv->activated = TRUE;
        deepin_animation_image_start_animation (self);
    }
}

void deepin_animation_image_deactivate (DeepinAnimationImage *self)
{
    if (self->priv->activated == TRUE) {
        self->priv->activated = FALSE;
        deepin_animation_image_stop_animation (self);
    }
}

gboolean deepin_animation_image_get_activated (DeepinAnimationImage *self)
{
    return self->priv->activated;
}

