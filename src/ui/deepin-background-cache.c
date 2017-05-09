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
#include <string.h>
#include <stdlib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <cairo.h>
#include <json-glib/json-glib.h>
#include "deepin-background-cache.h"
#include "deepin-message-hub.h"

#define BACKGROUND_SCHEMA "com.deepin.wrap.gnome.desktop.background"
#define EXTRA_BACKGROUND_SCHEMA "com.deepin.dde.appearance"
#define GSETTINGS_BG_KEY "picture-uri"
#define GSETTINGS_EXTRA_URIS "background-uris"
#define GSETTINGS_PRIM_CLR "primary-color"

static const char* fallback_background_name = "/usr/share/backgrounds/default_background.jpg";

typedef struct _ScaledCacheInfo
{
    gint monitor;
    gint workspace;
    double scale;
    cairo_surface_t* surface;
} ScaledCacheInfo;

struct _DeepinBackgroundCachePrivate
{
    GList *caches;

    GList *defaults; // caches for default, monitor&workspace is useless

    GList *preinstalled_wallpapers;
    GDBusProxy *appearance_intf;
    char *default_uri;

    GSettings *bg_settings;
    GSettings *extra_settings;
};

static DeepinBackgroundCache* _the_cache = NULL;


G_DEFINE_TYPE (DeepinBackgroundCache, deepin_background_cache, G_TYPE_OBJECT);

static void _clear_cache_list(GList **caches) 
{
    if (caches == NULL) return;

    GList* l = *caches;
    while (l) {
        ScaledCacheInfo* sci = (ScaledCacheInfo*)l->data;
        cairo_surface_destroy(sci->surface);
        g_slice_free(ScaledCacheInfo, sci);
        l = l->next;
    }
    g_list_free(*caches);
    *caches = NULL;
}

static void deepin_background_cache_flush(DeepinBackgroundCache* self)
{
    DeepinBackgroundCachePrivate* priv = self->priv;
    if (priv->caches) {
        _clear_cache_list(&priv->caches);
    }

    if (priv->defaults) {
        _clear_cache_list(&priv->defaults);
    }
}

static void deepin_background_cache_invalidate(DeepinBackgroundCache* self, int index)
{
    DeepinBackgroundCachePrivate* priv = self->priv;
    if (priv->caches) {
        GList* l = priv->caches;
        while (l) {
            ScaledCacheInfo* sci = (ScaledCacheInfo*)l->data;
            if (sci->workspace == index) {
                cairo_surface_destroy(sci->surface);
                g_slice_free(ScaledCacheInfo, sci);

                GList *old = l;
                l = l->next;
                priv->caches = g_list_remove_link(priv->caches, old);
                g_list_free(old);
            } else {
                l = l->next;
            }
        }
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

typedef char* (get_picture_filename_callback)(DeepinBackgroundCache *, int, int);

static char* get_picture_filename_cb(DeepinBackgroundCache *self, int monitor_index, int workspace_index)
{
    DeepinBackgroundCachePrivate *priv = self->priv;
    
    char* filename = NULL;
    char** extra_uris = g_settings_get_strv(priv->extra_settings, GSETTINGS_EXTRA_URIS);
    int nr_uris = g_strv_length (extra_uris);

    char* uri = NULL;
    if (nr_uris > 0 && nr_uris > workspace_index) {
        uri = extra_uris[workspace_index];
    }

    if (uri == NULL || g_strcmp0(uri, "") == 0) {
        uri = priv->default_uri;
    }

    if (g_uri_parse_scheme (uri) != NULL) {
        GFile* file = g_file_new_for_uri (uri);
        if (g_file_query_exists(file, NULL)) {
            filename = g_file_get_path (file);
        }
        g_object_unref (file);

    } else {
        filename = g_strdup(uri);
    }

    g_strfreev(extra_uris);

    if (filename == NULL) {
        return g_strdup(fallback_background_name);
    }

    return filename;
}

static void _do_reload_background(DeepinBackgroundCache* self);

static void on_file_changed(GFileMonitor *monitor,
        GFile            *file,
        GFile            *other_file,
        GFileMonitorEvent event_type,
        gpointer          user_data)
{
    _do_reload_background(DEEPIN_BACKGROUND_CACHE(user_data));
}

static void deepin_background_cache_load_default_background(DeepinBackgroundCache* self)
{ 
    DeepinBackgroundCachePrivate *priv = self->priv;

    GdkPixbuf *pixbuf = NULL;
    GError* error = NULL;

    MetaScreen *screen = meta_get_display()->active_screen;
    int nr_ws = meta_screen_get_n_workspaces (screen);
    gchar* path = get_picture_filename_cb (self, 0, nr_ws);

    pixbuf = gdk_pixbuf_new_from_file(path, &error);
    if (!pixbuf) {
        meta_verbose("%s\n", error->message);
        g_free(path);
        g_error_free(error);
        return;
    }

    cairo_surface_t* background = gdk_cairo_surface_create_from_pixbuf(pixbuf, 1.0, NULL);
    if (!background || cairo_surface_status(background) != CAIRO_STATUS_SUCCESS) {
        meta_verbose("%s create surface failed", __func__);
        if (background) g_clear_pointer(&background, cairo_surface_destroy);
    }

    ScaledCacheInfo* sci = g_slice_new(ScaledCacheInfo);
    sci->scale = 1.0;
    sci->surface = background;
    priv->defaults = g_list_append(priv->defaults, sci);

    g_free(path);
    g_object_unref(pixbuf);
}

static void deepin_background_cache_load_background_for_workspace(DeepinBackgroundCache* self,
        int workspace_index, get_picture_filename_callback* get_picture_filename)
{ 
    DeepinBackgroundCachePrivate *priv = self->priv;

    GdkScreen* gscreen = gdk_screen_get_default();
    gint n_monitors = gdk_screen_get_n_monitors(gscreen);

    gboolean solid_mode = TRUE;
    GdkPixbuf *pixbuf = NULL;
    GError* error = NULL;

    gchar* path = get_picture_filename (self, 0, workspace_index);
    meta_verbose("uri for workspace %d: %s\n", workspace_index, path);
    if (!path) {
        goto _next;
    }

    pixbuf = gdk_pixbuf_new_from_file(path, &error);
    if (!pixbuf) {
        meta_verbose("%s\n", error->message);
        g_free(path);
        path = NULL;
        g_error_free(error);
        goto _next;
    }

    solid_mode = FALSE;

_next:
    for (int monitor = 0; monitor < n_monitors; monitor++) {
        GdkRectangle r;
        cairo_surface_t* background;

        gdk_screen_get_monitor_geometry(gscreen, monitor, &r);

        if (!solid_mode) {
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
        sci->workspace = workspace_index;
        sci->surface = background;
        priv->caches = g_list_append(priv->caches, sci);

        meta_verbose("%s: create scaled(1.0) for monitor #%d, workspace %d\n", __func__,
                monitor, workspace_index);
    }

    if (path) g_free(path);
    if (pixbuf) g_object_unref(pixbuf);
}

static void deepin_background_cache_load_background(DeepinBackgroundCache* self)
{
    DeepinBackgroundCachePrivate* priv = self->priv;
    MetaScreen *screen = meta_get_display()->active_screen;
    int nr_ws = meta_screen_get_n_workspaces (screen);

    for (int workspace_index = 0; workspace_index < nr_ws; workspace_index++) {
        deepin_background_cache_load_background_for_workspace(self, workspace_index,
                get_picture_filename_cb);
    }
}

static void _do_reload_background(DeepinBackgroundCache* self)
{
    DeepinBackgroundCachePrivate* priv = self->priv;

    deepin_background_cache_flush(self);
    deepin_background_cache_load_background(self);
    deepin_background_cache_load_default_background(self);
    deepin_message_hub_desktop_changed();
}

static void deepin_background_cache_settings_chagned(GSettings *settings,
        gchar* key, gpointer user_data)
{
    if (g_str_equal(key, GSETTINGS_BG_KEY) || g_str_equal(key, GSETTINGS_PRIM_CLR)) {
        _do_reload_background((DeepinBackgroundCache*)user_data);
    }
}

static void on_screen_changed(DeepinMessageHub* hub, MetaScreen* screen,
        DeepinBackgroundCache* self)
{
    _do_reload_background(self);
}

// change background for workspace index, this may not expand array
static void change_background (DeepinBackgroundCache *self, int index, const char* uri)
{
    DeepinBackgroundCachePrivate *priv = self->priv;
    MetaScreen *screen = meta_get_display()->active_screen;

    int nr_ws = meta_screen_get_n_workspaces (screen);
    if (index > nr_ws) return;

    char* old = get_picture_filename_cb (self, 0, index);
    if (g_strcmp0(old, uri) == 0) {
        g_free(old);
        return;
    }
    g_free(old);

    char** extra_uris = g_settings_get_strv(priv->extra_settings, GSETTINGS_EXTRA_URIS);
    int nr_uris = g_strv_length (extra_uris);

    int new_size = nr_uris;
    // keep sync with workspace length
    if (nr_uris > nr_ws) {
        new_size = nr_ws;
    } else if (nr_uris < nr_ws) {
        int oldsz = nr_uris;
        new_size = nr_ws;

        char **old = extra_uris;
        extra_uris = (char**)g_realloc(extra_uris, sizeof(char*) * (nr_ws+1));

        for (int i = oldsz; i < nr_ws; i++) {
            extra_uris[i] = g_strdup(priv->default_uri);
        }
    }

    extra_uris[index] = g_strdup(uri);
    extra_uris[new_size] = NULL;
    g_settings_set_strv (priv->extra_settings, "background-uris", extra_uris);

    g_strfreev(extra_uris);

    deepin_background_cache_invalidate(self, index);
    deepin_background_cache_load_background_for_workspace(self, index,
            get_picture_filename_cb);
    deepin_message_hub_desktop_changed();
}

void deepin_change_background (int index, const char* uri)
{
    DeepinBackgroundCache *self = deepin_get_background ();
    change_background (self, index, uri);
}

static const char *transient_uri = NULL;

static char* get_transient_filename_cb(DeepinBackgroundCache *self, int mon, int ws)
{
    if (transient_uri)
        return g_strdup(transient_uri);

    return NULL;
}

void deepin_change_background_transient (int index, const char* uri)
{
    DeepinBackgroundCache *self = deepin_get_background ();

    transient_uri = uri;

    deepin_background_cache_invalidate(self, index);
    deepin_background_cache_load_background_for_workspace(self, index,
            get_transient_filename_cb);
    deepin_message_hub_desktop_changed();

    transient_uri = NULL;
}

char* deepin_get_background_uri (int index)
{
    DeepinBackgroundCache *self = deepin_get_background ();
    return get_picture_filename_cb (self, 0, index);
}

static void on_workspace_added(DeepinMessageHub* hub, gint index,
        DeepinBackgroundCache* self)
{
    change_background (self, index, self->priv->default_uri);
}

static gboolean on_idle_emit_change(gpointer data)
{
    deepin_message_hub_desktop_changed();
    return G_SOURCE_REMOVE;
}

static void on_workspace_removed(DeepinMessageHub* hub, gint index,
        DeepinBackgroundCache* self)
{
    DeepinBackgroundCachePrivate *priv = self->priv;
    MetaScreen *screen = meta_get_display()->active_screen;

    int nr_ws = meta_screen_get_n_workspaces (screen);
    if (index > nr_ws) return;

    char** extra_uris = g_settings_get_strv(priv->extra_settings, GSETTINGS_EXTRA_URIS);
    int nr_uris = g_strv_length (extra_uris);

    if (index >= nr_uris) {
        goto cleanup;
    }

    char **new_extra_uris = (char**)g_malloc(sizeof(char*) * (nr_ws+1));
    for (int i = 0; i < index; i++) {
        new_extra_uris[i] = g_strdup(extra_uris[i]);
    }

    for (int i = index; i < nr_ws; i++) {
        if (i+1 >= nr_uris) {
            new_extra_uris[i] = g_strdup(priv->default_uri);
        } else {
            new_extra_uris[i] = g_strdup(extra_uris[i+1]);
        }
    }
    new_extra_uris[nr_ws] = NULL;

    g_settings_set_strv (priv->extra_settings, "background-uris", new_extra_uris);
    g_strfreev(new_extra_uris);

    for (int i = index; i < nr_ws; i++) {
        deepin_background_cache_invalidate(self, i);
        deepin_background_cache_load_background_for_workspace(self, i, get_picture_filename_cb);
    }

    g_idle_add(on_idle_emit_change, NULL);

cleanup:
    g_strfreev(extra_uris);
}

static void reorder_workspace_background(DeepinBackgroundCache *self, int from, int to)
{
    DeepinBackgroundCachePrivate *priv = self->priv;
    MetaScreen *screen = meta_get_display()->active_screen;

    meta_verbose("%s: %d <-> %d\n", __func__, from, to);
    int nr_ws = meta_screen_get_n_workspaces (screen);

    char** extra_uris = g_settings_get_strv(priv->extra_settings, GSETTINGS_EXTRA_URIS);
    int nr_uris = g_strv_length (extra_uris);

    // keep sync with workspace length
    g_return_if_fail (nr_uris == nr_ws);

    char **new_extra_uris = (char**)g_malloc(sizeof(char*) * (nr_ws+1));
    for (int i = 0; i < nr_ws; i++) {
        new_extra_uris[i] = g_strdup(extra_uris[i]);
    }

    char *tmp = new_extra_uris[from];
    int d = from < to ? 1 : -1;
    for (int k = from+d; d > 0 ? k <= to : k >= to; k += d) {
        new_extra_uris[k-d] = new_extra_uris[k];
    }
    new_extra_uris[to] = tmp;
    new_extra_uris[nr_ws] = 0;

    g_settings_set_strv (priv->extra_settings, "background-uris", new_extra_uris);

    g_strfreev(new_extra_uris);
    g_strfreev(extra_uris);

    for (int k = from; d > 0 ? k <= to : k >= to; k += d) {
        deepin_background_cache_invalidate(self, k);
        deepin_background_cache_load_background_for_workspace(self, k, get_picture_filename_cb);
    }
    deepin_message_hub_desktop_changed();
}

static void on_workspace_reordered(DeepinMessageHub* hub, gint index, 
        int new_index, DeepinBackgroundCache* self)
{
    reorder_workspace_background(self, index, new_index);
}

static void deepin_background_cache_init (DeepinBackgroundCache *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_BACKGROUND_CACHE, DeepinBackgroundCachePrivate);

    self->priv->caches = NULL;
    self->priv->preinstalled_wallpapers = NULL;
    self->priv->default_uri = g_strdup_printf("file://%s", fallback_background_name);
    self->priv->appearance_intf = NULL;

    self->priv->bg_settings = g_settings_new(BACKGROUND_SCHEMA);
    self->priv->extra_settings = g_settings_new(EXTRA_BACKGROUND_SCHEMA);
    g_signal_connect(G_OBJECT(self->priv->bg_settings), "changed",
            (GCallback)deepin_background_cache_settings_chagned, self);
    g_signal_connect(G_OBJECT(self->priv->extra_settings), "changed",
            (GCallback)deepin_background_cache_settings_chagned, self);

    g_object_connect(G_OBJECT(deepin_message_hub_get()),
            "signal::screen-changed", (GCallback)on_screen_changed, self,
            "signal::workspace-added", (GCallback)on_workspace_added, self,
            "signal::workspace-removed", (GCallback)on_workspace_removed, self,
            "signal::workspace-reordered", (GCallback)on_workspace_reordered, self,
            NULL);
}

static void deepin_background_cache_finalize (GObject *object)
{
    DeepinBackgroundCachePrivate* priv = DEEPIN_BACKGROUND_CACHE(object)->priv;

    deepin_background_cache_flush(DEEPIN_BACKGROUND_CACHE(object));

    if (priv->extra_settings) {
        g_clear_pointer(&priv->extra_settings, g_object_unref);
    }
    
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
        deepin_background_cache_load_default_background(_the_cache);
    }
    return _the_cache;
}

cairo_surface_t* deepin_background_cache_get_surface(gint monitor, gint workspace, double scale)
{
    DeepinBackgroundCachePrivate* priv = deepin_get_background()->priv;
    cairo_surface_t* base = NULL;

    GList* l = priv->caches;
    while (l) {
        ScaledCacheInfo* sci = (ScaledCacheInfo*)l->data;
        if (sci->monitor == monitor && sci->workspace == workspace) {
            if (sci->scale == scale) {
                meta_verbose("%s: reuse scaled(%f) for monitor #%d, workspace #%d\n", __func__,
                        scale, monitor, workspace);
                return sci->surface;
            } else if (sci->scale == 1.0) 
                base = sci->surface;
        }
        l = l->next;
    }
    
    gint w = scale * cairo_image_surface_get_width(base),
         h = scale * cairo_image_surface_get_height(base);
    cairo_surface_t* surf = cairo_image_surface_create(
            cairo_image_surface_get_format(base), w, h);
    cairo_t* cr = cairo_create(surf);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, base, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    ScaledCacheInfo* sci = g_slice_new(ScaledCacheInfo);
    sci->scale = scale;
    sci->monitor = monitor;
    sci->workspace = workspace;
    sci->surface = surf;
    priv->caches = g_list_append(priv->caches, sci);

    meta_verbose("%s: create scaled(%f) for monitor #%d, workspace #%d\n", __func__, 
            scale, monitor, workspace);
    return sci->surface;
}

// right now, now need to cache scales at all
cairo_surface_t* deepin_background_cache_get_default(double scale)
{
    DeepinBackgroundCache* self = deepin_get_background();
    DeepinBackgroundCachePrivate* priv = self->priv;

    GList* l = priv->defaults;
    if (l) {
        ScaledCacheInfo* sci = (ScaledCacheInfo*)l->data;
        return sci->surface;
    }
    return NULL;
}

void deepin_background_cache_request_new_default_uri ()
{
    DeepinBackgroundCache *self = deepin_get_background ();
    DeepinBackgroundCachePrivate *priv = self->priv;

    GError *error = NULL;
    if (priv->appearance_intf == NULL) {
        char *json_str = NULL;
        GList *nodes = NULL;
        GVariant *ret = NULL;
        JsonNode *root = NULL;

        priv->appearance_intf = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, 
                G_DBUS_PROXY_FLAGS_NONE, NULL, 
                "com.deepin.daemon.Appearance", "/com/deepin/daemon/Appearance",
                "com.deepin.daemon.Appearance", NULL, &error);
        if (error) {
            meta_warning ("%s: %s\n", __func__, error->message);
            g_error_free(error);
            goto done;
        }

        ret = g_dbus_proxy_call_sync(priv->appearance_intf, "List", g_variant_new("(s)", 
                    "background"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
        if (error) {
            meta_warning ("%s: %s\n", __func__, error->message);
            g_error_free(error);
            goto done;
        }

        g_variant_get(ret, "(s)", &json_str);
        root = json_from_string (json_str, &error);
        if (error) {
            meta_warning ("%s: %s\n", __func__, error->message);
            g_error_free(error);
            goto done;
        }

        nodes = json_array_get_elements(json_node_get_array(root));
        GList *np = nodes;
        while (np) {
            JsonObject *obj = json_node_get_object(np->data);
            if (json_object_get_boolean_member(obj, "Deletable") == FALSE) {
                priv->preinstalled_wallpapers = g_list_append(priv->preinstalled_wallpapers,
                        strdup(json_object_get_string_member(obj, "Id")));
            }
            np = np->next;
        }

done:
        if (nodes) g_list_free(nodes);
        if (root) json_node_unref(root);
        if (json_str) free(json_str);
        if (ret) g_variant_unref(ret);
    }


    if (priv->preinstalled_wallpapers != NULL) {
        int index = g_random_int_range (0, g_list_length(priv->preinstalled_wallpapers));
        priv->default_uri = (char*)g_list_nth_data(priv->preinstalled_wallpapers, index);

        _clear_cache_list(&priv->defaults);
        deepin_background_cache_load_default_background(self);
    }
}
