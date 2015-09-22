/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-name-entry.c
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * deepin metacity is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * deepin metacity is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "deepin-name-entry.h"
#include "deepin-design.h"

struct _DeepinNameEntryPrivate
{
    gint disposed: 1;
};

G_DEFINE_TYPE (DeepinNameEntry, deepin_name_entry, GTK_TYPE_ENTRY);

static void deepin_name_entry_init (DeepinNameEntry *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_NAME_ENTRY,
            DeepinNameEntryPrivate);

}

static void deepin_name_entry_finalize (GObject *object)
{
	G_OBJECT_CLASS (deepin_name_entry_parent_class)->finalize (object);
}

static void deepin_name_entry_get_preferred_width (GtkWidget *widget,
        gint *minimum, gint *natural)
{
    *minimum = *natural = WORKSPACE_NAME_WIDTH;
}

static void deepin_name_entry_get_preferred_height (GtkWidget *widget,
        gint *minimum, gint *natural)
{
    *minimum = *natural = WORKSPACE_NAME_HEIGHT + 2*NAME_SHAPE_PADDING;
}

static gboolean deepin_name_entry_draw(GtkWidget *widget, cairo_t *cr)
{
    DeepinNameEntry* entry = DEEPIN_NAME_ENTRY(widget);

    GtkStateFlags state;
    GdkRGBA text_color;
    GtkStyleContext *context;

    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);

    state = gtk_widget_get_state_flags (widget);
    context = gtk_widget_get_style_context (widget);
    gtk_style_context_get_color(context, state, &text_color);

    if (!gtk_widget_has_focus(widget)) {
        gtk_render_background(context, cr, 0, 0, alloc.width, alloc.height);

        GdkRectangle text_area;
        gtk_entry_get_text_area(GTK_ENTRY(widget), &text_area);

        PangoLayout* layout = pango_layout_copy(
                gtk_entry_get_layout(GTK_ENTRY(entry)));
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
        pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
        pango_layout_set_wrap(layout, PANGO_WRAP_CHAR);
        pango_layout_set_width(layout, text_area.width * PANGO_SCALE);

        cairo_move_to(cr, text_area.x, text_area.y);
        gdk_cairo_set_source_rgba(cr, &text_color);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
        return TRUE;
    }

    return GTK_WIDGET_CLASS(deepin_name_entry_parent_class)->draw(widget, cr);
}

static void deepin_name_entry_class_init (DeepinNameEntryClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass* widget_class = (GtkWidgetClass*) klass;

	g_type_class_add_private (klass, sizeof (DeepinNameEntryPrivate));

	object_class->finalize = deepin_name_entry_finalize;

    widget_class->get_preferred_width = deepin_name_entry_get_preferred_width;
    /*widget_class->get_preferred_height = deepin_name_entry_get_preferred_height;*/
    widget_class->draw = deepin_name_entry_draw;
}

GtkWidget* deepin_name_entry_new()
{
    GtkWidget* w = (GtkWidget*)g_object_new(DEEPIN_TYPE_NAME_ENTRY, NULL);
    gtk_entry_set_max_length(GTK_ENTRY(w), WORKSPACE_NAME_MAX_LENGTH);
    gtk_entry_set_width_chars(GTK_ENTRY(w), 6);
    gtk_entry_set_alignment(GTK_ENTRY(w), 0.5);
    gtk_entry_set_has_frame(GTK_ENTRY(w), FALSE);

    deepin_setup_style_class(w, "deepin-workspace-thumb-clone-name");
    return w;
}

