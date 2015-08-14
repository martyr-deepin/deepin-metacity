/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-wm-background.c
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * gtk-skeleton is free software: you can redistribute it and/or modify it
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

#include <config.h>
#include "../core/screen-private.h"
#include "../core/display-private.h"
#include "deepin-wm-background.h"
#include "deepin-design.h"
#include "deepin-shadow-workspace.h"
#include "deepin-fixed.h"

struct _DeepinWMBackgroundPrivate
{
    gint disposed: 1;
    GtkWidget* fixed;

    DeepinShadowWorkspace* active_workspace;
    GList* worskpaces;
};


G_DEFINE_TYPE (DeepinWMBackground, deepin_wm_background, GTK_TYPE_WINDOW);

static void deepin_wm_background_init (DeepinWMBackground *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_WM_BACKGROUND, DeepinWMBackgroundPrivate);

    DeepinWMBackgroundPrivate* priv = self->priv;
    priv->worskpaces = NULL;
}

static void deepin_wm_background_finalize (GObject *object)
{
    /* TODO: Add deinitalization code here */

    G_OBJECT_CLASS (deepin_wm_background_parent_class)->finalize (object);
}

static gboolean deepin_wm_background_real_draw(GtkWidget *widget, cairo_t* cr)
{
    return GTK_WIDGET_CLASS(deepin_wm_background_parent_class)->draw(widget, cr);
}

static void deepin_wm_background_class_init (DeepinWMBackgroundClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS (klass);

    g_type_class_add_private (klass, sizeof (DeepinWMBackgroundPrivate));
    widget_class->draw = deepin_wm_background_real_draw;

    object_class->finalize = deepin_wm_background_finalize;
}

static void deepin_wm_background_setup(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    GdkRectangle geom;
    GdkScreen* screen = gdk_screen_get_default();
    gint monitor_index = gdk_screen_get_monitor_at_window(screen,
            gtk_widget_get_window(GTK_WIDGET(self)));
    gdk_screen_get_monitor_geometry(screen, monitor_index, &geom);
            
    priv->fixed = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(self), priv->fixed);

    MetaDisplay* display = meta_get_display();

    int top_offset = (int)(geom.height * FLOW_CLONE_TOP_OFFSET_PERCENT);
    int bottom_offset = (int)(geom.height * HORIZONTAL_OFFSET_PERCENT);
    float scale = (float)(geom.height - top_offset - bottom_offset) / geom.height;

    gint width = geom.width * scale, height = geom.height * scale;

    GList *l = display->active_screen->workspaces;
    gint current = 0;

    while (l) {
        MetaWorkspace* ws = (MetaWorkspace*)l->data;
        DeepinShadowWorkspace* dsw = (DeepinShadowWorkspace*)deepin_shadow_workspace_new();
        deepin_shadow_workspace_set_scale(dsw, scale);

        deepin_shadow_workspace_populate(dsw, ws);

        if (ws == display->active_screen->active_workspace) {
            current = g_list_index(display->active_screen->workspaces, l->data);
            priv->active_workspace = dsw;
            deepin_shadow_workspace_set_presentation(dsw, TRUE);
            deepin_shadow_workspace_set_current(dsw, TRUE);
        }
        
        priv->worskpaces = g_list_append(priv->worskpaces, dsw);

        l = l->next;
    }

    gint i = 0, pad = FLOW_CLONE_DISTANCE_PERCENT * geom.width;
    l = priv->worskpaces;
    while (l) {
        gint x = (geom.width - width) / 2 +  (i - current) * (width + pad);
        gtk_fixed_put(GTK_FIXED(priv->fixed), (GtkWidget*)l->data,
                x, top_offset);

        i++;
        l = l->next;
    }
}

GtkWidget* deepin_wm_background_new(void)
{
    GtkWidget* widget = (GtkWidget*)g_object_new(DEEPIN_TYPE_WM_BACKGROUND,
            "type", GTK_WINDOW_POPUP, NULL);
    deepin_setup_style_class(widget, "deepin-window-manager"); 

    GdkScreen* screen = gdk_screen_get_default();
    gint w = gdk_screen_get_width(screen), h = gdk_screen_get_height(screen);

    GdkVisual* visual = gdk_screen_get_rgba_visual (screen);
    if (visual) gtk_widget_set_visual (widget, visual);

    gtk_window_set_position(GTK_WINDOW(widget), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_default_size(GTK_WINDOW(widget), w, h);
    gtk_widget_realize (widget);

    deepin_wm_background_setup(DEEPIN_WM_BACKGROUND(widget));
    return widget;
}

