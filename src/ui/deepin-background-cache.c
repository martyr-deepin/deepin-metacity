/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

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
#include <gdk/gdkx.h>
#include <cairo.h>
#include "deepin-background-cache.h"
#include "deepin-message-hub.h"

#define GSETTINGS_BG_KEY "picture-uri"
#define GSETTINGS_PRIM_CLR "primary-color"
#define BACKGROUND_SCHEMA "com.deepin.wrap.gnome.desktop.background"

typedef struct _ScaledCacheInfo
{
    gint monitor;
    double scale;
    cairo_surface_t* surface;
} ScaledCacheInfo;

struct _DeepinBackgroundCachePrivate
{
    GList* caches;
    gboolean solid_mode;  // background is solid color

    GSettings *bg_settings;
};

static DeepinBackgroundCache* _the_cache = NULL;


G_DEFINE_TYPE (DeepinBackgroundCache, deepin_background_cache, G_TYPE_OBJECT);

static void deepin_background_cache_flush(DeepinBackgroundCache* self)
{
    DeepinBackgroundCachePrivate* priv = self->priv;
    if (priv->caches) {
        GList* l = priv->caches;
        while (l) {
            ScaledCacheInfo* sci = (ScaledCacheInfo*)l->data;
            cairo_surface_destroy(sci->surface);
            g_slice_free(ScaledCacheInfo, sci);
            l = l->next;
        }
        g_list_free(priv->caches);
        priv->caches = NULL;
    }
}

static GdkPixbuf* _do_scale(DeepinBackgroundCache* self, GdkPixbuf* pixbuf, gint width, gint height)
{
    GdkPixbuf* new_pixbuf;

    int pw = gdk_pixbuf_get_width(pixbuf), ph = gdk_pixbuf_get_height(pixbuf);

    if (pw == width && ph == height) {
        g_object_ref(pixbuf);
        return pixbuf;
    }

    double scale = (double)width / height;

    int w = pw;
    int h = (int)((double)w / scale);
    int x = 0, y = 0;
    if (h < ph) {
        y = (ph - h) / 2;
    } else {
        h = ph;
        w = (int)((double)h * scale);
        x = (pw - w) / 2;
    }

    meta_verbose("%s: scale = %f, (%d, %d, %d, %d)\n", __func__, scale, x, y, w, h);
    GdkPixbuf* subpixbuf = gdk_pixbuf_new_subpixbuf(pixbuf, x, y, w, h);

    new_pixbuf = gdk_pixbuf_scale_simple(subpixbuf, width, 
            height, GDK_INTERP_BILINEAR);

    g_object_unref(subpixbuf);

    return new_pixbuf;
}

static cairo_surface_t* _create_solid_background(DeepinBackgroundCache* self, GdkRectangle r)
{
    DeepinBackgroundCachePrivate* priv = self->priv;

    gchar* color_str = g_settings_get_string(priv->bg_settings, GSETTINGS_PRIM_CLR);
    GdkRGBA clr;
    gdk_rgba_parse(&clr, color_str);
    g_free(color_str);

    cairo_surface_t* bg = cairo_image_surface_create(CAIRO_FORMAT_RGB24, r.width, r.height);
    cairo_t* cr = cairo_create(bg);
    cairo_set_source_rgba(cr, clr.red, clr.green, clr.blue, clr.alpha);
    cairo_paint(cr);
    cairo_destroy(cr);

    return bg;
}

static void deepin_background_cache_load_background(DeepinBackgroundCache* self)
{
    DeepinBackgroundCachePrivate* priv = self->priv;

    gchar* uri = g_settings_get_string(priv->bg_settings, GSETTINGS_BG_KEY);
    meta_verbose("uri: %s\n", uri);

    GdkScreen* screen = gdk_screen_get_default();
    gint n_monitors = gdk_screen_get_n_monitors(screen);

    gchar* scheme = g_uri_parse_scheme(uri);
    GFile* f = NULL;
    GdkPixbuf* pixbuf = NULL;
    gchar* path = NULL;

    priv->solid_mode = TRUE;

    if (!scheme || g_str_equal(scheme, "file")) {
        f = g_file_new_for_uri(uri);
        if (!f) goto _next;

        path = g_file_get_path(f);
        GError* error = NULL;
        pixbuf = gdk_pixbuf_new_from_file(path, &error);

        if (!pixbuf) {
            meta_verbose("%s", error->message);
            g_error_free(error);
            goto _next;
        }

        priv->solid_mode = FALSE;
    }

_next:
    for (int monitor = 0; monitor < n_monitors; monitor++) {
        GdkRectangle r;
        cairo_surface_t* background;

        gdk_screen_get_monitor_geometry(screen, monitor, &r);

        if (!priv->solid_mode) {
            GdkPixbuf* scaled_pixbuf = _do_scale(self, pixbuf, r.width, r.height);

            background = gdk_cairo_surface_create_from_pixbuf(scaled_pixbuf, 1.0, NULL);
            if (!background || cairo_surface_status(background) != CAIRO_STATUS_SUCCESS) {
                meta_verbose("%s create surface failed", __func__);
                if (background) g_clear_pointer(&background, cairo_surface_destroy);
            }

            g_object_unref(scaled_pixbuf);

        } else {
            background = _create_solid_background(self, r);
        }

        ScaledCacheInfo* sci = g_slice_new(ScaledCacheInfo);
        sci->scale = 1.0;
        sci->monitor = monitor;
        sci->surface = background;
        priv->caches = g_list_append(priv->caches, sci);

        meta_verbose("%s: create scaled(1.0) for monitor #%d\n", __func__, monitor);
    }

    if (path) g_free(path);
    if (pixbuf) g_object_unref(pixbuf);
    if (scheme) g_free(scheme);
    if (f) g_object_unref(f);
    g_free(uri);
}

static void deepin_background_cache_settings_chagned(GSettings *settings,
        gchar* key, gpointer user_data)
{
    if (g_str_equal(key, GSETTINGS_BG_KEY) || g_str_equal(key, GSETTINGS_PRIM_CLR)) {
        deepin_background_cache_flush((DeepinBackgroundCache*)user_data);
        deepin_background_cache_load_background((DeepinBackgroundCache*)user_data);
        deepin_message_hub_desktop_changed();
    }
}

static void on_screen_resized(DeepinMessageHub* hub, MetaScreen* screen,
        DeepinBackgroundCache* self)
{
    deepin_background_cache_flush(self);
    deepin_background_cache_load_background(self);
    deepin_message_hub_desktop_changed();
}

static void deepin_background_cache_init (DeepinBackgroundCache *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_BACKGROUND_CACHE, DeepinBackgroundCachePrivate);

    self->priv->caches = NULL;

    self->priv->bg_settings = g_settings_new(BACKGROUND_SCHEMA);
    g_signal_connect(G_OBJECT(self->priv->bg_settings), "changed",
            (GCallback)deepin_background_cache_settings_chagned, self);

    g_signal_connect(G_OBJECT(deepin_message_hub_get()),
            "screen-changed", (GCallback)on_screen_resized, self);
}

static void deepin_background_cache_finalize (GObject *object)
{
    DeepinBackgroundCachePrivate* priv = DEEPIN_BACKGROUND_CACHE(object)->priv;

    deepin_background_cache_flush(DEEPIN_BACKGROUND_CACHE(object));
    
    if (priv->bg_settings) {
        g_clear_pointer(&priv->bg_settings, g_object_unref);
    }

	G_OBJECT_CLASS (deepin_background_cache_parent_class)->finalize (object);
}

static void deepin_background_cache_class_init (DeepinBackgroundCacheClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DeepinBackgroundCachePrivate));

	object_class->finalize = deepin_background_cache_finalize;
}

DeepinBackgroundCache* deepin_get_background()
{
    if (!_the_cache) {
        _the_cache = (DeepinBackgroundCache*)g_object_new(DEEPIN_TYPE_BACKGROUND_CACHE, NULL);
        deepin_background_cache_load_background(_the_cache);
    }
    return _the_cache;
}

cairo_surface_t* deepin_background_cache_get_surface(gint monitor, double scale)
{
    DeepinBackgroundCachePrivate* priv = deepin_get_background()->priv;
    cairo_surface_t* base = NULL;

    GList* l = priv->caches;
    while (l) {
        ScaledCacheInfo* sci = (ScaledCacheInfo*)l->data;
        if (sci->monitor == monitor) {
            if (sci->scale == scale) {
                meta_verbose("%s: reuse scaled(%f) for monitor #%d\n", __func__, scale, monitor);
                return sci->surface;
            } else if (sci->scale == 1.0) 
                base = sci->surface;
        }
        l = l->next;
    }
    
    gint w = scale * cairo_image_surface_get_width(base),
         h = scale * cairo_image_surface_get_height(base);
    cairo_surface_t* surf = cairo_image_surface_create(
            cairo_image_surface_get_format(base),
            w, h);
    cairo_t* cr = cairo_create(surf);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, base, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    ScaledCacheInfo* sci = g_slice_new(ScaledCacheInfo);
    sci->scale = scale;
    sci->monitor = monitor;
    sci->surface = surf;
    priv->caches = g_list_append(priv->caches, sci);

    meta_verbose("%s: create scaled(%f) for monitor #%d\n", __func__, scale, monitor);
    return sci->surface;
}

