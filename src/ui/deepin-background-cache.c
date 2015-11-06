/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-background-cache.c
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * metacity is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * metacity is distributed in the hope that it will be useful, but
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
#include "deepin-message-hub.h"

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

static void deepin_background_cache_flush(DeepinBackgroundCache* self)
{
    DeepinBackgroundCachePrivate* priv = self->priv;
    if (priv->background) {
        g_clear_pointer(&priv->background, cairo_surface_destroy);
    }

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

static GdkPixbuf* _do_scale(DeepinBackgroundCache* self, GdkPixbuf* pixbuf)
{
    GdkPixbuf* new_pixbuf;
    DeepinBackgroundCachePrivate* priv = self->priv;

    int pw = gdk_pixbuf_get_width(pixbuf), ph = gdk_pixbuf_get_height(pixbuf);

    if (pw == priv->fixed_width && ph == priv->fixed_height) return pixbuf;

    double scale = (double)priv->fixed_width / priv->fixed_height;

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

    new_pixbuf = gdk_pixbuf_scale_simple(subpixbuf, priv->fixed_width, 
            priv->fixed_height, GDK_INTERP_BILINEAR);

    g_object_unref(subpixbuf);
    g_object_unref(pixbuf);

    return new_pixbuf;
}

static void deepin_background_cache_load_background(DeepinBackgroundCache* self)
{
    DeepinBackgroundCachePrivate* priv = self->priv;

    gchar* uri = g_settings_get_string(priv->bg_settings, GSETTINGS_BG_KEY);
    meta_verbose("uri: %s\n", uri);

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
        GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(path, &error);

        if (!pixbuf) {
            meta_verbose("%s", error->message);
            g_error_free(error);
            goto _cleanup;
        }

        pixbuf = _do_scale(self, pixbuf);
        g_assert(!priv->background);

        priv->background = gdk_cairo_surface_create_from_pixbuf(pixbuf, 1.0, NULL);
    }

_cleanup:
    if (path) g_free(path);
    if (pixbuf) g_object_unref(pixbuf);
    if (scheme) g_free(scheme);
    if (f) g_object_unref(f);
    g_free(uri);
}

static void deepin_background_cache_settings_chagned(GSettings *settings,
        gchar* key, gpointer user_data)
{
    if (g_str_equal(key, GSETTINGS_BG_KEY)) {
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
            "screen-resized", (GCallback)on_screen_resized, self);
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
    cairo_surface_t* surf = cairo_image_surface_create(
            cairo_image_surface_get_format(priv->background),
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

    meta_verbose("%s: create scaled(%f) background", __func__, scale);
    return sci->surface;
}

