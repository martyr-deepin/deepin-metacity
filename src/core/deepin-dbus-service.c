/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-dbus-service.c
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * gtk-foobar is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * deepin-metacity is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <gio/gio.h>
#include <util.h>
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
}

static gboolean deepin_dbus_service_handle_perform_action(DeepinDBusWm *object,
    GDBusMethodInvocation *invocation, gint type)
{
    DeepinDBusService* self = DEEPIN_DBUS_SERVICE(object);
    meta_verbose("%s\n", __func__);

    switch((enum ActionType)type) {
        case SHOW_WORKSPACE_VIEW: 
        {
            MetaDisplay* display = meta_get_display();
            guint32 timestamp = meta_display_get_current_time_roundtrip(display);
            do_preview_workspace(display, display->active_screen, 
                    NULL, timestamp, NULL, NULL, FALSE);
            
            break;
        }

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
    deepin_dbus_wm_complete_toggle_debug(object, invocation);
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

