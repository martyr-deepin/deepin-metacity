/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity popup window thing showing windows you can tab to */

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
#include "deepin-switch-previewer.h"
#include "deepin-window-surface-manager.h"

static DeepinTabEntry* deepin_tab_entry_new (const MetaTabEntry *entry, gint                screen_width)
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
    te->icon = NULL;
    te->blank = entry->blank;

    te->rect.x = entry->rect.x;
    te->rect.y = entry->rect.y;
    te->rect.width = entry->rect.width;
    te->rect.height = entry->rect.height;

    te->inner_rect.x = entry->inner_rect.x;
    te->inner_rect.y = entry->inner_rect.y;
    te->inner_rect.width = entry->inner_rect.width;
    te->inner_rect.height = entry->inner_rect.height;

    MetaDisplay* display = meta_get_display();
    MetaWindow* window = meta_display_lookup_x_window(display, (Window)te->key);

    cairo_surface_t* ref = deepin_window_surface_manager_get_surface(window, 1.0);

    double sx = RECT_PREFER_WIDTH / (double)te->rect.width;
    double sy = RECT_PREFER_HEIGHT / (double)te->rect.height;

    cairo_surface_t* surface = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, RECT_PREFER_WIDTH, RECT_PREFER_HEIGHT);
    cairo_t* cr = cairo_create(surface);
    cairo_save(cr);
    cairo_scale(cr, sx, sy);
    cairo_set_source_surface(cr, ref, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);

#define ICON_SIZE 48
    GdkPixbuf* scaled = gdk_pixbuf_scale_simple (window->icon,
            ICON_SIZE, ICON_SIZE, GDK_INTERP_BILINEAR);
    double icon_width = gdk_pixbuf_get_width (scaled);
    double icon_height = gdk_pixbuf_get_height (scaled);

    gdk_cairo_set_source_pixbuf(cr, scaled,
            (RECT_PREFER_WIDTH - icon_width)/2.0,
            (RECT_PREFER_HEIGHT - icon_height));
    cairo_paint(cr);

    g_object_unref(scaled);
    cairo_destroy(cr);

    te->icon = surface;
    cairo_surface_reference(te->icon);
    return te;
}

static void deepin_tab_popup_setup_style(DeepinTabPopup* popup)
{
    deepin_setup_style_class(popup->window, "deepin-window-switcher");

    GList* tmp = popup->entries;
    while (tmp) {
        DeepinTabEntry* te = (DeepinTabEntry*)tmp->data;
        if (te->widget && META_IS_DEEPIN_TAB_WIDGET(te->widget)) {
            deepin_setup_style_class(te->widget, "deepin-window-switcher-item");
        }
        tmp = tmp->next;
    }
}

DeepinTabPopup* deepin_tab_popup_new (const MetaTabEntry *entries,
        int                 screen_number,
        int                 entry_count)
{
    DeepinTabPopup *popup;
    int i;
    int height;
    int width;
    GtkWidget *grid;
    GList *tmp;
    GdkScreen *screen;
    GdkVisual *visual;
    int screen_width;
    int max_width;
    float box_width, box_height;
    double item_scale = 1.0;
    float item_width, item_height;

    deepin_window_surface_manager_flush();

    popup = g_new (DeepinTabPopup, 1);

    screen = gdk_display_get_screen (gdk_display_get_default (),
            screen_number);
    visual = gdk_screen_get_rgba_visual (screen);

    popup->outline_window = gtk_window_new (GTK_WINDOW_POPUP);

    if (visual)
        gtk_widget_set_visual (popup->outline_window, visual);

    gtk_window_set_screen (GTK_WINDOW (popup->outline_window),
            screen);

    gtk_window_set_position (GTK_WINDOW (popup->outline_window),
            GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_default_size(GTK_WINDOW(popup->outline_window), 
            gdk_screen_get_width(screen), 
            gdk_screen_get_height(screen));
    gtk_widget_realize (popup->outline_window);

    popup->previewer = (MetaDeepinSwitchPreviewer*)meta_deepin_switch_previewer_new(popup);
    gtk_container_add(GTK_CONTAINER(popup->outline_window), (GtkWidget*)popup->previewer);

    popup->window = gtk_window_new (GTK_WINDOW_POPUP);
    if (visual)
        gtk_widget_set_visual (popup->window, visual);

    gtk_window_set_screen (GTK_WINDOW (popup->window),
            screen);

    gtk_window_set_position (GTK_WINDOW (popup->window),
            GTK_WIN_POS_CENTER_ALWAYS);
    /* enable resizing, to get never-shrink behavior */
    gtk_window_set_resizable (GTK_WINDOW (popup->window),
            TRUE);
    popup->current = NULL;
    popup->entries = NULL;
    popup->current_selected_entry = NULL;

    screen_width = gdk_screen_get_width (screen);
    max_width = screen_width - POPUP_SCREEN_PADDING * 2 - POPUP_PADDING * 2;
    for (i = 0; i < entry_count; ++i) {
        DeepinTabEntry* new_entry = deepin_tab_entry_new (&entries[i], screen_width);
        popup->entries = g_list_prepend (popup->entries, new_entry);
    }

    popup->entries = g_list_reverse (popup->entries);

    calculate_preferred_size(entry_count, max_width,
            &box_width, &box_height, &item_width, &item_height, &width);

    item_scale = item_width / SWITCHER_ITEM_PREFER_WIDTH;
    height = i / width;
    if (i % width)
        height += 1;


    grid = deepin_fixed_new();
    g_object_set(G_OBJECT(grid), "margin", POPUP_PADDING, NULL);
    gtk_widget_set_size_request(GTK_WIDGET(grid), box_width, box_height);
    gtk_container_add (GTK_CONTAINER (popup->window), grid);

    int left = 0, top = 0;
    tmp = popup->entries;
    while (tmp) {
        GtkWidget *w;

        DeepinTabEntry *te = (DeepinTabEntry*)tmp->data;

        if (!te->blank) {
            w = meta_deepin_tab_widget_new(te->icon);
            meta_deepin_tab_widget_set_scale(META_DEEPIN_TAB_WIDGET(w), item_scale);
        }

        te->widget = w;
        deepin_fixed_put(DEEPIN_FIXED(grid), w, 
                left * (item_width + SWITCHER_COLUMN_SPACING) 
                + (item_width + SWITCHER_COLUMN_SPACING) / 2,
                top * (item_height + SWITCHER_ROW_SPACING) 
                + (item_height + SWITCHER_ROW_SPACING) / 2);

        left++;
        if (left >= width) {
            left = 0, top++;
        }
        tmp = tmp->next;
    }

    deepin_tab_popup_setup_style(popup);

    meta_deepin_switch_previewer_populate(popup->previewer);
    gtk_widget_show_all(popup->outline_window);
    GdkWindow *window = gtk_widget_get_window (popup->outline_window);
    gdk_window_raise(window);

    return popup;
}

static void free_deepin_tab_entry (gpointer data, gpointer user_data)
{
    DeepinTabEntry *te;

    te = (DeepinTabEntry*)data;

    g_free (te->title);
    if (te->icon) cairo_surface_destroy (te->icon);

    g_free (te);
}

void deepin_tab_popup_free (DeepinTabPopup *popup)
{
    meta_verbose ("Destroying tab popup window\n");

    if (popup->outline_window != NULL) {
        gtk_widget_destroy (GTK_WIDGET(popup->previewer));
        gtk_widget_destroy (popup->outline_window);
    }
    gtk_widget_destroy (popup->window);

    g_list_foreach (popup->entries, free_deepin_tab_entry, NULL);

    g_list_free (popup->entries);

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

static void display_entry (DeepinTabPopup *popup,
        DeepinTabEntry     *te)
{
    if (popup->current_selected_entry) {
        meta_deepin_tab_widget_unselect (META_DEEPIN_TAB_WIDGET (popup->current_selected_entry->widget));
    }

    meta_deepin_tab_widget_select (META_DEEPIN_TAB_WIDGET (te->widget));
    meta_deepin_switch_previewer_select(popup->previewer, te);

    /* Must be before we handle an expose for the outline window */
    popup->current_selected_entry = te;
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
