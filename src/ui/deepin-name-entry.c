/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#include "deepin-name-entry.h"
#include "deepin-design.h"

struct _DeepinNameEntryPrivate
{
    gint disposed: 1;
    int recommend_width;
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

static void _gtk_entry_get_borders (GtkEntry *entry,
                        GtkBorder *border_out)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkBorder padding, border;
  GtkStyleContext *context;
  GtkStateFlags state;

  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);
  gtk_style_context_get_padding (context, state, &padding);
  gtk_style_context_get_border (context, state, &border);

  border_out->top = padding.top + border.top;
  border_out->bottom = padding.bottom + border.bottom;
  border_out->left = padding.left + border.left;
  border_out->right = padding.right + border.right;
}

static void deepin_name_entry_get_preferred_width (GtkWidget *widget,
        gint *minimum, gint *natural)
{
	GTK_WIDGET_CLASS (deepin_name_entry_parent_class)->get_preferred_width(
            widget, minimum, natural);

    DeepinNameEntry* self = DEEPIN_NAME_ENTRY(widget);
    GtkEntry *entry = GTK_ENTRY (widget);
    GtkBorder border;
    PangoFontMetrics *metrics;
    PangoContext *context;
    gint min, nat;
    gint char_width;
    gint digit_width;
    gint char_pixels;
    gint width_chars;

    _gtk_entry_get_borders(entry, &border);

    context = gtk_widget_get_pango_context (widget);
    metrics = pango_context_get_metrics (context,
            pango_context_get_font_description (context),
            pango_context_get_language (context));

    char_width = pango_font_metrics_get_approximate_char_width (metrics);
    digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
    char_pixels = (MAX (char_width, digit_width) + PANGO_SCALE - 1) / PANGO_SCALE;

    pango_font_metrics_unref (metrics);

    width_chars = gtk_entry_buffer_get_bytes(gtk_entry_get_buffer(entry));
    if (width_chars == 0)
        min = gtk_widget_has_focus(widget) ? char_pixels: 0;
    else {
        min = border.left + border.right + 
            MAX(MIN(char_pixels * width_chars, self->priv->recommend_width - 26), char_pixels);
    }
    nat = min;

    *minimum = min;
    *natural = nat;
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
    widget_class->draw = deepin_name_entry_draw;
}

GtkWidget* deepin_name_entry_new(int recommend_width)
{
    GtkWidget* w = (GtkWidget*)g_object_new(DEEPIN_TYPE_NAME_ENTRY, NULL);
    gtk_entry_set_alignment(GTK_ENTRY(w), 0.5);
    gtk_entry_set_has_frame(GTK_ENTRY(w), FALSE);

    DeepinNameEntry* self = DEEPIN_NAME_ENTRY(w);
    self->priv->recommend_width = recommend_width;
    deepin_setup_style_class(w, "deepin-workspace-thumb-clone-name");
    return w;
}

