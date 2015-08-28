/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-workspace-adder.c
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

#include <config.h>
#include <math.h>

#include "deepin-workspace-adder.h"
#include "deepin-design.h"

struct _DeepinWorkspaceAdderPrivate
{
    guint disposed: 1;
};




G_DEFINE_TYPE (DeepinWorkspaceAdder, deepin_workspace_adder, GTK_TYPE_EVENT_BOX);

static void deepin_workspace_adder_init (DeepinWorkspaceAdder *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_WORKSPACE_ADDER, DeepinWorkspaceAdderPrivate);

	/* TODO: Add initialization code here */
}

static void deepin_workspace_adder_finalize (GObject *object)
{
	/* TODO: Add deinitalization code here */

	G_OBJECT_CLASS (deepin_workspace_adder_parent_class)->finalize (object);
}

static void _draw_round_box(cairo_t* cr, gint width, gint height, double radius)
{
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    double xc = radius, yc = radius;
    double angle1 = 180.0  * (M_PI/180.0);  /* angles are specified */
    double angle2 = 270.0 * (M_PI/180.0);  /* in radians           */

    cairo_arc (cr, xc, yc, radius, angle1, angle2);

    xc = width - radius;
    angle1 = 270.0 * (M_PI/180.0);
    angle2 = 360.0 * (M_PI/180.0);
    cairo_arc (cr, xc, yc, radius, angle1, angle2);

    yc = height - radius;
    angle1 = 0.0 * (M_PI/180.0);
    angle2 = 90.0 * (M_PI/180.0);
    cairo_arc (cr, xc, yc, radius, angle1, angle2);

    xc = radius;
    angle1 = 90.0 * (M_PI/180.0);
    angle2 = 180.0 * (M_PI/180.0);
    cairo_arc (cr, xc, yc, radius, angle1, angle2);

    cairo_close_path(cr);
}

static const double PLUS_SIZE = 32.0;
static const double PLUS_LINE_WIDTH = 2.0;

static gboolean deepin_workspace_adder_draw(GtkWidget *widget,
        cairo_t *cr)
{
    GtkAllocation req;
    gtk_widget_get_allocation(widget, &req);

    GtkStyleContext* context = gtk_widget_get_style_context(widget);

    gtk_render_background(context, cr, 0, 0, req.width, req.height);
    /*int b = 2;*/
    /*gtk_render_background(context, cr, -b, -b, req.width+2*b, */
            /*req.height+2*b);*/

    /*_draw_round_box(cr, req.width, req.height, 4.0);*/
    /*cairo_clip(cr);*/

    // draw tha plus button
    cairo_move_to(cr, req.width / 2 - PLUS_SIZE / 2, req.height / 2);
    cairo_line_to(cr, req.width / 2 + PLUS_SIZE / 2, req.height / 2);

    cairo_move_to(cr, req.width / 2, req.height / 2 - PLUS_SIZE / 2);
    cairo_line_to(cr, req.width / 2, req.height / 2 + PLUS_SIZE / 2);

    cairo_set_line_width(cr, PLUS_LINE_WIDTH);
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 1.0);
    cairo_stroke(cr);
    return FALSE;
}

static void deepin_workspace_adder_class_init (DeepinWorkspaceAdderClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass* widget_class = (GtkWidgetClass*) klass;

	g_type_class_add_private (klass, sizeof (DeepinWorkspaceAdderPrivate));

	object_class->finalize = deepin_workspace_adder_finalize;

    widget_class->draw = deepin_workspace_adder_draw;
}

GtkWidget* deepin_workspace_adder_new()
{
    GtkWidget* w = (GtkWidget*)g_object_new(DEEPIN_TYPE_WORKSPACE_ADDER, NULL);
    
    deepin_setup_style_class(w, "deepin-workspace-add-button"); 
    return w;
}

