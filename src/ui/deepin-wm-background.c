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

#include "deepin-wm-background.h"
#include "deepin-design.h"

struct _DeepinWMBackgroundPrivate
{
  gint disposed: 1;
};




G_DEFINE_TYPE (DeepinWMBackground, deepin_wm_background, GTK_TYPE_WINDOW);

static void
deepin_wm_background_init (DeepinWMBackground *deepin_wm_background)
{
	deepin_wm_background->priv = G_TYPE_INSTANCE_GET_PRIVATE (deepin_wm_background, DEEPIN_TYPE_WM_BACKGROUND, DeepinWMBackgroundPrivate);

	/* TODO: Add initialization code here */
}

static void
deepin_wm_background_finalize (GObject *object)
{
	/* TODO: Add deinitalization code here */

	G_OBJECT_CLASS (deepin_wm_background_parent_class)->finalize (object);
}
    
static gboolean deepin_wm_background_real_draw(GtkWidget *widget, cairo_t* cr)
{
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    cairo_rectangle(cr, 100, 100, 100, 100);
    cairo_fill(cr);

    return GTK_WIDGET_CLASS(deepin_wm_background_parent_class)->draw(widget, cr);
}

static void
deepin_wm_background_class_init (DeepinWMBackgroundClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DeepinWMBackgroundPrivate));
    widget_class->draw = deepin_wm_background_real_draw;

	object_class->finalize = deepin_wm_background_finalize;
}

GtkWidget* deepin_wm_background_new(void)
{
    GtkWidget* self = (GtkWidget*)g_object_new(DEEPIN_TYPE_WM_BACKGROUND,
            "type", GTK_WINDOW_POPUP, NULL);
    deepin_setup_style_class(self, "deepin-window-manager"); 

    GdkScreen* screen =gdk_screen_get_default();
    gint w = gdk_screen_get_width(screen), h = gdk_screen_get_height(screen);
    GdkVisual* visual = gdk_screen_get_rgba_visual (screen);
    if (visual) gtk_widget_set_visual (self, visual);
    gtk_window_set_position(GTK_WINDOW(self), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_default_size(GTK_WINDOW(self), w, h);
    gtk_widget_realize (self);

    return self;
}


