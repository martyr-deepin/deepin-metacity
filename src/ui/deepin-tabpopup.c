/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity popup window thing showing windows you can tab to */


/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#include <config.h>

#include <gtk/gtk.h>
#include <math.h>
#include "util.h"
#include "core.h"
#include "deepin-tabpopup.h"
#include "deepin-tab-widget.h"
#include "deepin-fixed.h"
#include "select-workspace.h"
#include "deepin-design.h"
#include "deepin-message-hub.h"
#include "deepin-window-surface-manager.h"
#include "deepin-background-cache.h"
#include "deepin-desktop-background.h"

static DeepinTabEntry* deepin_tab_entry_new (const MetaTabEntry *entry)
{
    DeepinTabEntry *te;

    te = g_new (DeepinTabEntry, 1);
    te->key = entry->key;
    te->title = NULL;
    if (entry->title) {
        gchar *str;
        gchar *tmp;
        gchar *formatter = "%s";

        str = meta_g_utf8_strndup (entry->title, 4096);

        if (entry->hidden) {
            formatter = "[%s]";
        }

        tmp = g_markup_printf_escaped (formatter, str);
        g_free (str);
        str = tmp;

        if (entry->demands_attention) {
            /* Escape the whole line of text then markup the text and
             * copy it back into the original buffer.
             */
            tmp = g_strdup_printf ("<b>%s</b>", str);
            g_free (str);
            str = tmp;
        }

        te->title=g_strdup(str);

        g_free (str);
    }

    te->widget = NULL;
    te->blank = entry->blank;

    te->rect.x = entry->rect.x;
    te->rect.y = entry->rect.y;
    te->rect.width = entry->rect.width;
    te->rect.height = entry->rect.height;

    te->inner_rect.x = entry->inner_rect.x;
    te->inner_rect.y = entry->inner_rect.y;
    te->inner_rect.width = entry->inner_rect.width;
    te->inner_rect.height = entry->inner_rect.height;

    return te;
}

static void deepin_tab_popup_setup_style(DeepinTabPopup* popup)
{
    // use shenwei version of style for fastest performance
    const char* styles[2] = {
        "deepin-window-switcher-sw",
        "deepin-window-switcher-item-sw"
    };

    deepin_setup_style_class(popup->window, styles[0]);

    GList* tmp = popup->entries;
    while (tmp) {
        DeepinTabEntry* te = (DeepinTabEntry*)tmp->data;
        if (te->widget && META_IS_DEEPIN_TAB_WIDGET(te->widget)) {
            deepin_setup_style_class(te->widget, styles[1]);
        }
        tmp = tmp->next;
    }
}

static MetaTabEntry* _desktop_entry(DeepinTabPopup* popup)
{
    MetaTabEntry* entry = g_new0(MetaTabEntry, 1);

    MetaDisplay* display = meta_get_display();

    MetaWindow *window = NULL;
    MetaRectangle r;

    GSList* l = meta_display_list_windows(display);
    for (GSList* t = l; t; t = t->next) {
        MetaWindow* win = (MetaWindow*)t->data;
        if (win->type == META_WINDOW_DESKTOP) {
            window = win;
            break;
        }
    }
    g_slist_free(l);
    if (!window) return NULL;

    entry->key = (MetaTabEntryKey) window->xwindow;
    entry->title = window->title;

    entry->blank = FALSE;
    entry->hidden = !meta_window_showing_on_its_workspace (window);
    entry->demands_attention = window->wm_state_demands_attention;

    meta_window_get_outer_rect (window, &r);
    entry->rect = r;

    return entry;
}

static gboolean on_thumb_click_finished(MetaWindow* meta_win)
{
    MetaScreen* screen = meta_win->screen;
    MetaDisplay* display = screen->display;

    display->mouse_mode = FALSE;

    if (meta_win->type != META_WINDOW_DESKTOP) {
        meta_screen_unshow_desktop(screen);
        meta_window_activate(meta_win, gtk_get_current_event_time());

    } else if (!screen->active_workspace->showing_desktop) {
        meta_screen_show_desktop(screen, gtk_get_current_event_time());
    }

    meta_display_end_grab_op (display, gtk_get_current_event_time());
    return G_SOURCE_REMOVE;
}

static gboolean on_thumb_clicked(MetaDeepinTabWidget* tab,
               GdkEvent* event, gpointer user_data)
{
    meta_verbose("%s", __func__);

    DeepinTabPopup* popup = (DeepinTabPopup*)user_data;
    MetaWindow* meta_win = meta_deepin_tab_widget_get_meta_window(tab);

    deepin_tab_popup_select(popup, (MetaTabEntryKey)meta_win->xwindow);
  
    g_timeout_add(SWITCHER_SELECT_ANIMATION_DURATION, 
            (GSourceFunc)on_thumb_click_finished, meta_win);

    return TRUE;
}

static gboolean on_idle_relayout(DeepinTabPopup* self)
{
    int height;
    int width;
    float box_width, box_height;
    float item_width, item_height;
    int entry_count = g_list_length(self->entries);

    calculate_preferred_size(entry_count, self->max_width,
            &box_width, &box_height, &item_width, &item_height, &width);

    height = entry_count / width;
    if (entry_count % width)
        height += 1;

    GtkWidget* grid = gtk_bin_get_child(GTK_BIN(self->window));

    int left = 0, top = 0;
    GList* tmp = self->entries;
    while (tmp) {
        DeepinTabEntry *te = (DeepinTabEntry*)tmp->data;

        deepin_fixed_move(DEEPIN_FIXED(grid), te->widget, 
                left * item_width + item_width / 2,
                top * (item_height + SWITCHER_ROW_SPACING) 
                + (item_height + SWITCHER_ROW_SPACING) / 2,
                FALSE);

        left++;
        if (left >= width) {
            left = 0, top++;
        }
        tmp = tmp->next;
    }

    return G_SOURCE_REMOVE;
}

static void free_deepin_tab_entry (gpointer data, gpointer user_data)
{
    DeepinTabEntry *te;

    te = (DeepinTabEntry*)data;

    g_free (te->title);

    g_free (te);
}

static void display_entry (DeepinTabPopup *popup,
        DeepinTabEntry     *te)
{
    if (popup->current_selected_entry) {
        meta_deepin_tab_widget_unselect (META_DEEPIN_TAB_WIDGET (popup->current_selected_entry->widget));
    }

    meta_deepin_tab_widget_select (META_DEEPIN_TAB_WIDGET (te->widget));

    MetaDisplay* display = meta_get_display();
    MetaWindow* window = meta_display_lookup_x_window(display, (Window)te->key);    
    guint32 time = meta_display_get_current_time_roundtrip(display);
    if (window->type != META_WINDOW_DESKTOP) {
        meta_window_activate(window, time);
    } else {
        meta_screen_show_desktop(window->screen, time); 
    }

    popup->current_selected_entry = te;
}


static void on_window_removed(DeepinMessageHub* hub, MetaWindow* window, 
        gpointer data)
{
    if (window->type != META_WINDOW_NORMAL) return;

    DeepinTabPopup* self = (DeepinTabPopup*)data;
    MetaDisplay* display = meta_get_display();

    GList* tmp = self->entries;
    while (tmp) {
        DeepinTabEntry *te = (DeepinTabEntry*)tmp->data;
        MetaWindow* meta_win = meta_display_lookup_x_window(display, (Window)te->key);
        if (meta_win == window) {
            self->entries = g_list_remove_link(self->entries, tmp);

            GtkWidget* grid = gtk_bin_get_child(GTK_BIN(self->window));
            gtk_container_remove(GTK_CONTAINER(grid), te->widget);

            if (self->current == tmp) {
                self->current = tmp->next;
                if (self->current == NULL)
                    self->current = self->entries;
                DeepinTabEntry *te = (DeepinTabEntry*)self->current->data;
                display_entry(self, te);
            }

            free_deepin_tab_entry(te, NULL);
            g_list_free(tmp);

            break;
        }

        tmp = tmp->next;
    }

    self->idle_relayout_ids = g_list_append(self->idle_relayout_ids, 
            GUINT_TO_POINTER(g_idle_add((GSourceFunc)on_idle_relayout, data)));
}

static void on_window_change_workspace(DeepinMessageHub* hub, MetaWindow* window,
        MetaWorkspace* new_workspace, gpointer user_data)
{
    MetaWorkspace* active_ws = new_workspace->screen->active_workspace;
    
    if (active_ws != new_workspace) { // dest workspace
        on_window_removed(hub, window, user_data);
    }
}

static void on_screen_changed(DeepinMessageHub* hub, MetaScreen* screen,
        gpointer data)
{
    DeepinTabPopup* self = (DeepinTabPopup*)data;

    GdkScreen* gdkscreen = gdk_screen_get_default();
    GdkRectangle mon_geom;
    gint primary = gdk_screen_get_primary_monitor(gdkscreen);
    gdk_screen_get_monitor_geometry(gdkscreen, primary, &mon_geom);

    GtkAllocation alloc;
    gtk_widget_get_allocation(self->window, &alloc);
    gtk_window_move (GTK_WINDOW (self->window), 
            mon_geom.x + (mon_geom.width - alloc.width)/2,
            mon_geom.y + (mon_geom.height - alloc.height)/2);
}

static void on_size_changed(GtkWidget *top, GdkRectangle *alloc,
               gpointer data)
{
    GdkRectangle mon_geom;
    gint primary = gdk_screen_get_primary_monitor(gdk_screen_get_default());
    gdk_screen_get_monitor_geometry(gdk_screen_get_default(), primary, &mon_geom);

    gtk_window_move (GTK_WINDOW (top), 
            mon_geom.x + (mon_geom.width - alloc->width)/2,
            mon_geom.y + (mon_geom.height - alloc->height)/2);
}

DeepinTabPopup* deepin_tab_popup_new (const MetaTabEntry *entries,
        int                 screen_number,
        int                 entry_count,
        gboolean            show_desktop)
{
    DeepinTabPopup *popup;
    int i;
    int height;
    int width;
    GtkWidget *grid;
    GList *tmp;
    GdkScreen *screen;
    GdkVisual *visual;
    GdkRectangle mon_geom;
    int max_width;
    float box_width, box_height;
    float item_width, item_height;

    popup = g_new (DeepinTabPopup, 1);

    screen = gdk_display_get_screen (gdk_display_get_default (),
            screen_number);
    visual = gdk_screen_get_rgba_visual (screen);

    gint primary = gdk_screen_get_primary_monitor(screen);
    gdk_screen_get_monitor_geometry(screen, primary, &mon_geom);

    popup->window = gtk_window_new (GTK_WINDOW_POPUP);

    gtk_window_set_screen (GTK_WINDOW (popup->window), screen);

    /* enable resizing, to get never-shrink behavior */
    gtk_window_set_resizable (GTK_WINDOW (popup->window), TRUE);
    popup->current = NULL;
    popup->entries = NULL;
    popup->current_selected_entry = NULL;

    popup->max_width = max_width = 
        mon_geom.width - POPUP_SCREEN_PADDING * 2 - POPUP_PADDING * 2;
    for (i = 0; i < entry_count; ++i) {
        DeepinTabEntry* new_entry = deepin_tab_entry_new (&entries[i]);
        popup->entries = g_list_prepend (popup->entries, new_entry);
    }

#if 0
    if (show_desktop && entry_count > 1) {
        // desktop entry
        MetaTabEntry* tmp = _desktop_entry(popup);
        if (tmp) {
            DeepinTabEntry* new_entry = deepin_tab_entry_new(tmp);
            popup->entries = g_list_prepend (popup->entries, new_entry);
            g_free(tmp);

            entry_count++;
            i++;
        }
    }
#endif

    popup->entries = g_list_reverse (popup->entries);

    calculate_preferred_size(entry_count, max_width,
            &box_width, &box_height, &item_width, &item_height, &width);

    height = i / width;
    if (i % width)
        height += 1;

    grid = deepin_fixed_new();
    g_object_set(G_OBJECT(grid), "margin", POPUP_PADDING, NULL);
    gtk_widget_set_size_request(GTK_WIDGET(grid), box_width, box_height);
    gtk_container_add (GTK_CONTAINER (popup->window), grid);

    MetaDisplay* display = meta_get_display();

    int left = 0, top = 0;
    tmp = popup->entries;
    while (tmp) {
        DeepinTabEntry *te = (DeepinTabEntry*)tmp->data;

        MetaWindow* meta_win = meta_display_lookup_x_window(display, (Window)te->key);
        GtkWidget* w = meta_deepin_tab_widget_new(meta_win);

        te->widget = w;
        deepin_fixed_put(DEEPIN_FIXED(grid), w, 
                left * item_width + item_width / 2,
                top * (item_height + SWITCHER_ROW_SPACING) + item_height / 2);

        g_object_connect(G_OBJECT(w), 
                "signal::button-release-event", on_thumb_clicked, popup,
                NULL);
        left++;
        if (left >= width) {
            left = 0, top++;
        }
        tmp = tmp->next;
    }

    deepin_tab_popup_setup_style(popup);

    g_object_connect(G_OBJECT(deepin_message_hub_get()), 
            "signal::window-removed", on_window_removed, popup,
            "signal::about-to-change-workspace", on_window_change_workspace, popup,
            "signal::screen-changed", on_screen_changed, popup,
            NULL);

    g_signal_connect(popup->window, "size-allocate", (GCallback)on_size_changed, popup);

    popup->idle_relayout_ids = NULL;

    return popup;
}

void deepin_tab_popup_free (DeepinTabPopup *popup)
{
    meta_verbose ("Destroying tab popup window\n");

    /* run at most l->len iteration for source removal */
    GList* l = popup->idle_relayout_ids;
    while (l) {
        g_source_remove_by_user_data(popup);
        l = l->next;
    }

    g_signal_handlers_disconnect_by_data(G_OBJECT(deepin_message_hub_get()), 
            popup);

    gtk_widget_destroy(popup->window);

    g_list_foreach (popup->entries, free_deepin_tab_entry, NULL);

    g_list_free (popup->entries);
    popup->entries = NULL;

    g_free (popup);
}

void deepin_tab_popup_set_showing (DeepinTabPopup *popup,
        gboolean      showing)
{
    if (showing)
    {
        gtk_widget_show_all (popup->window);
    }
    else
    {
        if (gtk_widget_get_visible (popup->window))
        {
            meta_verbose ("Hiding tab popup window\n");
            gtk_widget_hide (popup->window);
            meta_core_increment_event_serial (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
        }
    }
}

void deepin_tab_popup_forward (DeepinTabPopup *popup)
{
    if (popup->current != NULL)
        popup->current = popup->current->next;

    if (popup->current == NULL)
        popup->current = popup->entries;

    if (popup->current != NULL) {
        DeepinTabEntry *te;

        te = popup->current->data;

        display_entry (popup, te);
    }
}

void deepin_tab_popup_backward (DeepinTabPopup *popup)
{
    if (popup->current != NULL)
        popup->current = popup->current->prev;

    if (popup->current == NULL)
        popup->current = g_list_last (popup->entries);

    if (popup->current != NULL) {
        DeepinTabEntry *te;

        te = popup->current->data;

        display_entry (popup, te);
    }
}

MetaTabEntryKey deepin_tab_popup_get_selected (DeepinTabPopup *popup)
{
    if (popup->current) {
        DeepinTabEntry *te;

        te = popup->current->data;

        return te->key;
    }
    else
        return (MetaTabEntryKey)None;
}

void deepin_tab_popup_select (DeepinTabPopup *popup,
        DeepinTabEntryKey key)
{
    GList *tmp;

    /* Note, "key" may not be in the list of entries; other code assumes
     * it's OK to pass in a key that isn't.
     */

    tmp = popup->entries;
    while (tmp != NULL) {
        DeepinTabEntry *te;

        te = tmp->data;

        if (te->key == key) {
            popup->current = tmp;

            display_entry (popup, te);

            return;
        }

        tmp = tmp->next;
    }
}
