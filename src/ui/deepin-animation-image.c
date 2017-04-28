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

    int width;
    int height;
};

enum
{
    PROP_0,
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

    priv->activated = FALSE;
}

static void deepin_animation_image_finalize (GObject *object)
{
    DeepinAnimationImage *image = DEEPIN_ANIMATION_IMAGE (object);
    DeepinAnimationImagePrivate *priv = image->priv;

    if (priv->disposed) return;

    priv->disposed = TRUE;

    G_OBJECT_CLASS (deepin_animation_image_parent_class)->finalize (object);
};

static void deepin_animation_image_set_property (GObject      *object,
        guint         prop_id,
        const GValue *value,
        GParamSpec   *pspec)
{
    DeepinAnimationImage *image = DEEPIN_ANIMATION_IMAGE (object);

    switch (prop_id)
    {

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
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static gboolean deepin_animation_image_draw (GtkWidget *widget, cairo_t *cr)
{
    DeepinAnimationImage *image = DEEPIN_ANIMATION_IMAGE (widget);
    DeepinAnimationImagePrivate *priv = image->priv;

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
}


GtkWidget* deepin_animation_image_new (GPtrArray *names)
{
    DeepinAnimationImage *dai;

    dai = g_object_new (DEEPIN_TYPE_ANIMATION_IMAGE, NULL);

    return GTK_WIDGET (dai);
}

void deepin_animation_image_activate (DeepinAnimationImage *self)
{
    if (self->priv->activated == FALSE) {
        self->priv->activated = TRUE;
    }
}

void deepin_animation_image_deactivate (DeepinAnimationImage *self)
{
    if (self->priv->activated == TRUE) {
        self->priv->activated = FALSE;
    }
}

gboolean deepin_animation_image_get_activated (DeepinAnimationImage *self)
{
    return self->priv->activated;
}

