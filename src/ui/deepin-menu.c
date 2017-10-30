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
#include <stdio.h>
#include <string.h>
#include <json-glib/json-glib.h>
#include "deepin-menu.h"
#include "main.h"
#include "util.h"
#include "core.h"
#include "metaaccellabel.h"
#include "ui.h"
#include "core/screen-private.h"
#include "deepin-message-hub.h"

typedef struct _MenuItem MenuItem;

typedef enum
{
    MENU_ITEM_SEPARATOR = 0,
    MENU_ITEM_NORMAL,
    MENU_ITEM_CHECKBOX,
} MetaMenuItemType;

struct _MenuItem
{
    const char* id;
    MetaMenuOp op;
    MetaMenuItemType type;
    const gboolean checked;
    const char *label;
};

static MenuItem menuitems[] = {
    { "minimize", META_MENU_OP_MINIMIZE, MENU_ITEM_NORMAL, FALSE, N_("Mi_nimize") },
    { "maximize", META_MENU_OP_MAXIMIZE, MENU_ITEM_NORMAL, FALSE, N_("Ma_ximize") },
    { "unmaximize", META_MENU_OP_UNMAXIMIZE, MENU_ITEM_NORMAL, FALSE, N_("Unma_ximize") },
    { "move", META_MENU_OP_MOVE, MENU_ITEM_NORMAL, FALSE, N_("_Move") },
    { "resize", META_MENU_OP_RESIZE, MENU_ITEM_NORMAL, FALSE, N_("_Resize") },
    { "on_top", META_MENU_OP_ABOVE, MENU_ITEM_CHECKBOX, FALSE, N_("Always on _Top") },
    { "not_on_top", META_MENU_OP_UNABOVE, MENU_ITEM_CHECKBOX, TRUE, N_("Always on _Top") },
    { "on_visible_workspace", META_MENU_OP_STICK, MENU_ITEM_CHECKBOX, FALSE, N_("_Always on Visible Workspace") },
    { "on_this_workspace", META_MENU_OP_UNSTICK, MENU_ITEM_CHECKBOX, TRUE,  N_("_Always on Visible Workspace") },
    { "move_left", META_MENU_OP_MOVE_LEFT, MENU_ITEM_NORMAL, FALSE, N_("Move to Workspace _Left") },
    { "move_right", META_MENU_OP_MOVE_RIGHT, MENU_ITEM_NORMAL, FALSE, N_("Move to Workspace R_ight") },
    { "Close", META_MENU_OP_DELETE, MENU_ITEM_NORMAL, FALSE, N_("_Close") }
};


static void deepin_window_menu_item_handler (GDBusProxy *proxy,
               gchar      *sender_name,
               gchar      *signal_name,
               GVariant   *parameters,
               gpointer    user_data)
{
    DeepinWindowMenu* menu = (DeepinWindowMenu*)user_data;

    MetaDisplay* display = meta_get_display();

    meta_verbose("%s: %s sig: %s, params: %s\n", __func__, sender_name, signal_name, 
            g_variant_print(parameters, TRUE));
    if (g_str_equal(signal_name, "ItemInvoked")) {
        meta_frames_notify_menu_hide (menu->frames);

        char* item_id;
        gboolean item_checked;
        g_variant_get(parameters, "(sb)", &item_id, &item_checked);

        int i;
        for (i = 0; i < (int) G_N_ELEMENTS (menuitems); i++) {
            MenuItem item = menuitems[i];
            if (g_str_equal(item.id, item_id)) {
                printf("found item %s\n", item.id);
                break;
            }
        }

        g_free(item_id);
        if (i == G_N_ELEMENTS(menuitems)) return;

        menu->func(menu,
                display->xdisplay,
                menu->client_xwindow,
                gtk_get_current_event_time (),
                menuitems[i].op,
                meta_workspace_index(display->active_screen->active_workspace),
                menu->data);
    } else if (g_str_equal(signal_name, "MenuUnregistered")) {
    }
}

DeepinWindowMenu* deepin_window_menu_new   (MetaFrames         *frames,
        MetaMenuOp          ops,
        MetaMenuOp          insensitive,
        Window              client_xwindow,
        unsigned long       active_workspace,
        int                 n_workspaces,
        MetaWindowMenuFunc  func,
        gpointer            data)
{
    int i;
    DeepinWindowMenu *menu;

    if (n_workspaces < 2) {
        ops &= ~(META_MENU_OP_STICK | META_MENU_OP_UNSTICK);
    }

    menu = g_new (DeepinWindowMenu, 1);
    menu->frames = frames;
    menu->client_xwindow = client_xwindow;
    menu->func = func;
    menu->data = data;
    menu->ops = ops;
    menu->insensitive = insensitive;

    GError* error = NULL;

    menu->manager = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, 
            G_DBUS_PROXY_FLAGS_NONE, NULL, 
            "com.deepin.menu", "/com/deepin/menu",
            "com.deepin.menu.Manager", 
            NULL, &error);

    if (error) {
        meta_warning ("%s: %s\n", __func__, error->message);
        g_error_free(error);
        deepin_window_menu_free(menu);
        return NULL;
    }

    GVariant* reply = g_dbus_proxy_call_sync(menu->manager, "RegisterMenu", NULL,
            G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

    if (error) {
        meta_warning ("%s: %s\n", __func__, error->message);
        g_error_free(error);
        deepin_window_menu_free(menu);
        return NULL;
    }

    GVariantIter iter;
	g_variant_iter_init (&iter, reply);
	GVariant* menu_object_path = g_variant_iter_next_value(&iter);

    menu->menu_interface = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
            G_DBUS_PROXY_FLAGS_NONE, NULL, 
            "com.deepin.menu", g_variant_get_string(menu_object_path, NULL),
            "com.deepin.menu.Menu", NULL, &error);
    meta_verbose ("%s: register interface %s\n", __func__, g_variant_get_string(menu_object_path, NULL));

	g_variant_unref(menu_object_path);
    g_variant_unref(reply);

    if (error) {
        meta_warning ("%s: %s\n", __func__, error->message);
        g_error_free(error);
        deepin_window_menu_free(menu);
        return NULL;
    }

    g_signal_connect(G_OBJECT(menu->menu_interface), "g-signal", 
            G_CALLBACK(deepin_window_menu_item_handler), menu);

    return menu;
}

static JsonNode* get_item_node(MenuItem item) 
{
    JsonBuilder* builder = json_builder_new();

    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "itemId");
    json_builder_add_string_value(builder, item.id);

    json_builder_set_member_name(builder, "itemText");
    json_builder_add_string_value(builder, gettext(item.label));

    json_builder_set_member_name(builder, "itemIcon");
    json_builder_add_string_value(builder, "");

    json_builder_set_member_name(builder, "itemIconHover");
    json_builder_add_string_value(builder, "");

    json_builder_set_member_name(builder, "itemIconInactive");
    json_builder_add_string_value(builder, "");

    json_builder_set_member_name(builder, "itemExtra");
    json_builder_add_string_value(builder, "");

    json_builder_set_member_name(builder, "isActive");
    json_builder_add_boolean_value(builder, TRUE);

    switch (item.op) {
        case META_MENU_OP_ABOVE:
        case META_MENU_OP_UNABOVE:
        case META_MENU_OP_STICK:
        case META_MENU_OP_UNSTICK:
            json_builder_set_member_name(builder, "isCheckable");
            json_builder_add_boolean_value(builder, TRUE);

            json_builder_set_member_name(builder, "checked");
            json_builder_add_boolean_value(builder, item.checked);
        default: break;
    }

    json_builder_end_object (builder);

    JsonNode* root = json_builder_get_root(builder);
    g_object_unref(builder);

    return root;
}

static char* get_items_node(DeepinWindowMenu* menu)
{
    JsonBuilder* builder = json_builder_new();

    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "items");
    json_builder_begin_array (builder);
    for (int i = 0; i < (int) G_N_ELEMENTS (menuitems); i++) {
        MenuItem item = menuitems[i];
        if ((menu->ops & item.op) && !(menu->insensitive & item.op)) {
            json_builder_add_value(builder, get_item_node(item));
        }
    }
    json_builder_end_array (builder);

    json_builder_end_object (builder);

    JsonGenerator* generator = json_generator_new();
    JsonNode* root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    char* data = json_generator_to_data(generator, NULL);

    json_node_unref(root);
    g_object_unref(generator);
    g_object_unref(builder);

    return data;
}

static void deepin_window_menu_show (DeepinWindowMenu     *menu,
        int                 root_x,
        int                 root_y, 
        const gchar*        content)
{
    GError* error = NULL;

    g_dbus_proxy_call_sync(menu->menu_interface, "ShowMenu", g_variant_new("(s)", content),
            G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

    if (error) {
        meta_warning ("%s: %s\n", __func__, error->message);
        g_error_free(error);
    }
}

void deepin_window_menu_popup (DeepinWindowMenu     *menu,
        int                 root_x,
        int                 root_y,
        int                 button,
        guint32             timestamp)
{
    double scale = deepin_message_hub_get_screen_scale ();
    root_x /= scale;
    root_y /= scale;

    JsonBuilder* builder = json_builder_new();

    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "x");
    json_builder_add_int_value(builder, root_x);
    json_builder_set_member_name(builder, "y");
    json_builder_add_int_value(builder, root_y);
    json_builder_set_member_name(builder, "isDockMenu");
    json_builder_add_boolean_value(builder, FALSE);
    json_builder_set_member_name(builder, "menuJsonContent");
    char* content = get_items_node(menu);
    json_builder_add_string_value(builder, content);
    g_free(content);

    json_builder_end_object (builder);

    JsonGenerator* generator = json_generator_new();
    JsonNode* root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);

    gchar* menu_json_content = json_generator_to_data(generator, NULL);
    json_node_unref(root);
    g_object_unref(generator);
    g_object_unref(builder);

    deepin_window_menu_show(menu, root_x, root_y, menu_json_content);

    g_free(menu_json_content);
}

void deepin_window_menu_free (DeepinWindowMenu *menu)
{
    if (menu->menu_interface) g_object_unref(menu->menu_interface);
    if (menu->manager) g_object_unref(menu->manager);
    g_free (menu);
}

