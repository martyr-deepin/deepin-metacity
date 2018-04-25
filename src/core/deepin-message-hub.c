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
#include <util.h>
#include <gdk/gdkx.h>
#include "deepin-message-hub.h"
#include "window-private.h"

#define DEEPIN_XSETTINGS "com.deepin.xsettings"

static DeepinMessageHub* _the_hub = NULL;

struct _DeepinMessageHubPrivate
{
    guint disposed: 1;
    GSettings *xsettings;
    gdouble cached_scale;
};


enum
{
    SIGNAL_WORKSPACE_ADDED,
    SIGNAL_WORKSPACE_REMOVED,
    SIGNAL_WORKSPACE_SWITCHED,
    SIGNAL_WORKSPACE_REORDERED,
    SIGNAL_COMPOSITING_CHANGED,
    SIGNAL_WINDOW_REMOVED,
    SIGNAL_WINDOW_ADDED,
    SIGNAL_WINDOW_DAMAGED,
    SIGNAL_DESKTOP_CHANGED,
    SIGNAL_SCREEN_CHANGED,
    SIGNAL_ABOUT_TO_CHANGE_WORKSPACE,
    SIGNAL_WINDOW_ABOVE_STATE_CHANGED,
    SIGNAL_DRAG_END,
    SIGNAL_UNABLE_TO_OPERATE,
    SIGNAL_SCREEN_CORNER_ENTERED,
    SIGNAL_SCREEN_CORNER_LEAVED,
    SIGNAL_SCREEN_SCALE_CHANGED,

    LAST_SIGNAL
};


static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (DeepinMessageHub, deepin_message_hub, G_TYPE_OBJECT);

static void deepin_message_hub_settings_chagned(GSettings *settings,
        gchar* key, gpointer user_data)
{
    DeepinMessageHub* self = deepin_message_hub_get();
    gdouble scale = g_settings_get_double (self->priv->xsettings, "scale-factor");
    if (self->priv->cached_scale != scale) {
        self->priv->cached_scale = scale;
        g_signal_emit(self, signals[SIGNAL_SCREEN_SCALE_CHANGED], 0, scale);
    }
}

static void deepin_message_hub_init (DeepinMessageHub *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_MESSAGE_HUB, DeepinMessageHubPrivate);

    self->priv->xsettings = g_settings_new (DEEPIN_XSETTINGS);
    g_signal_connect (G_OBJECT(self->priv->xsettings), "changed",
            (GCallback)deepin_message_hub_settings_chagned, self);
    self->priv->cached_scale = g_settings_get_double (self->priv->xsettings, "scale-factor");
}

static void deepin_message_hub_finalize (GObject *object)
{
    DeepinMessageHub *self = DEEPIN_MESSAGE_HUB (object);
    g_clear_pointer (&self->priv->xsettings, g_object_unref);
    G_OBJECT_CLASS (deepin_message_hub_parent_class)->finalize (object);
}

void deepin_message_hub_window_added(MetaWindow* window)
{
    meta_verbose("%s: %s\n", __func__, window->desc);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_WINDOW_ADDED], 0, window);
}

void deepin_message_hub_window_removed(MetaWindow* window)
{
    meta_verbose("%s: %s\n", __func__, window->desc);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_WINDOW_REMOVED], 0, window);
}

void deepin_message_hub_window_damaged(MetaWindow* window, XRectangle* rects, int n)
{
    if (window == NULL || window->unmanaging || window->withdrawn)
        return;

    g_signal_emit(deepin_message_hub_get(),
            signals[SIGNAL_WINDOW_DAMAGED], 0, window);
}

void deepin_message_hub_desktop_changed(void)
{
    meta_verbose("%s\n", __func__);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_DESKTOP_CHANGED], 0);
}

void deepin_message_hub_window_about_to_change_workspace(
        MetaWindow* window, MetaWorkspace* workspace)
{
    meta_verbose("%s: move %s to workspace %s\n", __func__, window->desc, 
            meta_workspace_get_name(workspace));
    g_signal_emit(deepin_message_hub_get(),
            signals[SIGNAL_ABOUT_TO_CHANGE_WORKSPACE], 0, 
            window, workspace);
}
 
void deepin_message_hub_window_above_state_changed(MetaWindow* window, gboolean above)
{
    meta_verbose("%s: set %s above = %d\n", __func__, window->desc, above);
    g_signal_emit(deepin_message_hub_get(),
            signals[SIGNAL_WINDOW_ABOVE_STATE_CHANGED], 0, 
            window, above);
}

void deepin_message_hub_drag_end(void)
{
    meta_verbose("%s\n", __func__);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_DRAG_END], 0); 
}

void deepin_message_hub_unable_to_operate(MetaWindow* window)
{
    meta_verbose("%s\n", __func__);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_UNABLE_TO_OPERATE], 0, window); 
}

static void deepin_message_hub_class_init (DeepinMessageHubClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (DeepinMessageHubPrivate));

    object_class->finalize = deepin_message_hub_finalize;

    signals[SIGNAL_WINDOW_REMOVED] = g_signal_new ("window-removed",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[SIGNAL_WINDOW_ADDED] = g_signal_new ("window-added",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[SIGNAL_WINDOW_DAMAGED] = g_signal_new ("window-damaged",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[SIGNAL_DESKTOP_CHANGED] = g_signal_new ("desktop-changed",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 0);

    signals[SIGNAL_SCREEN_CHANGED] = g_signal_new ("screen-changed",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[SIGNAL_ABOUT_TO_CHANGE_WORKSPACE] = g_signal_new (
            "about-to-change-workspace",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 2,
            G_TYPE_POINTER, G_TYPE_POINTER);

    signals[SIGNAL_WINDOW_ABOVE_STATE_CHANGED] = g_signal_new (
            "window-above-state-changed",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 2,
            G_TYPE_POINTER, G_TYPE_BOOLEAN);

    signals[SIGNAL_DRAG_END] = g_signal_new (
            "drag-end",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 0);

    signals[SIGNAL_UNABLE_TO_OPERATE] = g_signal_new ("unable-to-operate",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[SIGNAL_WORKSPACE_REMOVED] = g_signal_new ("workspace-removed",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_INT);

    signals[SIGNAL_WORKSPACE_ADDED] = g_signal_new ("workspace-added",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_INT);

    signals[SIGNAL_WORKSPACE_SWITCHED] = g_signal_new ("workspace-switched",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

    signals[SIGNAL_COMPOSITING_CHANGED] = g_signal_new ("compositing-changed",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_INT);

    signals[SIGNAL_WORKSPACE_REORDERED] = g_signal_new ("workspace-reordered",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

    signals[SIGNAL_SCREEN_CORNER_ENTERED] = g_signal_new ("screen-corner-entered",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_INT);

    signals[SIGNAL_SCREEN_CORNER_LEAVED] = g_signal_new ("screen-corner-leaved",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_INT);

    signals[SIGNAL_SCREEN_SCALE_CHANGED] = g_signal_new ("screen-scaled",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_DOUBLE);
}

static void on_message_unable_to_operate(MetaWindow* window, gpointer data)
{
    GError* error = NULL;

    GDBusProxy* sound_effect = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, 
            G_DBUS_PROXY_FLAGS_NONE, NULL, 
            "com.deepin.daemon.SoundEffect", "/com/deepin/daemon/SoundEffect",
            "com.deepin.daemon.SoundEffect", NULL, &error);

    if (error) {
        meta_warning ("%s: %s\n", __func__, error->message);
        g_error_free(error);
        return;
    }

    g_dbus_proxy_call_sync(sound_effect, "PlaySystemSound", g_variant_new("(s)", "dialog-error"), 
            G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

    if (error) {
        meta_warning ("%s: %s\n", __func__, error->message);
        g_error_free(error);
    }

    g_object_unref(sound_effect);
}

void deepin_message_hub_register_to_session()
{
    GError* error = NULL;

    GDBusProxy* sm = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, 
            G_DBUS_PROXY_FLAGS_NONE, NULL, 
            "com.deepin.SessionManager", "/com/deepin/SessionManager",
            "com.deepin.SessionManager", NULL, &error);

    if (error) {
        meta_warning ("%s: %s\n", __func__, error->message);
        g_error_free(error);
        return;
    }

    
    const gchar* cookie = g_getenv("DDE_SESSION_PROCESS_COOKIE_ID");
    if (cookie) {
        g_dbus_proxy_call(sm, "Register", g_variant_new("(s)", cookie), 
                G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
        g_unsetenv("DDE_SESSION_PROCESS_COOKIE_ID");
    }

    g_object_unref(sm);

}

static void on_monitors_changed(GdkScreen *gdkscreen, gpointer user_data)
{
    meta_verbose("%s\n", __func__);
    MetaScreen* screen = meta_screen_for_x_screen(gdk_x11_screen_get_xscreen(gdkscreen));
    g_assert(screen != NULL);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_SCREEN_CHANGED], 0, screen);
}

static void on_screen_resized(GdkScreen *screen, gpointer user_data)
{
    meta_verbose("%s\n", __func__);
}

void deepin_message_hub_workspace_added(int index)
{
    meta_verbose("%s: %d\n", __func__, index);
    if (!meta_get_display ()->display_opening) {
        g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_WORKSPACE_ADDED], 0, index);
    }
}

void deepin_message_hub_workspace_removed(int index)
{
    meta_verbose("%s: %d\n", __func__, index);
    if (!meta_get_display ()->display_opening) {
        g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_WORKSPACE_REMOVED], 0, index);
    }
}

void deepin_message_hub_workspace_switched(int from, int to)
{
    meta_verbose("%s: %d -> %d\n", __func__, from, to);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_WORKSPACE_SWITCHED], 0, from, to);
}

void deepin_message_hub_workspace_reordered(int index, int new_index)
{
    meta_verbose("%s: %d -> %d\n", __func__, index, new_index);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_WORKSPACE_REORDERED], 0, index, new_index);
}

void deepin_message_hub_screen_corner_entered(MetaScreen *screen, MetaScreenCorner corner)
{
    meta_verbose("%s: %d\n", __func__, corner);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_SCREEN_CORNER_ENTERED], 0, corner);
}

void deepin_message_hub_screen_corner_leaved(MetaScreen *screen, MetaScreenCorner corner)
{
    meta_verbose("%s: %d\n", __func__, corner);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_SCREEN_CORNER_LEAVED], 0, corner);
}

void deepin_message_hub_compositing_changed(gboolean enabled)
{
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_COMPOSITING_CHANGED], 0, enabled);
}

gdouble deepin_message_hub_get_screen_scale()
{
    DeepinMessageHub* self = deepin_message_hub_get();
    return self->priv->cached_scale;
}

DeepinMessageHub* deepin_message_hub_get()
{
    if (!_the_hub) {
        _the_hub = (DeepinMessageHub*)g_object_new(DEEPIN_TYPE_MESSAGE_HUB, NULL);
        g_signal_connect(_the_hub, "unable-to-operate", G_CALLBACK(on_message_unable_to_operate), NULL);
        GdkScreen* screen = gdk_screen_get_default();
        g_object_connect(screen,
                "signal::monitors-changed", on_monitors_changed, NULL, 
                "signal::size-changed", on_screen_resized, NULL, NULL);
    }
    return _the_hub;
}

