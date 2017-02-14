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
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <util.h>
#include <gdk/gdkx.h>
#include "deepin-message-hub.h"
#include "deepin-corner-indicator.h"
#include "window-private.h"
#include "screen-private.h"
#include "display-private.h"

#define DEEPIN_ZONE_SETTINGS "com.deepin.dde.zone"
#define CORNER_THRESHOLD 150

struct _DeepinCornerIndicatorPrivate
{
    guint disposed: 1;
    MetaScreenCorner corner;
    MetaScreen *screen;

    char* key;
    char* action;

    int strokeCount;
    gboolean startRecord;

    float last_distance_factor;

    gint64 last_trigger_time; // 0 is invalid
    gint64 last_reset_time; // 0 is invalid

    cairo_surface_t *effect;

    GSettings *settings;
    uint polling_id;

    float opacity; // 0 - 1

    GdkDevice *pointer;
};

#define CORNER_SIZE 32

G_DEFINE_TYPE (DeepinCornerIndicator, deepin_corner_indicator, GTK_TYPE_WINDOW);

static void deepin_corner_indicator_settings_chagned(GSettings *settings,
        gchar* key, gpointer user_data)
{
    DeepinCornerIndicatorPrivate *priv = DEEPIN_CORNER_INDICATOR(user_data)->priv;
    if (priv->action != NULL) free(priv->action);
    priv->action = g_settings_get_string(priv->settings, priv->key);
}

static void deepin_corner_indicator_init (DeepinCornerIndicator *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_CORNER_INDICATOR, DeepinCornerIndicatorPrivate);

    DeepinCornerIndicatorPrivate *priv = self->priv;

    priv->key = NULL;
    priv->action = NULL;
    priv->strokeCount = 0;
    priv->startRecord = FALSE;
    priv->last_distance_factor = 0.0f;
    priv->opacity = priv->last_distance_factor;
    priv->last_reset_time = 0;
    priv->last_trigger_time = 0;

    priv->effect = NULL;
    
    priv->pointer = gdk_seat_get_pointer (gdk_display_get_default_seat ( gdk_display_get_default ()));
    priv->settings = g_settings_new(DEEPIN_ZONE_SETTINGS);
    g_signal_connect(G_OBJECT(priv->settings), "changed",
            (GCallback)deepin_corner_indicator_settings_chagned, self);
}

static void deepin_corner_indicator_finalize (GObject *object)
{
    DeepinCornerIndicatorPrivate *priv = DEEPIN_CORNER_INDICATOR (object)->priv;
    g_clear_pointer(&priv->effect, cairo_surface_destroy);
    g_clear_pointer(&priv->settings, g_object_unref);

    G_OBJECT_CLASS (deepin_corner_indicator_parent_class)->finalize (object);
}

static gboolean string_contains (const char *src, const char *pattern)
{
    if (!src || !pattern) return FALSE;
    return g_strstr_len(src, -1, pattern) != NULL;
}

static gboolean blocked (DeepinCornerIndicator *self)
{
    DeepinCornerIndicatorPrivate *priv = self->priv;

    if (priv->action == NULL || strlen(priv->action) == 0)
        return TRUE;

    MetaWindow *active_window = meta_display_get_focus_window (priv->screen->display);
    if (active_window == NULL) 
        return FALSE;

    gboolean isLauncherShowing = FALSE;
    meta_verbose("%s: title %s\n", __func__, active_window->title);
    if (active_window->title != NULL) {
        if (string_contains(active_window->title, "dde-launcher")) {
            isLauncherShowing = TRUE;
        }
    }

    if (isLauncherShowing) {
        if (string_contains(priv->action, "com.deepin.dde.Launcher")) {
            return FALSE;
        }
        meta_verbose ("launcher is showing, do not exec action\n");
        return TRUE;
    }

    return FALSE;
}

static gboolean is_app_in_list (DeepinCornerIndicator *self, int pid, const char **list)
{
    DeepinCornerIndicatorPrivate *priv = self->priv;

    char cmd[256] = {'\0'};
    char* proc = g_strdup_printf("/proc/%d/cmdline", pid);
    readlink(proc, cmd, sizeof cmd - 1);
    free(proc);

    const char **p = list;
    while (*p != NULL) {
        if (string_contains(cmd, *p)) {
            return TRUE;
        }
        p++;
    }
    return FALSE;
}

static gboolean should_perform_action (DeepinCornerIndicator *self)
{
    DeepinCornerIndicatorPrivate *priv = self->priv;

    char** black_list = g_settings_get_strv(priv->settings, "black-list");

    MetaWindow *active_window = meta_display_get_focus_window (priv->screen->display);
    if (active_window == NULL) 
        return TRUE;


    int pid = active_window->net_wm_pid;
    if (is_app_in_list(self, pid, black_list)) {
        meta_verbose("active window app in blacklist\n");
        g_strfreev (black_list);
        return FALSE;
    }
    g_strfreev (black_list);

    if (active_window->fullscreen) {
        char ** white_list = g_settings_get_strv(priv->settings, "white-list");
        if (is_app_in_list(self, pid, white_list)) {
            meta_verbose("active window is fullscreen, and in whiteList\n");
            g_strfreev (white_list);
            return TRUE;
        }
        meta_verbose("active window is fullscreen, and not in whiteList\n");
        g_strfreev (white_list);
        return FALSE;
    }

    return TRUE;
}

static void corner_leaved(DeepinCornerIndicator *self, MetaScreenCorner corner)
{
    DeepinCornerIndicatorPrivate *priv = self->priv;

    if (corner != priv->corner)
        return;

    if (priv->startRecord) {
        meta_verbose ("leave [%s]\n", priv->key);
        priv->strokeCount = 0;
        priv->startRecord = FALSE;
        priv->last_distance_factor = 0.0f;
        priv->opacity = priv->last_distance_factor;
        gtk_widget_queue_draw(GTK_WIDGET(self));

        if (priv->polling_id != 0) {
            g_source_remove (priv->polling_id);
            priv->polling_id = 0;
        }

        meta_screen_leave_corner(priv->screen, priv->corner);
    }
}

static gboolean inside_effect_region(DeepinCornerIndicator *self, GdkPoint pos)
{
    DeepinCornerIndicatorPrivate *priv = self->priv;

    int x1, x2, y1, y2;
    gdk_window_get_geometry (gtk_widget_get_window(self), &x1, &y1, &x2, &y2);
    x2 += x1;
    y2 += y1;

    return pos.x >= x1 && pos.x <= x2 && pos.y >= y1 && pos.y <= y2;
}

static float distance_factor(DeepinCornerIndicator *self, GdkPoint pos, float cx, float cy)
{
    DeepinCornerIndicatorPrivate *priv = self->priv;

    float ex = pos.x, ey = pos.y;

    float dx = (ex - cx);
    float dy = (ey - cy);
    float d = fabsf(dx) + fabsf(dy);

    float factor = d / (CORNER_SIZE * 2);
    return 1.0f - fmaxf(fminf(factor, 1.0f), 0.0f);
}

static float distance_factor_for_corner (DeepinCornerIndicator *self, GdkPoint pos)
{
    int x1, x2, y1, y2;
    gdk_window_get_geometry (gtk_widget_get_window(self), &x1, &y1, &x2, &y2);
    x2 += x1;
    y2 += y1;

    float d = 0.0f;
    switch (self->priv->corner) {
        case META_SCREEN_TOPLEFT:
            d = distance_factor (self, pos, x1, y1); break;

        case META_SCREEN_TOPRIGHT:
            d = distance_factor (self, pos, x2-1, y1); break;

        case META_SCREEN_BOTTOMLEFT:
            d = distance_factor (self, pos, x1, y2-1); break;

        case META_SCREEN_BOTTOMRIGHT:
            d = distance_factor (self, pos, x2-1, y2-1); break;
    }

    return d;
}

static gboolean reach_threshold (DeepinCornerIndicator *self, GdkPoint pos)
{
    DeepinCornerIndicatorPrivate *priv = self->priv;

    float d = distance_factor_for_corner (self, pos);

    gboolean hit = FALSE;
    if (priv->last_distance_factor != d) {
        priv->last_distance_factor = d;
        priv->opacity = priv->last_distance_factor;
        gtk_widget_queue_draw(GTK_WIDGET(self));
        hit = d == 1.0f;
        meta_verbose ("distance factor = %f\n", d);
    }
    return hit;
}

static gboolean at_trigger_point(DeepinCornerIndicator *self, GdkPoint pos)
{
    DeepinCornerIndicatorPrivate *priv = self->priv;

    return distance_factor_for_corner (self, pos) == 1.0f;
}

static void push_back (DeepinCornerIndicator *self, GdkPoint pos) 
{
    DeepinCornerIndicatorPrivate *priv = self->priv;

    float ex = pos.x, ey = pos.y;

    switch (priv->corner) {
        case META_SCREEN_TOPLEFT:
            ex += 1.0f; ey += 1.0f; break;

        case META_SCREEN_TOPRIGHT:
            ex -= 1.0f; ey += 1.0f; break;

        case META_SCREEN_BOTTOMLEFT:
            ex += 1.0f; ey -= 1.0f; break;

        case META_SCREEN_BOTTOMRIGHT:
            ex -= 1.0f; ey -= 1.0f; break;
    }

    gdk_device_warp(priv->pointer, gdk_screen_get_default(), ex, ey);
}

static gboolean on_delayed_action (DeepinCornerIndicator *self)
{
    GError *error = NULL;
    if (!g_spawn_command_line_async(self->priv->action, &error)) {
        g_warning("%s", error->message);
        g_error_free(error);
    }

    return FALSE;
}

static void perform_action (DeepinCornerIndicator *self)
{
    DeepinCornerIndicatorPrivate *priv = self->priv;

    if (!blocked(self) && should_perform_action(self)) {
        meta_verbose ("[%s]: action: %s\n", priv->key, priv->action);

        int d = 0;
        if (string_contains(priv->action, "com.deepin.dde.ControlCenter")) 
            d = g_settings_get_int(priv->settings, "delay");

        g_timeout_add(d, on_delayed_action, self);
    }
}

static void mouse_move(DeepinCornerIndicator *self, GdkPoint pos)
{
    DeepinCornerIndicatorPrivate *priv = self->priv;

    if (priv->startRecord) {
        if (!inside_effect_region(self, pos)) {
            corner_leaved(self, priv->corner);
            return;
        }

        if (blocked(self)) return;

        gint64 timestamp = g_get_monotonic_time () / 1000;
        if (reach_threshold(self, pos)) {
            if (priv->strokeCount > 1) {
                perform_action(self);
                priv->strokeCount = 0;
                priv->last_trigger_time = g_get_monotonic_time () / 1000;
                priv->last_reset_time = 0;

            } else if (priv->last_reset_time != 0) {
                if (timestamp - priv->last_reset_time >= CORNER_THRESHOLD) {
                    priv->strokeCount++;
                }

            } else if (priv->last_trigger_time == 0 || 
                    timestamp - priv->last_trigger_time >= CORNER_THRESHOLD) {
                priv->last_reset_time = g_get_monotonic_time () / 1000;
                priv->strokeCount++;
            }

        } else if (at_trigger_point(self, pos)) {
            if (priv->last_reset_time != 0 && 
                    timestamp - priv->last_reset_time >= CORNER_THRESHOLD) {
                priv->strokeCount++;
                // warp mouse cursor back a little
                push_back(self, pos);
            }
        }
    }
}


static gboolean polling_timeout (DeepinCornerIndicator *self)
{
    DeepinCornerIndicatorPrivate *priv = self->priv;

    GdkPoint pos;
    gdk_device_get_position (priv->pointer, NULL, &pos.x, &pos.y);
    if (priv->startRecord) mouse_move (self, pos);
    return TRUE;
}


static void corner_entered(DeepinMessageHub *hub, MetaScreenCorner corner,
        DeepinCornerIndicator *self)
{
    DeepinCornerIndicatorPrivate *priv = self->priv;

    if (corner != priv->corner)
        return;

    priv->startRecord = TRUE;
    meta_verbose ("enter [%s]\n", priv->key);

    if (priv->polling_id != 0) {
        g_source_remove (priv->polling_id);
        priv->polling_id = 0;
    }
    priv->polling_id = g_timeout_add(50, polling_timeout, self);
}

static gboolean deepin_corner_indicator_real_draw(GtkWidget *widget, cairo_t* cr)
{
    DeepinCornerIndicatorPrivate* priv = DEEPIN_CORNER_INDICATOR(widget)->priv;

#ifdef G_DEBUG
    cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
    cairo_paint(cr);
#endif

    if (priv->effect) {
        cairo_set_source_surface(cr, priv->effect, 0, 0);
        cairo_paint_with_alpha(cr, priv->opacity);
    }

    return TRUE;
}

static void deepin_corner_indicator_class_init (DeepinCornerIndicatorClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS (klass);

    g_type_class_add_private (klass, sizeof (DeepinCornerIndicatorPrivate));

    object_class->finalize = deepin_corner_indicator_finalize;

    widget_class->draw = deepin_corner_indicator_real_draw;
}

static GdkPixbuf* get_button_pixbuf (DeepinCornerIndicator *self)
{
    const char * icon_name = NULL;
    switch (self->priv->corner) {
        case META_SCREEN_TOPLEFT: icon_name = "topleft"; break;
        case META_SCREEN_TOPRIGHT: icon_name = "topright"; break;
        case META_SCREEN_BOTTOMLEFT: icon_name = "bottomleft"; break;
        case META_SCREEN_BOTTOMRIGHT: icon_name = "bottomright"; break;
    }

    GError *error = NULL;
    char *path = g_strdup_printf (METACITY_PKGDATADIR "/%s.png", icon_name);
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file (path, &error);
    if (!pixbuf) {
        g_warning ("%s\n", error->message);
        g_error_free (error);
        return NULL;
    }

    return pixbuf;
}

GtkWidget* deepin_corner_indicator_new (MetaScreen *screen, MetaScreenCorner corner,
        const char* key, int x, int y)
{
    GtkWidget *widget = g_object_new (DEEPIN_TYPE_CORNER_INDICATOR, NULL);

    DeepinCornerIndicator *self = DEEPIN_CORNER_INDICATOR (widget);

    self->priv->corner = corner;
    self->priv->screen = screen;
    self->priv->key = strdup(key);
    self->priv->action = g_settings_get_string(self->priv->settings, key);

    GdkPixbuf *pixbuf = get_button_pixbuf (self);
    if (pixbuf) {
        self->priv->effect = gdk_cairo_surface_create_from_pixbuf (pixbuf, 0, NULL);
        g_object_unref (pixbuf);
    }

    GdkVisual *visual = gdk_screen_get_rgba_visual(gdk_screen_get_default());
    if (visual) gtk_widget_set_visual(widget, visual);

    gtk_widget_set_app_paintable (widget, TRUE);
    gtk_window_set_default_size (widget, CORNER_SIZE, CORNER_SIZE);

    gtk_widget_realize (widget);
    gdk_window_set_override_redirect (gtk_widget_get_window (widget), TRUE);

    gdk_window_move_resize (gtk_widget_get_window (widget), x, y, CORNER_SIZE, CORNER_SIZE);
    gdk_window_raise (gtk_widget_get_window (widget));

    cairo_rectangle_int_t r = {0, 0, 0, 0};
    cairo_region_t *shape_region = cairo_region_create_rectangle (&r);
    gdk_window_input_shape_combine_region (gtk_widget_get_window (widget), 
            shape_region, 0, 0);
    cairo_region_destroy (shape_region);

    g_object_connect(G_OBJECT(deepin_message_hub_get()),
            "signal::screen-corner-entered", (GCallback)corner_entered, self, 
            NULL);

    return widget;
}

