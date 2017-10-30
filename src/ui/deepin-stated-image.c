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

#include "deepin-stated-image.h"

struct _DeepinStatedImagePrivate
{
    gchar *filename; //template filename
    int width;
    int height;

    GdkPixbuf *normal;
    GdkPixbuf *hover;
    GdkPixbuf *press;

    DeepinStatedImageState state;
};

enum
{
    PROP_0,
    PROP_FILE,
    PROP_STATE,
    NUM_PROPERTIES
};

static GParamSpec *image_props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (DeepinStatedImage, deepin_stated_image, GTK_TYPE_EVENT_BOX)

static void deepin_stated_image_get_preferred_width (GtkWidget *widget,
        gint *minimum, gint *natural)
{
    DeepinStatedImage *image = DEEPIN_STATED_IMAGE (widget);
    DeepinStatedImagePrivate *priv = image->priv;

    *minimum = *natural = priv->width;
}

static void deepin_stated_image_get_preferred_height (GtkWidget *widget,
        gint *minimum, gint *natural)
{
    DeepinStatedImage *image = DEEPIN_STATED_IMAGE (widget);
    DeepinStatedImagePrivate *priv = image->priv;

    *minimum = *natural = priv->height;
}

static void deepin_stated_image_init (DeepinStatedImage *image)
{
    DeepinStatedImagePrivate *priv;

    image->priv = deepin_stated_image_get_instance_private (image);
    priv = image->priv;
    priv->width = priv->height = 48; //default

    priv->state = DSINormal;
}

static void deepin_stated_image_finalize (GObject *object)
{
    DeepinStatedImage *image = DEEPIN_STATED_IMAGE (object);
    DeepinStatedImagePrivate *priv = image->priv;

    g_free (priv->filename);
    GdkPixbuf** pbs[] = {&priv->normal, &priv->hover, &priv->press};
    for (int i = 0; i < G_N_ELEMENTS(pbs); i++) {
        if (*pbs[i]) g_clear_pointer(pbs[i], g_object_unref);
    }

    G_OBJECT_CLASS (deepin_stated_image_parent_class)->finalize (object);
};

static void deepin_stated_image_set_from_file (DeepinStatedImage    *image,
        const gchar *filename)
{
    DeepinStatedImagePrivate *priv;

    g_return_if_fail (DEEPIN_IS_STATED_IMAGE (image));

    priv = image->priv;

    g_object_freeze_notify (G_OBJECT (image));

    if (filename == NULL) {
        priv->filename = NULL;
        g_object_thaw_notify (G_OBJECT (image));
        return;
    }

    priv->filename = g_strdup (filename);

    const char * states[] = {"normal", "hover", "press"};
    GdkPixbuf** pbs[] = {&priv->normal, &priv->hover, &priv->press};
    for (int i = 0; i < G_N_ELEMENTS(pbs); i++) {
        char *name = g_strdup_printf (METACITY_PKGDATADIR "/%s_%s.svg", priv->filename, states[i]);

        GError *error = NULL;
        *pbs[i] = gdk_pixbuf_new_from_file (name, &error);
        if (*pbs[i] == NULL) {
            g_warning ("%s\n", error->message);
            g_error_free (error);
        }
        g_free (name);
    }

    if (priv->normal) {
        priv->width = gdk_pixbuf_get_width (priv->normal);
        priv->height = gdk_pixbuf_get_height (priv->normal);
        gtk_widget_queue_resize (GTK_WIDGET(image));
    }

    g_object_thaw_notify (G_OBJECT (image));
}

static void deepin_stated_image_set_state (DeepinStatedImage *image, int value)
{
    g_return_if_fail (value >= DSINormal && value <= DSIPressed);
    DeepinStatedImagePrivate *priv = image->priv;

    if (priv->state != value) {
        priv->state = value;
        gtk_widget_queue_draw (GTK_WIDGET(image));
        g_object_notify_by_pspec (G_OBJECT(image), image_props[PROP_STATE]);
    }
}

static void deepin_stated_image_set_property (GObject      *object,
        guint         prop_id,
        const GValue *value,
        GParamSpec   *pspec)
{
    DeepinStatedImage *image = DEEPIN_STATED_IMAGE (object);

    switch (prop_id)
    {
        case PROP_FILE:
            deepin_stated_image_set_from_file (image, g_value_get_string (value));
            break;

        case PROP_STATE:
            deepin_stated_image_set_state (image, g_value_get_int (value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void deepin_stated_image_get_property (GObject     *object,
        guint        prop_id,
        GValue      *value,
        GParamSpec  *pspec)
{
    DeepinStatedImage *image = DEEPIN_STATED_IMAGE (object);
    DeepinStatedImagePrivate *priv = image->priv;

    switch (prop_id)
    {
        case PROP_FILE:
            g_value_set_string (value, priv->filename);
            break;
        case PROP_STATE:
            g_value_set_int (value, (int)priv->state);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static gboolean deepin_stated_image_draw (GtkWidget *widget, cairo_t *cr)
{
    DeepinStatedImage *image = DEEPIN_STATED_IMAGE (widget);
    DeepinStatedImagePrivate *priv = image->priv;

    GdkPixbuf *pb = priv->state == DSINormal ? priv->normal :
                    priv->state == DSIPrelight ? priv->hover : 
                    priv->press;
    gdk_cairo_set_source_pixbuf (cr, pb, 0, 0);
    cairo_paint (cr);

    return FALSE;
}

static gboolean deepin_stated_image_leave(GtkWidget *widget, GdkEventCrossing *event)
{
    DeepinStatedImage *image = DEEPIN_STATED_IMAGE (widget);

    deepin_stated_image_set_state (image, DSINormal);
    
    if (GTK_WIDGET_CLASS (deepin_stated_image_parent_class)->leave_notify_event)
        return GTK_WIDGET_CLASS(deepin_stated_image_parent_class)->leave_notify_event (widget, event);
}

static gboolean deepin_stated_image_enter(GtkWidget *widget, GdkEventCrossing *event)
{
    DeepinStatedImage *image = DEEPIN_STATED_IMAGE (widget);

    deepin_stated_image_set_state (image, DSIPrelight);
    
    if (GTK_WIDGET_CLASS (deepin_stated_image_parent_class)->enter_notify_event)
        return GTK_WIDGET_CLASS(deepin_stated_image_parent_class)->enter_notify_event (widget, event);
}

static gboolean deepin_stated_image_button_press(GtkWidget *widget, GdkEventButton *event)
{
    DeepinStatedImage *image = DEEPIN_STATED_IMAGE (widget);

    deepin_stated_image_set_state (image, DSIPressed);
    
    if (GTK_WIDGET_CLASS (deepin_stated_image_parent_class)->button_press_event)
        return GTK_WIDGET_CLASS(deepin_stated_image_parent_class)->button_press_event (widget, event);
}

static gboolean deepin_stated_image_button_release(GtkWidget *widget, GdkEventButton *event)
{
    DeepinStatedImage *image = DEEPIN_STATED_IMAGE (widget);

    deepin_stated_image_set_state (image, DSIPrelight);
    
    if (GTK_WIDGET_CLASS (deepin_stated_image_parent_class)->button_release_event)
        return GTK_WIDGET_CLASS(deepin_stated_image_parent_class)->button_release_event (widget, event);
}

static void deepin_stated_image_class_init (DeepinStatedImageClass *class)
{
    GObjectClass *gobject_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS (class);

    gobject_class->set_property = deepin_stated_image_set_property;
    gobject_class->get_property = deepin_stated_image_get_property;
    gobject_class->finalize = deepin_stated_image_finalize;

    widget_class = GTK_WIDGET_CLASS (class);
    widget_class->draw = deepin_stated_image_draw;
    widget_class->get_preferred_width = deepin_stated_image_get_preferred_width;
    widget_class->get_preferred_height = deepin_stated_image_get_preferred_height;

    widget_class->enter_notify_event = deepin_stated_image_enter;
    widget_class->leave_notify_event = deepin_stated_image_leave;
    widget_class->button_press_event = deepin_stated_image_button_press;
    widget_class->button_release_event = deepin_stated_image_button_release;

    image_props[PROP_FILE] =
        g_param_spec_string ("file",
                ("Filename"),
                ("Filename to load and display"),
                NULL,
                G_PARAM_READWRITE);

    image_props[PROP_STATE] =
        g_param_spec_int ("state",
                ("State"),
                ("Current state"),
                0, G_MAXINT,
                0,
                G_PARAM_READWRITE);

    g_object_class_install_properties (gobject_class, NUM_PROPERTIES, image_props);
}


GtkWidget* deepin_stated_image_new_from_file (const gchar *filename)
{
    DeepinStatedImage *image;

    image = g_object_new (DEEPIN_TYPE_STATED_IMAGE, NULL);

    deepin_stated_image_set_from_file (image, filename);

    gtk_event_box_set_above_child(GTK_EVENT_BOX(image), FALSE);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(image), FALSE);

    return GTK_WIDGET (image);
}

