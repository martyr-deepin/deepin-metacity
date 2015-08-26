/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-desktop-background.c
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * deepin metacity is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * gtk-skeleton is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "deepin-desktop-background.h"
#include <gdk/gdkx.h>
#include "deepin-background-cache.h"
#include "../core/workspace.h"
#include "../core/screen-private.h"
#include "deepin-message-hub.h"

struct _DeepinDesktopBackgroundPrivate
{
    MetaScreen* screen;
    MetaWorkspace* active_workspace;
};


G_DEFINE_TYPE (DeepinDesktopBackground, deepin_desktop_background, GTK_TYPE_WINDOW);

static void deepin_desktop_background_init (DeepinDesktopBackground *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
            DEEPIN_TYPE_DESKTOP_BACKGROUND, DeepinDesktopBackgroundPrivate);

    /* TODO: Add initialization code here */
}

static void deepin_desktop_background_finalize (GObject *object)
{
    /* TODO: Add deinitalization code here */

    G_OBJECT_CLASS (deepin_desktop_background_parent_class)->finalize (object);
}

/* FIXME: support fading effect when change wallpaper */
/* FIXME: change wallpaper according to workspace change */
static gboolean deepin_desktop_background_real_draw(GtkWidget *widget, cairo_t* cr)
{
    DeepinDesktopBackground* self = DEEPIN_DESKTOP_BACKGROUND(widget);

    cairo_set_source_surface(cr,
            deepin_background_cache_get_surface(1.0), 0, 0);
    cairo_paint(cr);
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
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

DeepinDesktopBackground* deepin_desktop_background_new(MetaScreen* screen)
{
    GtkWidget* widget = (GtkWidget*)g_object_new(DEEPIN_TYPE_DESKTOP_BACKGROUND,
            "type", GTK_WINDOW_TOPLEVEL, NULL);

    deepin_setup_style_class(widget, "deepin-window-manager"); 

    DeepinDesktopBackground* self = DEEPIN_DESKTOP_BACKGROUND(widget);
    self->priv->screen = screen;

    GdkDisplay* gdisplay = gdk_display_get_default();
    GdkScreen* gscreen = gdk_display_get_default_screen(gdisplay);

    GdkVisual* visual = gdk_screen_get_rgba_visual (gscreen);
    if (visual) gtk_widget_set_visual (widget, visual);

    gint w = screen->rect.width, h = screen->rect.height;
    gtk_window_set_position(GTK_WINDOW(widget), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_default_size(GTK_WINDOW(widget), w, h);

    gtk_window_set_keep_below(GTK_WINDOW(widget), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(widget), GDK_WINDOW_TYPE_HINT_DESKTOP);

    g_signal_connect(G_OBJECT(deepin_message_hub_get()),
            "desktop-changed", on_desktop_changed, widget);
    return self;
}

