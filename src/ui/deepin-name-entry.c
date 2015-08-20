/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-name-entry.c
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

#include "deepin-name-entry.h"
#include "deepin-design.h"

struct _DeepinNameEntryPrivate
{
    gint disposed: 1;
};




G_DEFINE_TYPE (DeepinNameEntry, deepin_name_entry, GTK_TYPE_ENTRY);

static void deepin_name_entry_init (DeepinNameEntry *deepin_name_entry)
{
	deepin_name_entry->priv = G_TYPE_INSTANCE_GET_PRIVATE (deepin_name_entry, DEEPIN_TYPE_NAME_ENTRY, DeepinNameEntryPrivate);

	/* TODO: Add initialization code here */
}

static void deepin_name_entry_finalize (GObject *object)
{
	/* TODO: Add deinitalization code here */

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

static void deepin_name_entry_class_init (DeepinNameEntryClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass* widget_class = (GtkWidgetClass*) klass;

	g_type_class_add_private (klass, sizeof (DeepinNameEntryPrivate));

	object_class->finalize = deepin_name_entry_finalize;

    widget_class->get_preferred_width = deepin_name_entry_get_preferred_width;
    widget_class->get_preferred_height = deepin_name_entry_get_preferred_height;
    /*widget_class->size_allocate = deepin_name_entry_size_allocate;*/
    /*widget_class->draw = deepin_name_entry_draw;*/
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

