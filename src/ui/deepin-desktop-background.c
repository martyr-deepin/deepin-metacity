/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#include "deepin-desktop-background.h"
#include <gdk/gdkx.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include "deepin-background-cache.h"
#include "deepin-window-surface-manager.h"
#include "../core/workspace.h"
#include "../core/window-private.h"
#include "../core/display-private.h"
#include "../core/screen-private.h"
#include "deepin-message-hub.h"

struct _DeepinDesktopBackgroundPrivate
{
    MetaScreen* screen;
    GdkRectangle geometry;
    gint monitor;
    MetaWorkspace* active_workspace;
    cairo_surface_t *last_background;
    gint delay_switch_count;
};


G_DEFINE_TYPE (DeepinDesktopBackground, deepin_desktop_background, GTK_TYPE_WINDOW);

static void deepin_desktop_background_init (DeepinDesktopBackground *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
            DEEPIN_TYPE_DESKTOP_BACKGROUND, DeepinDesktopBackgroundPrivate);
    self->priv->last_background = NULL;
    self->priv->delay_switch_count = 5;
}

static void deepin_desktop_background_finalize (GObject *object)
{
    DeepinDesktopBackground* self = DEEPIN_DESKTOP_BACKGROUND(object);
    g_signal_handlers_disconnect_by_data(G_OBJECT(deepin_message_hub_get()), object);
    g_clear_pointer(&self->priv->last_background, cairo_surface_destroy);

    G_OBJECT_CLASS (deepin_desktop_background_parent_class)->finalize (object);
}

static gboolean deepin_desktop_background_real_draw(GtkWidget *widget, cairo_t* cr)
{

    DeepinDesktopBackground* self = DEEPIN_DESKTOP_BACKGROUND(widget);
    DeepinDesktopBackgroundPrivate *priv = self->priv;

    MetaScreen *screen = meta_get_display()->active_screen;

    if (priv->monitor >= gdk_screen_get_n_monitors(gdk_screen_get_default()))
        return FALSE;

    if (priv->last_background) {
        if (priv->delay_switch_count > 0) {
            cairo_set_source_surface(cr, priv->last_background, 0, 0);
            cairo_paint(cr);
            priv->delay_switch_count--;
        } else {
            g_clear_pointer(&priv->last_background, cairo_surface_destroy);
        }
    }

    int index = meta_workspace_index(screen->active_workspace);
    cairo_surface_t* bg = deepin_background_cache_get_surface(priv->monitor, index, 1.0);
    if (bg) {
        cairo_set_source_surface(cr, bg, 0, 0);
        cairo_paint(cr);
    }

    if (priv->delay_switch_count == 0) {
        if (priv->last_background != bg) {
            g_clear_pointer(&priv->last_background, cairo_surface_destroy);
            priv->last_background = cairo_surface_reference(bg);
        }
    }

    MetaDisplay *display = screen->display;
    if (display->hiding_windows_mode)
        return TRUE;

    if (priv->monitor != gdk_screen_get_primary_monitor(gdk_screen_get_default()))
        return TRUE;

    if (display->desktop_win && display->desktop_win->hidden)
        return TRUE;

    if (display->desktop_surface != NULL) {
        if (cairo_surface_status(display->desktop_surface) == 0) {
            cairo_set_source_surface(cr, display->desktop_surface, 0, 0);
            cairo_paint(cr);
        } else {
            meta_verbose ("%s: status %d\n", __func__, cairo_surface_status(display->desktop_surface));
        }
    }
    return TRUE;
}

static void deepin_desktop_background_class_init (DeepinDesktopBackgroundClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS (klass);

    g_type_class_add_private (klass, sizeof (DeepinDesktopBackgroundPrivate));
    widget_class->draw = deepin_desktop_background_real_draw;

    object_class->finalize = deepin_desktop_background_finalize;
}

static void on_desktop_changed(DeepinMessageHub* hub, DeepinDesktopBackground* self)
{
    self->priv->delay_switch_count = 5;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void on_workspace_switched(DeepinMessageHub* hub, int from, int to, DeepinDesktopBackground* self)
{
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

DeepinDesktopBackground* deepin_desktop_background_new(MetaScreen* screen, gint monitor)
{
    GtkWidget* widget = (GtkWidget*)g_object_new(DEEPIN_TYPE_DESKTOP_BACKGROUND,
            "type", GTK_WINDOW_TOPLEVEL, NULL);

    deepin_setup_style_class(widget, "deepin-window-manager"); 

    DeepinDesktopBackground* self = DEEPIN_DESKTOP_BACKGROUND(widget);
    DeepinDesktopBackgroundPrivate* priv = self->priv;
    priv->screen = screen;

    GdkDisplay* gdisplay = gdk_display_get_default();
    GdkScreen* gscreen = gdk_display_get_default_screen(gdisplay);

    GdkVisual* visual = gdk_screen_get_rgba_visual (gscreen);
    if (visual) gtk_widget_set_visual (widget, visual);

    priv->monitor = monitor;
    gdk_screen_get_monitor_geometry(gdk_screen_get_default(), monitor, &priv->geometry);

    char *title = g_strdup_printf ("metacity background %d", monitor);
    gtk_window_set_title(GTK_WINDOW(widget), title);
    free(title);

    gtk_window_move(GTK_WINDOW(widget), priv->geometry.x, priv->geometry.y);
    gtk_window_set_default_size(GTK_WINDOW(widget), priv->geometry.width, priv->geometry.height);

    gtk_window_set_keep_below(GTK_WINDOW(widget), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(widget), GDK_WINDOW_TYPE_HINT_DESKTOP);

    g_object_connect(G_OBJECT(deepin_message_hub_get()),
            "signal::desktop-changed", on_desktop_changed, widget,
            "signal::workspace-switched", on_workspace_switched, widget,
            NULL);
    return self;
}

