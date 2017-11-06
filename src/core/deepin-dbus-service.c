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
#include "deepin-background-cache.h"
#include "deepin-message-hub.h"
#include "deepin-dbus-wm.h"
#include "deepin-keybindings.h"

static DeepinDBusWm* _the_service = NULL;

enum ActionType
{
    NONE = 0,
    SHOW_WORKSPACE_VIEW, /* disabled forever ever */
    MAXIMIZE_CURRENT,
    MINIMIZE_CURRENT,
    OPEN_LAUNCHER,
    CUSTOM_COMMAND,
    WINDOW_OVERVIEW,
    WINDOW_OVERVIEW_ALL
};

static gboolean deepin_dbus_service_handle_perform_action(DeepinDBusWm *object,
    GDBusMethodInvocation *invocation, gint type, gpointer data)
{
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    MetaScreen* screen = display->active_screen;
    guint32 timestamp = 0;
    MetaWindow* current = NULL;

    if (!display->compositor) {
        switch((enum ActionType)type) {
            case SHOW_WORKSPACE_VIEW: 
            case WINDOW_OVERVIEW:
            case WINDOW_OVERVIEW_ALL:
            {
                GError *error = NULL;
                if (!g_spawn_command_line_async("/usr/lib/deepin-daemon/dde-warning-dialog", &error)) {
                    g_warning("%s", error->message);
                    g_error_free(error);
                }
                break;
            }
/*
				case ActionType.MAXIMIZE_CURRENT:
					if (current == null || current.window_type != WindowType.NORMAL)
						break;

					if (current.get_maximized () == (MaximizeFlags.HORIZONTAL | MaximizeFlags.VERTICAL))
						current.unmaximize (MaximizeFlags.HORIZONTAL | MaximizeFlags.VERTICAL);
					else
						current.maximize (MaximizeFlags.HORIZONTAL | MaximizeFlags.VERTICAL);
					break;
				case ActionType.MINIMIZE_CURRENT:
					if (current != null && current.window_type == WindowType.NORMAL)
						current.minimize ();
					break;
                    */

            default: break;
        }
    } else {
        switch((enum ActionType)type) {
            case SHOW_WORKSPACE_VIEW: 
                break;

            case WINDOW_OVERVIEW: {
                timestamp = meta_display_get_current_time_roundtrip(display);
                do_expose_windows(display, display->active_screen, NULL, 
                        timestamp, NULL, 1, NULL);
                break;
            }

            case WINDOW_OVERVIEW_ALL: {
                timestamp = meta_display_get_current_time_roundtrip(display);
                do_expose_windows(display, display->active_screen, NULL, 
                        timestamp, NULL, 2, NULL);
                break;
            }

            default: break;
        }
    }

    if (type == MAXIMIZE_CURRENT) {
        current = meta_display_get_focus_window (display);
        if (current == NULL || current->type != META_WINDOW_NORMAL) {
            goto done;
        }

        if (META_WINDOW_MAXIMIZED(current)) {
            meta_window_unmaximize(current,
                    META_MAXIMIZE_VERTICAL | META_MAXIMIZE_HORIZONTAL);
        } else if (current->has_maximize_func) {
            meta_window_maximize(current,
                    META_MAXIMIZE_VERTICAL | META_MAXIMIZE_HORIZONTAL);
        }
    } else if (type == MINIMIZE_CURRENT) {
        current = meta_display_get_focus_window (display);
        if (current == NULL || current->type != META_WINDOW_NORMAL || current->has_minimize_func) {
            goto done;
        }

        meta_window_minimize(current);
    }

done:
    deepin_dbus_wm_complete_perform_action(object, invocation);
    return TRUE;
}

static gboolean deepin_dbus_service_handle_toggle_debug( DeepinDBusWm *object,
        GDBusMethodInvocation *invocation, gpointer data)
{
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

static gboolean deepin_dbus_service_handle_enable_zone_detected (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation,
        gboolean val, gpointer data)
{
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    meta_screen_enable_corner_actions (display->active_screen, val);
    deepin_dbus_wm_complete_enable_zone_detected(object, invocation);

    return TRUE;
}

static gboolean deepin_dbus_service_handle_present_windows (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation,
        GVariant *xids, gpointer data)
{
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    if (display->compositor) {
        guint32 timestamp = meta_display_get_current_time_roundtrip(display);
        do_expose_windows(display, display->active_screen, NULL, timestamp, NULL, 3, xids);
    }
    deepin_dbus_wm_complete_present_windows(object, invocation);

    return TRUE;
}

static gboolean deepin_dbus_service_handle_request_hide_windows( DeepinDBusWm *object,
        GDBusMethodInvocation *invocation, gpointer data)
{
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    meta_screen_request_hide_windows (display->active_screen);
    deepin_dbus_wm_complete_request_hide_windows(object, invocation);
    return TRUE;
}

static gboolean deepin_dbus_service_handle_cancel_hide_windows( DeepinDBusWm *object,
        GDBusMethodInvocation *invocation, gpointer data)
{
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    meta_screen_cancel_hide_windows (display->active_screen);
    deepin_dbus_wm_complete_cancel_hide_windows(object, invocation);
    return TRUE;
}

static gboolean deepin_dbus_service_handle_change_current_workspace_background (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation,
        char *uri, gpointer data)
{
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    int index = meta_workspace_index(display->active_screen->active_workspace);
    deepin_change_background (index, uri);
    deepin_dbus_wm_complete_change_current_workspace_background (object, invocation);
    return TRUE;
}

static gboolean deepin_dbus_service_handle_set_transient_background (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation,
        char *uri, gpointer data)
{
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    int index = meta_workspace_index(display->active_screen->active_workspace);
    deepin_change_background_transient (index, uri);
    deepin_dbus_wm_complete_set_transient_background (object, invocation);
    return TRUE;
}

static gboolean deepin_dbus_service_handle_get_current_workspace_background (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation,
        gpointer data)
{
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    int index = meta_workspace_index(display->active_screen->active_workspace);
    char *uri = deepin_get_background_uri (index);
    deepin_dbus_wm_complete_get_current_workspace_background (object, invocation, uri);
    free(uri);
    return TRUE;
}

static gboolean deepin_dbus_service_handle_switch_application (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation,
        gboolean backward, gpointer data)
{
    meta_verbose("%s\n", __func__);

/*
            var current = display.get_tab_current (Meta.TabList.NORMAL, workspace);
            var window = display.get_tab_next (Meta.TabList.NORMAL, workspace,
                    current, backward);
            if (window == null) {
                window = current;
            }

            window.activate (display.get_current_time_roundtrip ());
            */

    MetaDisplay* display = meta_get_display();
    MetaWorkspace* workspace = display->active_screen->active_workspace;
    MetaWindow* current = meta_display_get_tab_current(display, META_TAB_LIST_NORMAL,
            display->active_screen, workspace);
    MetaWindow* next = meta_display_get_tab_next(display, META_TAB_LIST_NORMAL,
            display->active_screen, workspace, current, backward);
    if (!next) {
        next = current;
    }

    if (next) {
        guint32 timestamp = meta_display_get_current_time_roundtrip(display);
        meta_window_activate(next, timestamp);
    }
    deepin_dbus_wm_complete_switch_application (object, invocation);
    return TRUE;
}

static gboolean deepin_dbus_service_handle_switch_to_workspace (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation,
        gboolean backward, gpointer data)
{
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    MetaScreen* screen = display->active_screen;
    {
        gint motion = backward ? META_MOTION_LEFT: META_MOTION_RIGHT;
        MetaWorkspace *next = meta_workspace_get_neighbor(screen->active_workspace, motion);

        if (next && next != screen->active_workspace) {
            guint32 timestamp = meta_display_get_current_time_roundtrip(display);
            meta_workspace_activate(next, timestamp);
        }
    }
    deepin_dbus_wm_complete_switch_to_workspace (object, invocation);
    return TRUE;
}

static gboolean deepin_dbus_service_handle_tile_active_window (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation,
        guint side, gpointer data)
{
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    MetaScreen* screen = display->active_screen;
    MetaWindow* current = meta_display_get_focus_window (display);
    if (current == NULL || current->type != META_WINDOW_NORMAL || 
            meta_window_is_maximized(current) ||
            !meta_window_can_tile_side_by_side(current)) {
        goto done;
    }

    meta_window_tile_by_side(current, (MetaTileSide)side);

done:
    deepin_dbus_wm_complete_tile_active_window (object, invocation);
    return TRUE;
}

static gboolean deepin_dbus_service_handle_begin_to_move_active_window (
        DeepinDBusWm *object,
        GDBusMethodInvocation *invocation,
        gpointer data)
{
    meta_verbose("%s\n", __func__);

    MetaDisplay* display = meta_get_display();
    MetaScreen* screen = display->active_screen;
    MetaWindow* current = meta_display_get_focus_window (display);
    if (current == NULL || current->type != META_WINDOW_NORMAL) {
        goto done;
    }

    meta_window_begin_to_move(current);

done:
    deepin_dbus_wm_complete_begin_to_move_active_window (object, invocation);
    return TRUE;
}

static void on_bus_acquired(GDBusConnection *connection,
        const gchar *name, gpointer user_data)
{
    gboolean ret = g_dbus_interface_skeleton_export(
            G_DBUS_INTERFACE_SKELETON(user_data), 
            connection, "/com/deepin/wm", NULL);
    meta_verbose("%s result %s\n", __func__, ret ? "success":"failure");
}

static void on_workspace_added (DeepinMessageHub *hub, int index, DeepinDBusWm *object)
{
    deepin_dbus_wm_emit_workspace_added (object, index);
}

static void on_workspace_removed (DeepinMessageHub *hub, int index, DeepinDBusWm *object)
{
    deepin_dbus_wm_emit_workspace_removed (object, index);
}

static void on_workspace_switched (DeepinMessageHub *hub, int from, int to, DeepinDBusWm *object)
{
    deepin_dbus_wm_emit_workspace_switched (object, from ,to);
}

DeepinDBusWm* deepin_dbus_service_get()
{
    if (!_the_service) {
        _the_service = deepin_dbus_wm_skeleton_new (); 

        g_object_connect (G_OBJECT(_the_service),
                "signal::handle_perform_action", deepin_dbus_service_handle_perform_action, NULL,
                "signal::handle_present_windows", deepin_dbus_service_handle_present_windows, NULL,
                "signal::handle_request_hide_windows", deepin_dbus_service_handle_request_hide_windows,  NULL,
                "signal::handle_cancel_hide_windows", deepin_dbus_service_handle_cancel_hide_windows, NULL,
                "signal::handle_toggle_debug", deepin_dbus_service_handle_toggle_debug, NULL,
                "signal::handle_enable_zone_detected", deepin_dbus_service_handle_enable_zone_detected, NULL,
                "signal::handle_change_current_workspace_background",
                deepin_dbus_service_handle_change_current_workspace_background, NULL,
                "signal::handle_set_transient_background",
                deepin_dbus_service_handle_set_transient_background, NULL,
                "signal::handle_get_current_workspace_background",
                deepin_dbus_service_handle_get_current_workspace_background, NULL,
                "signal::handle_switch_application",
                deepin_dbus_service_handle_switch_application, NULL,
                "signal::handle_switch_to_workspace",
                deepin_dbus_service_handle_switch_to_workspace, NULL,
                "signal::handle_tile_active_window",
                deepin_dbus_service_handle_tile_active_window, NULL,
                "signal::handle_begin_to_move_active_window",
                deepin_dbus_service_handle_begin_to_move_active_window, NULL,
                NULL);

        g_object_connect (G_OBJECT(deepin_message_hub_get ()),
                "signal::workspace-added", on_workspace_added, _the_service,
                "signal::workspace-removed", on_workspace_removed, _the_service,
                "signal::workspace-switched", on_workspace_switched, _the_service,
                NULL);

        g_bus_own_name(G_BUS_TYPE_SESSION, 
                "com.deepin.wm",
                G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT|G_BUS_NAME_OWNER_FLAGS_REPLACE,
                on_bus_acquired, NULL, NULL, g_object_ref(_the_service), g_object_unref);
    }
    return _the_service;
}

