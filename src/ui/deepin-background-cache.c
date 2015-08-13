/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-background-cache.c
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * gtk-skeleton is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * gtk-skeleton is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <math.h>
#include <util.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <cairo.h>
#include "deepin-background-cache.h"

#define GSETTINGS_BG_KEY "picture-uri"
#define BACKGROUND_SCHEMA "com.deepin.wrap.gnome.desktop.background"

typedef struct _ScaledCacheInfo
{
    double scale;
    cairo_surface_t* surface;
} ScaledCacheInfo;

struct _DeepinBackgroundCachePrivate
{
    cairo_surface_t* background;
    GList* caches;

    gint fixed_width, fixed_height;
    GSettings *bg_settings;
};

static DeepinBackgroundCache* _the_cache = NULL;


G_DEFINE_TYPE (DeepinBackgroundCache, deepin_background_cache, G_TYPE_OBJECT);

static void deepin_background_cache_load_background(DeepinBackgroundCache* self)
{
    DeepinBackgroundCachePrivate* priv = self->priv;

    gchar* uri = g_settings_get_string(priv->bg_settings, GSETTINGS_BG_KEY);
    g_message("uri: %s", uri);

    GdkScreen* screen = gdk_screen_get_default();
    priv->fixed_width = gdk_screen_get_width(screen);
    priv->fixed_height = gdk_screen_get_height(screen);

    gchar* scheme = g_uri_parse_scheme(uri);
    GFile* f = NULL;
    GdkPixbuf* pixbuf = NULL;
    gchar* path = NULL;

    if (!scheme || g_str_equal(scheme, "file")) {
        f = g_file_new_for_uri(uri);
        if (!f) goto _cleanup;

        path = g_file_get_path(f);
        GError* error = NULL;
        GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_scale(path,
                priv->fixed_width,
                priv->fixed_height,
                FALSE, &error);

        if (!pixbuf) {
            g_message("%s", error->message);
            g_error_free(error);
            goto _cleanup;
        }

        if (!gdk_pixbuf_get_has_alpha(pixbuf)) {
            GdkPixbuf* orig = pixbuf;
            pixbuf = gdk_pixbuf_add_alpha(orig, FALSE, 0, 0, 0);
            g_object_unref(orig);
        }

        if (priv->background) {
            cairo_surface_destroy(priv->background);
        }

        priv->background = gdk_cairo_surface_create_from_pixbuf(pixbuf, 1.0, NULL);
        if (cairo_image_surface_get_format(priv->background) != CAIRO_FORMAT_ARGB32) {
            g_warning("%s: surface is not argb", __func__);
        }
    }

_cleanup:
    if (path) g_free(path);
    if (pixbuf) g_object_unref(pixbuf);
    if (f) g_object_unref(f);
    g_free(uri);
}

static void deepin_background_cache_settings_chagned(GSettings *settings,
        gchar* key, gpointer user_data)
{
    if (g_str_equal(key, GSETTINGS_BG_KEY)) {
        deepin_background_cache_load_background((DeepinBackgroundCache*)user_data);
    }
}

static void deepin_background_cache_init (DeepinBackgroundCache *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_BACKGROUND_CACHE, DeepinBackgroundCachePrivate);

    self->priv->caches = NULL;

    self->priv->bg_settings = g_settings_new(BACKGROUND_SCHEMA);
    g_signal_connect(G_OBJECT(self->priv->bg_settings), "changed",
            (GCallback)deepin_background_cache_load_background, self);
}

static void deepin_background_cache_finalize (GObject *object)
{
    DeepinBackgroundCachePrivate* priv = DEEPIN_BACKGROUND_CACHE(object)->priv;

    if (priv->background) {
        cairo_surface_destroy(priv->background);
    }
    
    GList* l = priv->caches;
    while (l) {
        ScaledCacheInfo* sci = (ScaledCacheInfo*)l->data;
        cairo_surface_destroy(sci->surface);
        g_slice_free(ScaledCacheInfo, sci);
        l = l->next;
    }
    g_list_free(priv->caches);

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

cairo_surface_t* deepin_background_cache_get_surface(double scale)
{
    DeepinBackgroundCachePrivate* priv = deepin_get_background()->priv;
    if (scale == 1.0) return priv->background;

    GList* l = priv->caches;
    while (l) {
        ScaledCacheInfo* sci = (ScaledCacheInfo*)l->data;
        if (sci->scale == scale) return sci->surface;
        l = l->next;
    }
    
    gint w = scale * priv->fixed_width, h = scale * priv->fixed_height;
    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            w, h);
    cairo_t* cr = cairo_create(surf);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, priv->background, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    ScaledCacheInfo* sci = g_slice_new(ScaledCacheInfo);
    sci->scale = scale;
    sci->surface = surf;
    priv->caches = g_list_append(priv->caches, sci);

    return sci->surface;
}

