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
#include <gio/gio.h>
#include <util.h>
#include "screen-private.h"
#include "deepin-dbus-service.h"
#include "deepin-dbus-wm.h"
#include "deepin-keybindings.h"

struct _DeepinDBusServicePrivate
{
    gint disposed: 1;
    guint dbus_id;
};

static DeepinDBusService* _the_service = NULL;

static void deepin_dbus_wm_interface_init (DeepinDBusWmIface *iface);
static gboolean deepin_dbus_service_handle_perform_action (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation,
        gint type);

static gboolean deepin_dbus_service_handle_toggle_debug (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation);

static gboolean deepin_dbus_service_handle_request_hide_windows (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation);

static gboolean deepin_dbus_service_handle_cancel_hide_windows (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation);

static gboolean deepin_dbus_service_handle_present_windows (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation,
        GVariant *xids);

G_DEFINE_TYPE_WITH_CODE (DeepinDBusService, deepin_dbus_service,
        DEEPIN_DBUS_TYPE_WM_SKELETON,
        G_IMPLEMENT_INTERFACE(DEEPIN_DBUS_TYPE_WM, deepin_dbus_wm_interface_init));

enum ActionType
{
    NONE = 0,
    SHOW_WORKSPACE_VIEW,
    MAXIMIZE_CURRENT,
    MINIMIZE_CURRENT,
    OPEN_LAUNCHER,
    CUSTOM_COMMAND,
    WINDOW_OVERVIEW,
    WINDOW_OVERVIEW_ALL
};


static void deepin_dbus_wm_interface_init (DeepinDBusWmIface *iface)
{
    iface->handle_perform_action = deepin_dbus_service_handle_perform_action;
    iface->handle_toggle_debug = deepin_dbus_service_handle_toggle_debug;
    iface->handle_request_hide_windows = deepin_dbus_service_handle_request_hide_windows;
    iface->handle_cancel_hide_windows = deepin_dbus_service_handle_cancel_hide_windows;
    iface->handle_present_windows = deepin_dbus_service_handle_present_windows;
}

static gboolean deepin_dbus_service_handle_perform_action(DeepinDBusWm *object,
    GDBusMethodInvocation *invocation, gint type)
{
    DeepinDBusService* self = DEEPIN_DBUS_SERVICE(object);
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    guint32 timestamp = meta_display_get_current_time_roundtrip(display);
    switch((enum ActionType)type) {
        case SHOW_WORKSPACE_VIEW: 
            do_preview_workspace(display, display->active_screen, 
                    NULL, timestamp, NULL, NULL, FALSE);
            
            break;

        case WINDOW_OVERVIEW:
            do_expose_windows(display, display->active_screen, NULL, 
                    timestamp, NULL, 1, NULL);

        case WINDOW_OVERVIEW_ALL:
            do_expose_windows(display, display->active_screen, NULL, 
                    timestamp, NULL, 2, NULL);

        default: break;
    }

    deepin_dbus_wm_complete_perform_action(object, invocation);
    return TRUE;
}

static gboolean deepin_dbus_service_handle_toggle_debug( DeepinDBusWm *object,
        GDBusMethodInvocation *invocation)
{
    DeepinDBusService* self = DEEPIN_DBUS_SERVICE(object);
    meta_verbose("%s\n", __func__);
    gboolean new_val = !meta_is_debugging ();

    if (new_val) {
        g_setenv ("METACITY_DEBUG", "1", TRUE);
        g_setenv ("METACITY_VERBOSE", "1", TRUE);
        g_setenv ("METACITY_USE_LOGFILE", "1", TRUE);
    } else {
        g_unsetenv ("METACITY_DEBUG");
        g_unsetenv ("METACITY_VERBOSE");
        g_unsetenv ("METACITY_USE_LOGFILE");
    }
    meta_set_debugging (new_val);
    meta_set_verbose (new_val);
    deepin_dbus_wm_complete_toggle_debug(object, invocation);
    return TRUE;
}

static gboolean deepin_dbus_service_handle_present_windows (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation,
        GVariant *xids)
{
    DeepinDBusService* self = DEEPIN_DBUS_SERVICE(object);
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    guint32 timestamp = meta_display_get_current_time_roundtrip(display);
    do_expose_windows(display, display->active_screen, NULL, timestamp, NULL, 3, xids);
    deepin_dbus_wm_complete_present_windows(object, invocation);

    return TRUE;
}

static gboolean deepin_dbus_service_handle_request_hide_windows( DeepinDBusWm *object,
        GDBusMethodInvocation *invocation)
{
    DeepinDBusService* self = DEEPIN_DBUS_SERVICE(object);
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    meta_screen_request_hide_windows (display->active_screen);
    deepin_dbus_wm_complete_request_hide_windows(object, invocation);
    return TRUE;
}

static gboolean deepin_dbus_service_handle_cancel_hide_windows( DeepinDBusWm *object,
        GDBusMethodInvocation *invocation)
{
    DeepinDBusService* self = DEEPIN_DBUS_SERVICE(object);
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    meta_screen_cancel_hide_windows (display->active_screen);
    deepin_dbus_wm_complete_cancel_hide_windows(object, invocation);
    return TRUE;
}

static void on_bus_acquired(GDBusConnection *connection,
        const gchar *name, gpointer user_data)
{
    DeepinDBusService* self = DEEPIN_DBUS_SERVICE(user_data);
    gboolean ret = g_dbus_interface_skeleton_export(
            G_DBUS_INTERFACE_SKELETON(self), 
            connection, "/com/deepin/wm", NULL);
    meta_verbose("%s result %s\n", __func__, ret ? "success":"failure");
}

static void deepin_dbus_service_init (DeepinDBusService *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, DEEPIN_TYPE_DBUS_SERVICE, DeepinDBusServicePrivate);

    
    self->priv->dbus_id = g_bus_own_name(G_BUS_TYPE_SESSION, 
            "com.deepin.wm",
            G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT|G_BUS_NAME_OWNER_FLAGS_REPLACE,
            on_bus_acquired, NULL, NULL, g_object_ref(self), g_object_unref);
}

static void deepin_dbus_service_finalize (GObject *object)
{
	/* TODO: Add deinitalization code here */
    DeepinDBusService* self = DEEPIN_DBUS_SERVICE(object);
    DeepinDBusServicePrivate* priv = self->priv;

    if (priv->dbus_id != 0) {
        g_bus_unown_name(priv->dbus_id);
        priv->dbus_id = 0;
    }

	G_OBJECT_CLASS (deepin_dbus_service_parent_class)->finalize (object);
}

static void deepin_dbus_service_class_init (DeepinDBusServiceClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DeepinDBusServicePrivate));

	object_class->finalize = deepin_dbus_service_finalize;
}

DeepinDBusService* deepin_dbus_service_get()
{
    if (!_the_service) {
        _the_service = g_object_new(DEEPIN_TYPE_DBUS_SERVICE, NULL);
    }
    return _the_service;
}

