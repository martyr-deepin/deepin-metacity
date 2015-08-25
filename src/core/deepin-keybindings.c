/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* deepin custom keybindings */

/*
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <config.h>
#include <util.h>
#include <tabpopup.h>
#include <ui.h>
#include <gdk/gdkdevice.h>
#include <gdk/gdkx.h>
#include <screen.h>
#include "workspace.h"
#include "keybindings.h"
#include "window-private.h"
#include "deepin-shadow-workspace.h"
#include "deepin-window-surface-manager.h"
#include "deepin-wm-background.h"

static unsigned int get_primary_modifier (MetaDisplay *display,
        unsigned int entire_binding_mask)
{
    /* The idea here is to see if the "main" modifier
     * for Alt+Tab has been pressed/released. So if the binding
     * is Alt+Shift+Tab then releasing Alt is the thing that
     * ends the operation. It's pretty random how we order
     * these.
     */
    unsigned int masks[] = { Mod5Mask, Mod4Mask, Mod3Mask,
        Mod2Mask, Mod1Mask, ControlMask, ShiftMask, LockMask };

    int i;

    i = 0;
    while (i < (int) G_N_ELEMENTS (masks)) {
        if (entire_binding_mask & masks[i])
            return masks[i];
        ++i;
    }

    return 0;
}

static gboolean primary_modifier_still_pressed (MetaDisplay *display,
        unsigned int entire_binding_mask)
{
    unsigned int primary_modifier;
    int x, y, root_x, root_y;
    Window root, child;
    guint mask;
    MetaScreen *random_screen;
    Window      random_xwindow;

    primary_modifier = get_primary_modifier (display, entire_binding_mask);

    random_screen = display->screens->data;
    random_xwindow = random_screen->no_focus_window;
    XQueryPointer (display->xdisplay,
            random_xwindow, /* some random window */
            &root, &child,
            &root_x, &root_y,
            &x, &y,
            &mask);

    meta_topic (META_DEBUG_KEYBINDINGS,
            "Primary modifier 0x%x full grab mask 0x%x current state 0x%x\n",
            primary_modifier, entire_binding_mask, mask);

    if ((mask & primary_modifier) == 0)
        return FALSE;
    else
        return TRUE;
}

static MetaGrabOp tab_op_from_tab_type (MetaTabList type)
{
    switch (type)
    {
        case META_TAB_LIST_NORMAL:
            return META_GRAB_OP_KEYBOARD_TABBING_NORMAL;
        case META_TAB_LIST_DOCKS:
            return META_GRAB_OP_KEYBOARD_TABBING_DOCK;
        case META_TAB_LIST_GROUP:
            return META_GRAB_OP_KEYBOARD_TABBING_GROUP;
    }

    g_assert_not_reached ();

    return 0;
}

static void do_choose_window (MetaDisplay    *display,
        MetaScreen     *screen,
        MetaWindow     *event_window,
        XEvent         *event,
        MetaKeyBinding *binding,
        gboolean        backward)
{
    MetaTabList type = binding->handler->data;
    MetaWindow *initial_selection;

    /* reverse direction if shift is down */
    if (event->xkey.state & ShiftMask)
        backward = !backward;

    initial_selection = meta_display_get_tab_next (display, type,
            screen, screen->active_workspace, NULL, backward);

    /* Note that focus_window may not be in the tab chain, but it's OK */
    if (initial_selection == NULL)
        initial_selection = meta_display_get_tab_current (display,
                type, screen,
                screen->active_workspace);

    meta_topic (META_DEBUG_KEYBINDINGS,
            "Initially selecting window %s\n",
            initial_selection ? initial_selection->desc : "(none)");

    if (initial_selection != NULL) {
        if (binding->mask == 0) {
            /* If no modifiers, we can't do the "hold down modifier to keep
             * moving" thing, so we just instaswitch by one window.
             */
            meta_topic (META_DEBUG_FOCUS,
                    "Activating %s and turning off mouse_mode due to "
                    "switch/cycle windows with no modifiers\n",
                    initial_selection->desc);
            display->mouse_mode = FALSE;
            meta_window_activate (initial_selection, event->xkey.time);
        } else if (meta_display_begin_grab_op (display,
                    screen,
                    NULL,
                    tab_op_from_tab_type (type),
                    FALSE,
                    FALSE,
                    0,
                    binding->mask,
                    event->xkey.time,
                    0, 0)) {
            if (!primary_modifier_still_pressed (display,
                        binding->mask)) {
                /* This handles a race where modifier might be released
                 * before we establish the grab. must end grab
                 * prior to trying to focus a window.
                 */
                meta_topic (META_DEBUG_FOCUS,
                        "Ending grab, activating %s, and turning off "
                        "mouse_mode due to switch/cycle windows where "
                        "modifier was released prior to grab\n",
                        initial_selection->desc);
                meta_display_end_grab_op (display, event->xkey.time);
                display->mouse_mode = FALSE;
                meta_window_activate (initial_selection, event->xkey.time);
            } else {
                deepin_tab_popup_select (screen->tab_popup,
                        (MetaTabEntryKey) initial_selection->xwindow);
                deepin_tab_popup_set_showing (screen->tab_popup, TRUE);
            }
        }
    }
}

static void handle_switch(MetaDisplay *display, MetaScreen *screen,
        MetaWindow *window, XEvent *event,
        MetaKeyBinding *binding, gpointer user_data)
{
    gint backwards = (binding->handler->flags & META_KEY_BINDING_IS_REVERSED) != 0;
    do_choose_window (display, screen, window, event, binding, backwards);
}

static void _do_ungrab(MetaScreen* screen, GtkWidget* w, gboolean release_keyboard)
{
    GdkDisplay* gdisplay = gdk_x11_lookup_xdisplay(screen->display->xdisplay);
    GdkDeviceManager* dev_man = gdk_display_get_device_manager(gdisplay);
    GdkDevice* pointer = gdk_device_manager_get_client_pointer(dev_man);
    GdkDevice* kb = gdk_device_get_associated_device(pointer);

    if (release_keyboard) 
        gdk_device_ungrab(kb, GDK_CURRENT_TIME);
    gdk_device_ungrab(pointer, GDK_CURRENT_TIME);
}

static void _do_grab(MetaScreen* screen, GtkWidget* w, gboolean grab_keyboard)
{
    GdkDisplay* gdisplay = gdk_x11_lookup_xdisplay(screen->display->xdisplay);
    GdkDeviceManager* dev_man = gdk_display_get_device_manager(gdisplay);
    GdkDevice* pointer = gdk_device_manager_get_client_pointer(dev_man);
    GdkDevice* kb = gdk_device_get_associated_device(pointer);

    g_assert(gdk_device_get_source(pointer) == GDK_SOURCE_MOUSE);
    g_assert(gdk_device_get_source(kb) == GDK_SOURCE_KEYBOARD);

    GdkGrabStatus ret = GDK_GRAB_SUCCESS;
    if (grab_keyboard) {
        ret = gdk_device_grab(kb, gtk_widget_get_window(w), 
                GDK_OWNERSHIP_NONE, TRUE,
                GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
                NULL, GDK_CURRENT_TIME);
        if (ret != GDK_GRAB_SUCCESS) {
            g_message("%s: grab keyboard failed", __func__);
        }
    }

    ret = gdk_device_grab(pointer, gtk_widget_get_window(w), 
            GDK_OWNERSHIP_NONE, TRUE,
            GDK_BUTTON_PRESS_MASK| GDK_BUTTON_RELEASE_MASK| 
            GDK_ENTER_NOTIFY_MASK| GDK_FOCUS_CHANGE_MASK,
            NULL, GDK_CURRENT_TIME);
    if (ret != GDK_GRAB_SUCCESS) {
        g_message("%s: grab failed", __func__);
    }
}

static void handle_preview_workspace(MetaDisplay *display, MetaScreen *screen,
        MetaWindow *window, XEvent *event,
        MetaKeyBinding *binding, gpointer user_data)
{
    g_message("%s", __func__);
    unsigned int grab_mask = binding->mask;
    if (meta_display_begin_grab_op (display,
                screen,
                NULL,
                META_GRAB_OP_KEYBOARD_PREVIEWING_WORKSPACE,
                FALSE,
                FALSE,
                0,
                grab_mask,
                event->xkey.time,
                0, 0))
    {
        gboolean grabbed_before_release = 
            primary_modifier_still_pressed (display, grab_mask);

        meta_topic (META_DEBUG_KEYBINDINGS, "Activating workspace preview\n");

        if (!grabbed_before_release) {
            /* end the grab right away, modifier possibly released
             * before we could establish the grab and receive the
             * release event. Must end grab before we can switch
             * spaces.
             */
            g_message("not grabbed_before_release");
            meta_display_end_grab_op (display, event->xkey.time);
            return;
        }

        deepin_wm_background_setup(screen->ws_previewer);
        gtk_widget_show_all(GTK_WIDGET(screen->ws_previewer));
        gtk_window_move(GTK_WINDOW(screen->ws_previewer), 0, 0);

        /* rely on auto ungrab when destroyed */
        _do_grab(screen, screen->ws_previewer, TRUE);
    }
}

static void handle_expose_windows(MetaDisplay *display, MetaScreen *screen,
        MetaWindow *window, XEvent *event,
        MetaKeyBinding *binding, gpointer user_data)
{
    g_message("%s", __func__);
    unsigned int grab_mask = binding->mask;
    if (meta_display_begin_grab_op (display,
                screen,
                NULL,
                META_GRAB_OP_KEYBOARD_EXPOSING_WINDOWS,
                FALSE,
                FALSE,
                0,
                grab_mask,
                event->xkey.time,
                0, 0))
    {
        gboolean grabbed_before_release = 
            primary_modifier_still_pressed (display, grab_mask);

        meta_topic (META_DEBUG_KEYBINDINGS, "Activating workspace preview\n");

        if (!grabbed_before_release) {
            /* end the grab right away, modifier possibly released
             * before we could establish the grab and receive the
             * release event. Must end grab before we can switch
             * spaces.
             */
            g_message("not grabbed_before_release");
            meta_display_end_grab_op (display, event->xkey.time);
            return;
        }

        GtkWidget* top = screen->exposing_windows_popup;
        deepin_window_surface_manager_flush();

        DeepinShadowWorkspace* active_workspace = 
            (DeepinShadowWorkspace*)deepin_shadow_workspace_new();
        deepin_shadow_workspace_populate(active_workspace, screen->active_workspace);
        deepin_shadow_workspace_set_presentation(active_workspace, TRUE);
        deepin_shadow_workspace_set_current(active_workspace, TRUE);

        gtk_container_add(GTK_CONTAINER(top), (GtkWidget*)active_workspace);
        gtk_widget_show_all(top);

        _do_grab(screen, top, FALSE);
    }
}

static void handle_workspace_switch(MetaDisplay *display, MetaScreen *screen,
        MetaWindow *window, XEvent *event,
        MetaKeyBinding *binding, gpointer user_data)
{
    MetaWorkspace* workspace = NULL;

    MetaKeyBindingAction action = meta_prefs_get_keybinding_action(binding->name);
    if (action == META_KEYBINDING_ACTION_WORKSPACE_RIGHT) {
        g_message("%s: to right", __func__);
        workspace = meta_workspace_get_neighbor (screen->active_workspace, 
                META_MOTION_RIGHT);
    } else if (action == META_KEYBINDING_ACTION_WORKSPACE_LEFT) {
        workspace = meta_workspace_get_neighbor (screen->active_workspace, 
                META_MOTION_LEFT);
        g_message("%s: to left", __func__);
    }

    if (workspace) {
        meta_workspace_activate(workspace, event->xkey.time);
    }
}

void deepin_init_custom_handlers(MetaDisplay* display)
{
    deepin_meta_override_keybinding_handler("switch-applications",
            handle_switch, NULL, NULL);
    deepin_meta_override_keybinding_handler("switch-to-workspace-right",
            handle_workspace_switch, NULL, NULL);
    deepin_meta_override_keybinding_handler("switch-to-workspace-left",
            handle_workspace_switch, NULL, NULL);

    deepin_meta_display_add_keybinding(display, "expose-all-windows",
            META_KEY_BINDING_NONE, handle_expose_windows, 1);
    deepin_meta_display_add_keybinding(display, "expose-windows",
            META_KEY_BINDING_NONE, handle_expose_windows, 2);
    deepin_meta_display_add_keybinding(display, "preview-workspace",
            META_KEY_BINDING_NONE, handle_preview_workspace, 2);
}

