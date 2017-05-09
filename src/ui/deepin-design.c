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
#include <util.h>
#include <math.h>
#include <gio/gdesktopappinfo.h>
#include <libbamf/libbamf.h>
#include "deepin-design.h"
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
    gboolean item_need_scale = FALSE;

    iw = SWITCHER_ITEM_PREFER_WIDTH;
    ih = SWITCHER_ITEM_PREFER_HEIGHT;

    cols = (int) ((max_width + SWITCHER_COLUMN_SPACING) / (SWITCHER_ITEM_PREFER_WIDTH + SWITCHER_COLUMN_SPACING));
    if (cols < SWITCHER_MIN_ITEMS_EACH_ROW && entry_count > cols ) {
        item_need_scale = TRUE;
        cols = MIN(SWITCHER_MIN_ITEMS_EACH_ROW, entry_count);
    }

    if (cols * SWITCHER_MAX_ROWS < entry_count) {
        cols = (int) ceilf((float) entry_count / SWITCHER_MAX_ROWS);
        item_need_scale = TRUE;
    }

    if (item_need_scale) {
        iw = (max_width + SWITCHER_COLUMN_SPACING) / cols - SWITCHER_COLUMN_SPACING;
        float item_scale = iw / SWITCHER_ITEM_PREFER_WIDTH;
        ih = SWITCHER_ITEM_PREFER_HEIGHT * item_scale;
    }

    if (entry_count < cols) {
        if (entry_count > 0) {
            bw = (iw + SWITCHER_COLUMN_SPACING) * entry_count - SWITCHER_COLUMN_SPACING;
        } else {
            g_assert(0);
            bw = 0;
        }
    } else {
        bw = (iw + SWITCHER_COLUMN_SPACING) * cols - SWITCHER_COLUMN_SPACING;
    }

    int rows = (entry_count + cols - 1) / cols;
    if (rows > 0) {
        bh = (ih + SWITCHER_ROW_SPACING) * rows - SWITCHER_ROW_SPACING;
    } else {
        g_assert(0);
        bh = 0;
    }

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

GdkPixbuf* meta_window_get_application_icon(MetaWindow* window, int icon_size)
{
    BamfMatcher* matcher = bamf_matcher_get_default();
    BamfApplication* app = bamf_matcher_get_application_for_xid(matcher, window->xwindow);

    GdkPixbuf* image = NULL;
    GtkIconTheme* theme = gtk_icon_theme_get_default();

    if (app) {
        const gchar* desktop_filename = bamf_application_get_desktop_file(app);
        if (desktop_filename) {
            GIcon* icon = NULL;
            GtkIconInfo* iconinfo = NULL;

            GDesktopAppInfo* appinfo = g_desktop_app_info_new_from_filename(desktop_filename);
            if (appinfo) {
                icon = g_app_info_get_icon(appinfo);
                if (icon) {
                    iconinfo = gtk_icon_theme_lookup_by_gicon(theme, icon, icon_size, 0);
                    if (iconinfo) {
                        image = gtk_icon_info_load_icon(iconinfo, NULL);
                    }
                }

                g_object_unref(appinfo);
            }
            if (iconinfo) g_object_unref(iconinfo);

            if (image) return image;
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

