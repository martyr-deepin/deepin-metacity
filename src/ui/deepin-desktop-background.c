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
#include "deepin-background-cache.h"
#include "../core/workspace.h"
#include "../core/screen-private.h"
#include "deepin-message-hub.h"

struct _DeepinDesktopBackgroundPrivate
{
    MetaScreen* screen;
    GdkRectangle geometry;
    gint monitor;
    MetaWorkspace* active_workspace;
};


G_DEFINE_TYPE (DeepinDesktopBackground, deepin_desktop_background, GTK_TYPE_WINDOW);

static void deepin_desktop_background_init (DeepinDesktopBackground *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
            DEEPIN_TYPE_DESKTOP_BACKGROUND, DeepinDesktopBackgroundPrivate);
}

static void deepin_desktop_background_finalize (GObject *object)
{
    G_OBJECT_CLASS (deepin_desktop_background_parent_class)->finalize (object);
}

static gboolean deepin_desktop_background_real_draw(GtkWidget *widget, cairo_t* cr)
{
    DeepinDesktopBackground* self = DEEPIN_DESKTOP_BACKGROUND(widget);
    MetaScreen *screen = meta_get_display()->active_screen;

    if (self->priv->monitor >= gdk_screen_get_n_monitors(gdk_screen_get_default()))
        return FALSE;

    int index = meta_workspace_index(screen->active_workspace);
    cairo_surface_t* bg = deepin_background_cache_get_surface(self->priv->monitor, index, 1.0);
    if (bg) {
        cairo_set_source_surface(cr, bg, 0, 0);
        cairo_paint(cr);
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

static void on_screen_changed(DeepinMessageHub* hub, MetaScreen* screen,
        DeepinDesktopBackground* self)
{
    DeepinDesktopBackgroundPrivate* priv = self->priv;
    if (screen == priv->screen) {
        //FIXME: this means the monitor gets deleted, need to be handled by outsider
        if (priv->monitor >= gdk_screen_get_n_monitors(gdk_screen_get_default()))
            return;

        gdk_screen_get_monitor_geometry(gdk_screen_get_default(),
                priv->monitor, &priv->geometry);
        gdk_window_move_resize(gtk_widget_get_window(GTK_WIDGET(self)),
                priv->geometry.x, priv->geometry.y,
                priv->geometry.width, priv->geometry.height);
    }
}

static void on_desktop_changed(DeepinMessageHub* hub, DeepinDesktopBackground* self)
{
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

    gtk_window_move(GTK_WINDOW(widget), priv->geometry.x, priv->geometry.y);
    gtk_window_set_default_size(GTK_WINDOW(widget), priv->geometry.width, priv->geometry.height);

    gtk_window_set_keep_below(GTK_WINDOW(widget), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(widget), GDK_WINDOW_TYPE_HINT_DESKTOP);

    g_object_connect(G_OBJECT(deepin_message_hub_get()),
            "signal::desktop-changed", on_desktop_changed, widget,
            "signal::screen-changed", on_screen_changed, widget,
            "signal::workspace-switched", on_workspace_switched, widget,
            NULL);
    return self;
}

