/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* deepin custom keybindings */


/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#include <glib.h>
#include <config.h>
#include <util.h>
#include <tabpopup.h>
#include <ui.h>
#include <gdk/gdkdevice.h>
#include <gdk/gdkx.h>
#include <screen.h>
#include "errors.h"
#include "workspace.h"
#include "keybindings.h"
#include "window-private.h"
#include "deepin-workspace-overview.h"
#include "deepin-window-surface-manager.h"
#include "deepin-wm-background.h"
#include "deepin-message-hub.h"
#include "deepin-workspace-indicator.h"

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

static void _activate_selection_or_desktop(MetaWindow* target, guint32 timestamp)
{
    if (target->type != META_WINDOW_DESKTOP) {
        meta_window_activate (target, timestamp);

    } else {
        meta_screen_show_desktop(target->screen, timestamp);
    }
}

static gboolean _grab_device (MetaDisplay* display, Window xwindow,
        int device_id, guint32 timestamp)
{
  unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
  XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };
  int ret;

  XISetMask (mask.mask, XI_ButtonPress);
  XISetMask (mask.mask, XI_ButtonRelease);
  XISetMask (mask.mask, XI_Enter);
  XISetMask (mask.mask, XI_Leave);
  XISetMask (mask.mask, XI_Motion);
  XISetMask (mask.mask, XI_KeyPress);
  XISetMask (mask.mask, XI_KeyRelease);
  XISetMask (mask.mask, XI_FocusIn);
  XISetMask (mask.mask, XI_FocusOut);

  ret = XIGrabDevice(display->xdisplay, device_id,
                      xwindow, 
                      timestamp,
                      None,
                      XIGrabModeAsync, XIGrabModeAsync,
                      TRUE, /* owner_events */
                      &mask);

  return (ret == Success);
}

static gboolean _do_grab_pointer(MetaScreen* screen, GtkWidget* w, 
        guint32 timestamp)
{
    GdkDisplay* gdisplay = gdk_x11_lookup_xdisplay(screen->display->xdisplay);
    GdkDeviceManager* dev_man = gdk_display_get_device_manager(gdisplay);
    GdkDevice* pointer = gdk_device_manager_get_client_pointer(dev_man);

    g_assert(gdk_device_get_source(pointer) == GDK_SOURCE_MOUSE);

    GdkGrabStatus ret = gdk_device_grab(pointer, gtk_widget_get_window(w), 
            GDK_OWNERSHIP_APPLICATION, TRUE,
            GDK_BUTTON_PRESS_MASK| GDK_BUTTON_RELEASE_MASK| 
            GDK_ENTER_NOTIFY_MASK| GDK_FOCUS_CHANGE_MASK,
            NULL, timestamp);
    return (ret == GDK_GRAB_SUCCESS);
}

static void _do_grab(MetaScreen* screen, GtkWidget* w, gboolean grab_keyboard)
{
    MetaDisplay* display = meta_get_display();
    Window xwin = GDK_WINDOW_XID(gtk_widget_get_window(w));

    guint32 timestamp = meta_display_get_current_time_roundtrip(screen->display);
    if (grab_keyboard) {
        if (!_grab_device(display, xwin, META_VIRTUAL_CORE_KEYBOARD_ID,
                timestamp)) {
            meta_verbose("grab keyboard failed\n");
        }
    }

    if (!_do_grab_pointer(screen, w, timestamp)) {
        meta_verbose("grab pointer failed\n");
    }
}

typedef struct PopupData_ 
{
    MetaScreen* screen;
    MetaWindow *initial_selection;
    guint timestamp;
    MetaKeyBinding* binding;
} PopupData;

static gboolean on_delayed_popup(PopupData* pd)
{
    MetaScreen* screen = pd->screen;
    MetaDisplay* display = screen->display;


    if (display->grab_op != META_GRAB_OP_NONE) {
        meta_verbose("%s", __func__);
        if (!primary_modifier_still_pressed (display,
                    pd->binding->mask)) {
            /* This handles a race where modifier might be released
             * before we establish the grab. must end grab
             * prior to trying to focus a window.
             */
            meta_topic (META_DEBUG_FOCUS,
                    "Ending grab, activating %s, and turning off "
                    "mouse_mode due to switch/cycle windows where "
                    "modifier was released prior to grab\n",
                    pd->initial_selection->desc);
            meta_display_end_grab_op (display, pd->timestamp);
            display->mouse_mode = FALSE;
            _activate_selection_or_desktop(pd->initial_selection, pd->timestamp);
        } else {
            if (!screen->show_desktop_before_grab)
                meta_screen_show_desktop(screen, pd->timestamp);

            deepin_tab_popup_set_showing(screen->tab_popup, TRUE);

            /* rely on auto ungrab when destroyed */
            _do_grab(screen, screen->tab_popup->window, FALSE);
        }
    }

    g_slice_free(PopupData, pd);
    return G_SOURCE_REMOVE;
}

static void do_choose_window (MetaDisplay    *display,
        MetaScreen     *screen,
        MetaWindow     *event_window,
        XIDeviceEvent         *event,
        MetaKeyBinding *binding,
        gboolean        backward)
{
    MetaTabList type = (MetaTabList)binding->handler->data;
    MetaWindow *initial_selection;

    /* reverse direction according to initial backward state */
    if (event->mods.base & ShiftMask)
        backward = !backward;

    initial_selection = meta_display_get_tab_next (display, type,
            screen, screen->active_workspace, NULL, backward);
    if (screen->active_workspace->showing_desktop) {
        initial_selection = meta_display_get_tab_next (display, type,
                screen, screen->active_workspace, initial_selection, backward);
    }

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
            _activate_selection_or_desktop(initial_selection, event->time);

        } else if (meta_display_begin_grab_op (display,
                    screen,
                    NULL,
                    tab_op_from_tab_type (type),
                    FALSE,
                    FALSE,
                    0,
                    binding->mask,
                    event->time,
                    0, 0)) {

            if (g_list_length(screen->tab_popup->entries) <= 1) {
                meta_display_end_grab_op (display, event->time);
                display->mouse_mode = FALSE;
                return;
            }

            PopupData* pd = (PopupData*)g_slice_alloc(sizeof(PopupData));
            pd->initial_selection = initial_selection;
            pd->screen = screen;
            pd->timestamp = event->time;
            pd->binding = binding;

            deepin_tab_popup_select(screen->tab_popup,
                    (MetaTabEntryKey)initial_selection->xwindow);
            g_timeout_add(POPUP_DELAY_TIMEOUT, (GSourceFunc)on_delayed_popup, pd);
        }
    }
}

static void handle_switch(MetaDisplay *display, MetaScreen *screen,
        MetaWindow *window, XIDeviceEvent *event,
        MetaKeyBinding *binding, gpointer user_data)
{
    gint backwards = (binding->handler->flags & META_KEY_BINDING_IS_REVERSED) != 0;
    meta_verbose("%s: backwards %d\n", __func__, backwards);
    do_choose_window (display, screen, window, event, binding, backwards);
}

static void on_drag_end(DeepinMessageHub* hub, GtkWidget* top)
{
    meta_verbose("on drag done, regrab");
    MetaDisplay* display = meta_get_display();
    _do_grab(display->active_screen, top, TRUE);
}

void do_preview_workspace(MetaDisplay *display, MetaScreen *screen,
        MetaWindow *window, guint32 timestamp,
        MetaKeyBinding *binding, gpointer user_data, 
        gboolean user_op)
{
    meta_verbose("%s\n", __func__);
    unsigned int grab_mask = binding ? binding->mask: 0;
    if (meta_display_begin_grab_op (display,
                screen,
                NULL,
                META_GRAB_OP_KEYBOARD_PREVIEWING_WORKSPACE,
                FALSE,
                FALSE,
                0,
                grab_mask,
                timestamp,
                0, 0))
    {
        gboolean grabbed_before_release = 
            user_op? primary_modifier_still_pressed (display, grab_mask): TRUE;

        meta_topic (META_DEBUG_KEYBINDINGS, "Activating workspace preview\n");

        if (!grabbed_before_release) {
            /* end the grab right away, modifier possibly released
             * before we could establish the grab and receive the
             * release event. Must end grab before we can switch
             * spaces.
             */
            meta_verbose("not grabbed_before_release\n");
            meta_display_end_grab_op (display, timestamp);
            return;
        }

        deepin_wm_background_setup(screen->ws_previewer);

        gtk_widget_show_all(GTK_WIDGET(screen->ws_previewer));
        gtk_window_move(GTK_WINDOW(screen->ws_previewer), 0, 0);
        gtk_window_set_focus(GTK_WINDOW(screen->ws_previewer), NULL);

        g_signal_connect(G_OBJECT(deepin_message_hub_get()),
                "drag-end", (GCallback)on_drag_end, screen->ws_previewer);

        /* rely on auto ungrab when destroyed */
        _do_grab(screen, (GtkWidget*)screen->ws_previewer, TRUE);
    }
}

static void handle_preview_workspace(MetaDisplay *display, MetaScreen *screen,
        MetaWindow *window, XIDeviceEvent *event,
        MetaKeyBinding *binding, gpointer user_data)
{
    if (!display->focus_window) {
        MetaWindow* focus_window = meta_stack_get_default_focus_window (
                screen->stack, screen->active_workspace, NULL);
        if (focus_window) {
            meta_window_focus (focus_window, event->time);
        } else {
            XSetInputFocus (display->xdisplay, screen->xroot,
                    RevertToPointerRoot, event->time);
        }
    }

    do_preview_workspace(display, screen, window, event->time, binding,
            user_data, TRUE);
}

enum {
    EXPOSE_WORKSPACE = 1,
    EXPOSE_ALL_WINDOWS = 2
};

static void handle_expose_windows(MetaDisplay *display, MetaScreen *screen,
        MetaWindow *window, XIDeviceEvent *event,
        MetaKeyBinding *binding, gpointer user_data)
{
    meta_verbose("%s\n", __func__);
    int expose_mode = binding->handler->data;

    unsigned int grab_mask = binding->mask;
    if (meta_display_begin_grab_op (display,
                screen,
                NULL,
                META_GRAB_OP_KEYBOARD_EXPOSING_WINDOWS,
                FALSE,
                FALSE,
                0,
                grab_mask,
                event->time,
                0, 0))
    {
        meta_topic (META_DEBUG_KEYBINDINGS, "Activating workspace preview\n");

        GtkWidget* top = screen->exposing_windows_popup;

        DeepinWorkspaceOverview* active_workspace = deepin_workspace_overview_new();
        if (expose_mode == EXPOSE_ALL_WINDOWS) {
            deepin_workspace_overview_set_show_all_windows(active_workspace, TRUE);
        }
        deepin_workspace_overview_populate(active_workspace, screen->active_workspace);

        gtk_container_add(GTK_CONTAINER(top), (GtkWidget*)active_workspace);

        gtk_window_move(GTK_WINDOW(top), 0, 0);
        gtk_widget_show_all(top);

        g_signal_connect(G_OBJECT(deepin_message_hub_get()),
                "drag-end", (GCallback)on_drag_end, top);

        _do_grab(screen, top, TRUE);
    }
}

static void handle_workspace_switch(MetaDisplay *display, MetaScreen *screen,
        MetaWindow *window, XIDeviceEvent *event,
        MetaKeyBinding *binding, gpointer user_data)
{
    gint motion;
    unsigned int grab_mask;

    /* Don't show the ws switcher if we get just one ws */
    if (meta_screen_get_n_workspaces(screen) == 1)
        return;

    if (screen->all_keys_grabbed) return;

    MetaKeyBindingAction action = meta_prefs_get_keybinding_action(binding->name);
    if (action == META_KEYBINDING_ACTION_WORKSPACE_RIGHT) {
        meta_verbose("%s: to right\n", __func__);
        motion = META_MOTION_RIGHT;
    } else if (action == META_KEYBINDING_ACTION_WORKSPACE_LEFT) {
        motion = META_MOTION_LEFT;
        meta_verbose("%s: to left\n", __func__);
    } else {
        return;
    }

    grab_mask = binding->mask;
    if (meta_display_begin_grab_op (display,
                screen,
                NULL,
                META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING,
                FALSE,
                FALSE,
                0,
                grab_mask,
                event->time,
                0, 0))
    {
        MetaWorkspace *next;
        gboolean grabbed_before_release;

        next = meta_workspace_get_neighbor(screen->active_workspace, motion);

        grabbed_before_release = primary_modifier_still_pressed (display, grab_mask);

        if (!(next && grabbed_before_release)) {
            meta_display_end_grab_op (display, event->time);
            return;
        }

        if (next != screen->active_workspace) {
            meta_workspace_activate(next, event->time);

            gtk_widget_show_all(screen->ws_popup);

            GtkWidget* w = gtk_bin_get_child(GTK_BIN(screen->ws_popup));
            DeepinWorkspaceIndicator* indi = DEEPIN_WORKSPACE_INDICATOR(w);
            deepin_workspace_indicator_request_workspace_change(indi, next);
        }
    }
}

static void handle_move_to_workspace  (MetaDisplay    *display,
                              MetaScreen     *screen,
                              MetaWindow     *window,
                              XIDeviceEvent         *event,
                              MetaKeyBinding *binding)
{
    MetaWorkspace *workspace;
    MetaMotionDirection motion;

    if (window->always_sticky)
        return;

    MetaKeyBindingAction action = meta_prefs_get_keybinding_action(binding->name);
    if (action == META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_RIGHT) {
        meta_verbose("%s: to right\n", __func__);
        motion = META_MOTION_RIGHT;
    } else if (action == META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_LEFT) {
        motion = META_MOTION_LEFT;
        meta_verbose("%s: to left\n", __func__);
    } else {
        return;
    }
    workspace = meta_workspace_get_neighbor(screen->active_workspace, motion);

    if (workspace) {
        /* Activate second, so the window is never unmapped */
        meta_window_change_workspace (window, workspace);
        workspace->screen->display->mouse_mode = FALSE;
        meta_workspace_activate_with_focus (workspace,
                window, event->time);
        meta_screen_ensure_workspace_popup(workspace->screen);
        gtk_widget_show_all(screen->ws_popup);

        GtkWidget* w = gtk_bin_get_child(GTK_BIN(screen->ws_popup));
        DeepinWorkspaceIndicator* indi = DEEPIN_WORKSPACE_INDICATOR(w);
        deepin_workspace_indicator_request_workspace_change(indi, workspace);
    }
}

void deepin_init_custom_handlers(MetaDisplay* display)
{
    deepin_meta_override_keybinding_handler("switch-applications",
            handle_switch, NULL, NULL);
    deepin_meta_override_keybinding_handler("switch-applications-backward",
            handle_switch, NULL, NULL);

    deepin_meta_override_keybinding_handler("switch-group",
            handle_switch, NULL, NULL);
    deepin_meta_override_keybinding_handler("switch-group-backward",
            handle_switch, NULL, NULL);
                          

    deepin_meta_override_keybinding_handler("switch-to-workspace-right",
            handle_workspace_switch, NULL, NULL);
    deepin_meta_override_keybinding_handler("switch-to-workspace-left",
            handle_workspace_switch, NULL, NULL);

    deepin_meta_override_keybinding_handler("move-to-workspace-left",
            handle_move_to_workspace, NULL, NULL);
    deepin_meta_override_keybinding_handler("move-to-workspace-right",
            handle_move_to_workspace, NULL, NULL);
                          

    deepin_meta_display_add_keybinding(display, "expose-all-windows",
            META_KEY_BINDING_NONE, handle_expose_windows, EXPOSE_ALL_WINDOWS);
    deepin_meta_display_add_keybinding(display, "expose-windows",
            META_KEY_BINDING_NONE, handle_expose_windows, EXPOSE_WORKSPACE);
    deepin_meta_display_add_keybinding(display, "preview-workspace",
            META_KEY_BINDING_NONE, handle_preview_workspace, 2);
}

