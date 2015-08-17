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
#include <gdk/gdkx.h>
#include <prefs.h>
#include "../core/screen-private.h"
#include "../core/display-private.h"
#include "deepin-wm-background.h"
#include "deepin-design.h"
#include "deepin-shadow-workspace.h"
#include "deepin-fixed.h"

struct _DeepinWMBackgroundPrivate
{
    MetaScreen* screen;
    GdkScreen* gscreen;

    gint disposed: 1;
    GtkWidget* fixed;

    DeepinShadowWorkspace* active_workspace;
    GList* worskpaces;
    GList* worskpace_thumbs;
    /*DeepinShadowWorkspaceAdder* adder;*/
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

    gint monitor_index = gdk_screen_get_monitor_at_window(priv->gscreen,
            gtk_widget_get_window(GTK_WIDGET(self)));
    gdk_screen_get_monitor_geometry(priv->gscreen, monitor_index, &geom);
            
    priv->fixed = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(self), priv->fixed);

    int top_offset = (int)(geom.height * FLOW_CLONE_TOP_OFFSET_PERCENT);
    int bottom_offset = (int)(geom.height * HORIZONTAL_OFFSET_PERCENT);
    float scale = (float)(geom.height - top_offset - bottom_offset) / geom.height;

    gint width = geom.width * scale, height = geom.height * scale;

    // calculate monitor width height ratio
    float monitor_whr = (float)geom.height / geom.width;
    gint thumb_width = geom.width * WORKSPACE_WIDTH_PERCENT;
    gint thumb_height = width * monitor_whr;

    GList *l = priv->screen->workspaces;
    gint current = 0;

    while (l) {
        MetaWorkspace* ws = (MetaWorkspace*)l->data;
        {
            DeepinShadowWorkspace* dsw = 
                (DeepinShadowWorkspace*)deepin_shadow_workspace_new();
            deepin_shadow_workspace_set_scale(dsw, scale);
            deepin_shadow_workspace_populate(dsw, ws);

            if (ws == priv->screen->active_workspace) {
                current = g_list_index(priv->screen->workspaces, l->data);
                priv->active_workspace = dsw;
                deepin_shadow_workspace_set_presentation(dsw, TRUE);
                deepin_shadow_workspace_set_current(dsw, TRUE);
            }

            priv->worskpaces = g_list_append(priv->worskpaces, dsw);
        }

        {
            DeepinShadowWorkspace* dsw = 
                (DeepinShadowWorkspace*)deepin_shadow_workspace_new();
            deepin_shadow_workspace_set_thumb_mode(dsw, TRUE);
            deepin_shadow_workspace_set_scale(dsw, WORKSPACE_WIDTH_PERCENT);
            deepin_shadow_workspace_populate(dsw, ws);

            if (ws == priv->screen->active_workspace) {
                deepin_shadow_workspace_set_current(dsw, TRUE);
            }

            priv->worskpace_thumbs = g_list_append(priv->worskpace_thumbs, dsw);
        }

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

    i = 0;
    l = priv->worskpace_thumbs;
    int thumb_spacing = geom.width * SPACING_PERCENT;
    
    gint count = g_list_length(priv->worskpace_thumbs);
    int thumb_y = (int)(geom.height * HORIZONTAL_OFFSET_PERCENT);
    int thumb_x = (geom.width - count * (thumb_width + thumb_spacing))/2;

    while (l) {
        int x = thumb_x + i * (thumb_width + thumb_spacing);
        gtk_fixed_put(GTK_FIXED(priv->fixed), (GtkWidget*)l->data,
                x, thumb_y);

        i++;
        l = l->next;
    }
}

GtkWidget* deepin_wm_background_new(MetaScreen* screen)
{
    GtkWidget* widget = (GtkWidget*)g_object_new(DEEPIN_TYPE_WM_BACKGROUND,
            "type", GTK_WINDOW_POPUP, NULL);
    deepin_setup_style_class(widget, "deepin-window-manager"); 

    DeepinWMBackground* self = DEEPIN_WM_BACKGROUND(widget);

    MetaDisplay* display = meta_get_display();
    GdkDisplay* gdisplay = gdk_x11_lookup_xdisplay(display->xdisplay);
    self->priv->gscreen = gdk_display_get_default_screen(gdisplay);

    GdkVisual* visual = gdk_screen_get_rgba_visual (self->priv->gscreen);
    if (visual) gtk_widget_set_visual (widget, visual);

    gint w = screen->rect.width, h = screen->rect.height;
    gtk_window_set_position(GTK_WINDOW(widget), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_default_size(GTK_WINDOW(widget), w, h);
    gtk_widget_realize (widget);

    self->priv->screen = screen;
    deepin_wm_background_setup(self);
    return widget;
}

void deepin_wm_background_handle_event(DeepinWMBackground* self, XEvent* event, 
        KeySym keysym, MetaKeyBindingAction action)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    gboolean backward = FALSE;
    if (keysym == XK_Tab
            || action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS
            || action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD) {
        g_message("tabbing inside preview workspace");
        if (keysym == XK_Tab)
            backward = event->xkey.state & ShiftMask;
        else
            backward = action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD;
        deepin_shadow_workspace_focus_next(priv->active_workspace, backward);
    }
}

