/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */


/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#define _GNU_SOURCE
#include <config.h>
#include <util.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gio/gdesktopappinfo.h>
#include <libbamf/libbamf.h>
#include "deepin-design.h"
#include "deepin-message-hub.h"
#include "boxes.h"
#include "../core/window-private.h"

static GtkCssProvider* _deepin_css_provider = NULL;

static struct {
    int entry_count, max_width;
    float box_width, box_height;
    float item_width, item_height;
    int items_each_row;
} cached = {0, 0, 0, 0, 0, 0, 0};

void calculate_preferred_size(gint entry_count, gint max_width,
        float* box_width, float* box_height, float* item_width, float* item_height,
        int* max_items_each_row)
{
    if (cached.entry_count == entry_count && cached.max_width == max_width) {
        if (box_width) *box_width = cached.box_width;
        if (box_height) *box_height = cached.box_height;
        if (item_width) *item_width = cached.item_width;
        if (item_height) *item_height = cached.item_height;
        if (max_items_each_row) *max_items_each_row = cached.items_each_row;
        return;
    }

    float bw, bh, iw, ih;
    gint cols;

    int prefered_width, prefered_height;
    deepin_switcher_get_prefer_size(&prefered_width, &prefered_height);

    iw = prefered_width;
    ih = prefered_height;

    cols = (int) max_width / iw;
    if (entry_count < cols) {
        bw = iw * entry_count;
    } else {
        bw = iw * cols;
    }

    int rows = (entry_count + cols - 1) / cols;
    bh = (ih + SWITCHER_ROW_SPACING) * rows - SWITCHER_ROW_SPACING;

    cached.box_width = bw;
    cached.box_height = bh;
    cached.item_width = iw;
    cached.item_height = ih;
    cached.items_each_row = cols;
    cached.entry_count = entry_count;
    cached.max_width = max_width;

    if (box_width) *box_width = cached.box_width;
    if (box_height) *box_height = cached.box_height;
    if (item_width) *item_width = cached.item_width;
    if (item_height) *item_height = cached.item_height;
    if (max_items_each_row) *max_items_each_row = cached.items_each_row;
}

GtkCssProvider* deepin_get_default_css_provider(void)
{
    if (_deepin_css_provider) return _deepin_css_provider;
    GtkCssProvider* css_style = gtk_css_provider_new();

    GFile* f = g_file_new_for_path(METACITY_PKGDATADIR "/deepin-wm.css");
    GError* error = NULL;
    if (!gtk_css_provider_load_from_file(css_style, f, &error)) {
        meta_topic(META_DEBUG_UI, "load css failed: %s", error->message);
        g_error_free(error);
        return NULL;
    }

    _deepin_css_provider = css_style;

    g_object_unref(f);
    return _deepin_css_provider;
}

void deepin_setup_style_class(GtkWidget* widget, const char* class_name)
{
    GtkStyleContext* style_ctx = gtk_widget_get_style_context(widget);

    GtkCssProvider* css_style = deepin_get_default_css_provider();

    gtk_style_context_add_provider(style_ctx,
            GTK_STYLE_PROVIDER(css_style), GTK_STYLE_PROVIDER_PRIORITY_USER);
    gtk_style_context_add_class(style_ctx, class_name);
}

static char ** xdg_dirs = NULL;

static char* build_desktop_path_for (const char* appid)
{
    if (xdg_dirs == NULL) {
        const char* xdg_dirs_string = g_getenv ("XDG_DATA_DIRS");
        if (!xdg_dirs_string) {
            xdg_dirs_string = "/usr/share";
        }
        xdg_dirs = g_strsplit (xdg_dirs_string, ":", 0);
    }

    char *target = NULL;
    const char** sp = &xdg_dirs[0];
    while (*sp != NULL) {
        char *path = g_strdup_printf ("%s/applications/%s.desktop", *sp, appid);
        if (access(path, F_OK) == 0) {
            target = path;
            break;
        }
        g_free (path);
        sp++;
    }

    return target;
}

static GdkPixbuf* get_icon_for_flatpak_app (const char* appid, int size)
{
    GdkPixbuf* image = NULL;

    const char *idx = strchr(appid+4, '/');
    if (!idx) idx = appid+4;
    char *id = strndupa(appid+4, idx - appid - 4);

    char *desktop_path = build_desktop_path_for(id);
    GDesktopAppInfo* appinfo = g_desktop_app_info_new_from_filename(desktop_path);
    if (appinfo) {
        GtkIconInfo* iconinfo = NULL;
        GIcon* icon = NULL;
        GtkIconTheme* theme = gtk_icon_theme_get_default();
        icon = g_app_info_get_icon(appinfo);
        if (icon) {
            iconinfo = gtk_icon_theme_lookup_by_gicon(theme, icon, size, 0);
            if (iconinfo) {
                image = gtk_icon_info_load_icon(iconinfo, NULL);
            }
        }

        g_object_unref(appinfo);
        if (iconinfo) g_object_unref(iconinfo);
    }
    g_free(desktop_path);

    return image;
}

static GdkPixbuf* get_icon_from_desktop_file(const char* desktop_filename, int icon_size)
{
    GdkPixbuf* image = NULL;
    GIcon* icon = NULL;
    GtkIconInfo* iconinfo = NULL;
    GtkIconTheme* theme = gtk_icon_theme_get_default();

    meta_verbose("get icon from %s");

    GDesktopAppInfo* appinfo = g_desktop_app_info_new_from_filename(desktop_filename);
    if (appinfo) {
        icon = g_app_info_get_icon(appinfo);
        if (icon) {
            iconinfo = gtk_icon_theme_lookup_by_gicon(theme, icon, icon_size, 
                    GTK_ICON_LOOKUP_FORCE_SIZE);
            if (iconinfo) {
                image = gtk_icon_info_load_icon(iconinfo, NULL);
            }
        }

        g_object_unref(appinfo);
    }
    if (iconinfo) g_object_unref(iconinfo);

    if (image) return image;
}

GdkPixbuf* meta_window_get_application_icon(MetaWindow* window, int icon_size)
{
    GdkPixbuf* image = NULL;
    if (window->flatpak_appid != NULL) {
        image = get_icon_for_flatpak_app(window->flatpak_appid, icon_size);
        if (image) return image;
    }

    if (g_strcmp0(window->res_class, "Wine") == 0) {
        int pid = window->net_wm_pid;
        if (pid > 0) {
            char* proc_env = g_strdup_printf ("/proc/%d/environ", pid);
            if (access(proc_env, F_OK) < 0) {
                goto out;
            }

            FILE* fp = fopen(proc_env, "r");
            long sz = 0, cap = 1024;
            char *buf = (char*)malloc(cap);

            char buf2[128];
            long len = 0;
            while ((len = fread(buf2, 1, 128, fp)) != 0) {
                if (sz + len > cap) {
                    cap *= 2;
                    buf = (char*)realloc(buf, cap);
                }
                memcpy(buf+sz, buf2, len);
                sz += len;
            }

            char* s = buf;
            while (s-buf < sz) {
                int len = strlen(s);

                char *sp = strchr(s, '=');
                if (sp == NULL) break;
                *sp = '\0';
                sp++;
                if (g_strcmp0(s, "GIO_LAUNCHED_DESKTOP_FILE") == 0) {
                    image = get_icon_from_desktop_file(sp, icon_size);
                    break;
                }
                s += len+1;
            }

error:
            free(buf);
            fclose(fp);

            if (image) return image;
        }
    }


    BamfMatcher* matcher = bamf_matcher_get_default();
    BamfApplication* app = bamf_matcher_get_application_for_xid(matcher, window->xwindow);

    GtkIconTheme* theme = gtk_icon_theme_get_default();

out:
    if (app) {
        const gchar* desktop_filename = bamf_application_get_desktop_file(app);
        if (desktop_filename) {
            image = get_icon_from_desktop_file(desktop_filename, icon_size);
            if (image) return image;
        }
    }

    if (!image && window->icon_name) {
        image = gtk_icon_theme_load_icon(theme, window->icon_name, icon_size, 0, NULL);
    }

    if (!image && window->icon) {
        image = window->icon;
        g_object_ref(window->icon);
    }

    // get icon for application that runs under terminal through wnck
    if (image == NULL) {
        meta_verbose("WM_CLASS: %s, %s", window->res_name, window->res_class);

        /* try to load icon from res_class first, cause window->icon may
         * contain a broken one 
         **/
        char* icon_name = g_ascii_strdown(window->res_class, -1);
        image = gtk_icon_theme_load_icon(theme, icon_name, icon_size, 0, NULL);
        g_free(icon_name);
    }

    if (!image) {
        image = gtk_icon_theme_load_icon(theme, "application-default-icon", icon_size, 0, NULL);
    }

    if (!image) {
        image = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, icon_size, icon_size);
        gdk_pixbuf_fill(image, 0x00000000);
    }

    return image;
}

#define SWITCHER_ITEM_SHAPE_PADDING 16

void deepin_switcher_get_inner_prefer_size(int *width, int *height)
{
    int iw, ih;

    double sx = deepin_message_hub_get_screen_scale ();

    deepin_switcher_get_prefer_size(&iw, &ih);

    int w = (iw - SWITCHER_ITEM_SHAPE_PADDING * 2);
    int h = (ih - SWITCHER_ITEM_SHAPE_PADDING * 2);

    if (width) *width = w;
    if (height) *height = h;
}

void deepin_switcher_get_prefer_size(int *width, int *height)
{
    double sx = deepin_message_hub_get_screen_scale ();
    if (width) *width = 128 * sx;
    if (height) *height = 128 * sx;
}

