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
#include "deepin-cloned-widget.h"
#include "deepin-tab-widget.h"
#include "deepin-fixed.h"
#include "select-workspace.h"
#include "deepin-design.h"
#include "deepin-switch-previewer.h"

static GdkPixbuf* dimm_icon (GdkPixbuf *pixbuf)
{
    int x, y, pixel_stride, row_stride;
    guchar *row, *pixels;
    int w, h;
    GdkPixbuf *dimmed_pixbuf;

    if (gdk_pixbuf_get_has_alpha (pixbuf)) {
        dimmed_pixbuf = gdk_pixbuf_copy (pixbuf);
    } else {
        dimmed_pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
    }

    w = gdk_pixbuf_get_width (dimmed_pixbuf);
    h = gdk_pixbuf_get_height (dimmed_pixbuf);

    pixel_stride = 4;

    row = gdk_pixbuf_get_pixels (dimmed_pixbuf);
    row_stride = gdk_pixbuf_get_rowstride (dimmed_pixbuf);

    for (y = 0; y < h; y++) {
        pixels = row;
        for (x = 0; x < w; x++) {
            pixels[3] /= 2;
            pixels += pixel_stride;
        }
        row += row_stride;
    }
    return dimmed_pixbuf;
}

static DeepinTabEntry* deepin_tab_entry_new (const MetaTabEntry *entry, gint                screen_width)
{
    DeepinTabEntry *te;

    te = g_new (DeepinTabEntry, 1);
    te->key = entry->key;
    te->title = NULL;
    if (entry->title)
    {
        gchar *str;
        gchar *tmp;
        gchar *formatter = "%s";

        str = meta_g_utf8_strndup (entry->title, 4096);

        if (entry->hidden)
        {
            formatter = "[%s]";
        }

        tmp = g_markup_printf_escaped (formatter, str);
        g_free (str);
        str = tmp;

        if (entry->demands_attention)
        {
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
    te->icon = entry->icon;
    te->blank = entry->blank;
    te->dimmed_icon = NULL;
    if (te->icon)
    {
        g_object_ref (G_OBJECT (te->icon));
        if (entry->hidden)
            te->dimmed_icon = dimm_icon (entry->icon);
    }

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
    GtkStyleContext* style_ctx = gtk_widget_get_style_context(popup->window);

    GtkCssProvider* css_style = gtk_css_provider_new();

    GFile* f = g_file_new_for_path(METACITY_PKGDATADIR "/deepin-wm.css");
    GError* error = NULL;
    if (!gtk_css_provider_load_from_file(css_style, f, &error)) {
        meta_topic(META_DEBUG_UI, "load css failed: %s", error->message);
        g_error_free(error);
        return;
    }

    gtk_style_context_add_provider(style_ctx,
            GTK_STYLE_PROVIDER(css_style), GTK_STYLE_PROVIDER_PRIORITY_USER);
    gtk_style_context_add_class(style_ctx, "deepin-window-switcher");

    GList* tmp = popup->entries;
    while (tmp) {
        DeepinTabEntry* te = (DeepinTabEntry*)tmp->data;
        if (te->widget && META_IS_DEEPIN_TAB_WIDGET(te->widget)) {
            style_ctx = gtk_widget_get_style_context(te->widget);
            gtk_style_context_add_provider(style_ctx,
                    GTK_STYLE_PROVIDER(css_style), GTK_STYLE_PROVIDER_PRIORITY_USER);
            gtk_style_context_add_class(style_ctx, "deepin-window-switcher-item");
        }
        tmp = tmp->next;
    }

    g_object_unref(f);
    g_object_unref(css_style);
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

    meta_topic(META_DEBUG_UI, 
            "%s: box(%f, %f), item (%f, %f), cols %d, rows %d, scale %g",
            __func__, box_width, box_height, item_width, item_height,
            width, height, item_scale);

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
            w = meta_deepin_tab_widget_new(te->dimmed_icon? te->dimmed_icon: te->icon);
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
    if (te->icon)
        g_object_unref (G_OBJECT (te->icon));
    if (te->dimmed_icon)
        g_object_unref (G_OBJECT (te->dimmed_icon));

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
