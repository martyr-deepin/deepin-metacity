/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#include <config.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <util.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <cairo-xlib.h>

#include "../core/workspace.h"
#include "boxes.h"
#include "deepin-design.h"
#include "deepin-workspace-preview-entry.h"
#include "deepin-message-hub.h"
#include "deepin-background-cache.h"

#define SET_STATE(w, state)  \
    gtk_style_context_set_state(gtk_widget_get_style_context(GTK_WIDGET(w)), (state))

struct _DeepinWorkspacePreviewEntryPrivate
{
    gint disposed: 1;
    gint selected: 1; 

    int fixed_width, fixed_height;

    MetaWorkspace* workspace;
    cairo_surface_t *background;
};

G_DEFINE_TYPE (DeepinWorkspacePreviewEntry, deepin_workspace_preview_entry, DEEPIN_TYPE_FIXED);

static void deepin_workspace_preview_entry_get_preferred_width (GtkWidget *widget,
        gint *minimum, gint *natural)
{
    DeepinWorkspacePreviewEntry *self = DEEPIN_WORKSPACE_PREVIEW_ENTRY (widget);

    *minimum = *natural = self->priv->fixed_width;
}

static void deepin_workspace_preview_entry_get_preferred_height (GtkWidget *widget,
        gint *minimum, gint *natural)
{
    DeepinWorkspacePreviewEntry *self = DEEPIN_WORKSPACE_PREVIEW_ENTRY (widget);

    *minimum = *natural = self->priv->fixed_height;
}

static void deepin_workspace_preview_entry_init (DeepinWorkspacePreviewEntry *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_WORKSPACE_PREVIEW_ENTRY, DeepinWorkspacePreviewEntryPrivate);
    memset(self->priv, 0, sizeof *self->priv);

    gtk_widget_set_sensitive(GTK_WIDGET(self), TRUE);
    gtk_widget_set_app_paintable(GTK_WIDGET(self), TRUE);
    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);

    GdkScreen *screen = gdk_screen_get_default();
    GdkRectangle monitor_geom;
    gint primary = gdk_screen_get_primary_monitor(screen);
    gdk_screen_get_monitor_geometry(screen, primary, &monitor_geom);

    self->priv->fixed_width = monitor_geom.width * DWI_WORKSPACE_SCALE;
    self->priv->fixed_height  = monitor_geom.height * DWI_WORKSPACE_SCALE;
}

static void on_desktop_changed(DeepinMessageHub* hub, gpointer data)
{
    DeepinWorkspacePreviewEntryPrivate* priv = DEEPIN_WORKSPACE_PREVIEW_ENTRY(data)->priv;

    if (priv->background) {
        g_clear_pointer(&priv->background, cairo_surface_destroy);
    }

    priv->background = deepin_background_cache_get_surface(
            0, meta_workspace_index(priv->workspace), DWI_WORKSPACE_SCALE * 0.96);
    cairo_surface_reference(priv->background);
    
    gtk_widget_queue_draw(GTK_WIDGET(data));
}

static void deepin_workspace_preview_entry_dispose (GObject *object)
{
    DeepinWorkspacePreviewEntry* self = DEEPIN_WORKSPACE_PREVIEW_ENTRY(object);
    DeepinWorkspacePreviewEntryPrivate* priv = self->priv;

    if (priv->disposed) return;
    priv->disposed = TRUE;

    g_object_disconnect(G_OBJECT(deepin_message_hub_get()), 
            "signal::desktop-changed", on_desktop_changed, self,
            NULL);

    if (priv->background) {
        g_clear_pointer(&priv->background, cairo_surface_destroy);
    }

    G_OBJECT_CLASS (deepin_workspace_preview_entry_parent_class)->dispose (object);
}

static void _style_get_borders (GtkStyleContext *context, GtkBorder *border_out)
{
    GtkBorder padding, border;
    GtkStateFlags state;

    state = gtk_style_context_get_state (context);
    gtk_style_context_get_padding (context, state, &padding);
    gtk_style_context_get_border (context, state, &border);

    border_out->top = padding.top + border.top;
    border_out->bottom = padding.bottom + border.bottom;
    border_out->left = padding.left + border.left;
    border_out->right = padding.right + border.right;
}

static gboolean deepin_workspace_preview_entry_draw (GtkWidget *widget,
        cairo_t *cr)
{
    DeepinWorkspacePreviewEntry *self = DEEPIN_WORKSPACE_PREVIEW_ENTRY (widget);
    DeepinWorkspacePreviewEntryPrivate *priv = self->priv;

    GtkRequisition req;
    GtkBorder borders;
    GtkStyleContext* context = gtk_widget_get_style_context (widget);

    gtk_widget_get_preferred_size(widget, &req, NULL);
    gdouble x = 0, y = 0, w = req.width, h = req.height;

    _style_get_borders(context, &borders);
    gdouble w2 = w * 0.5, h2 = h * 0.5;
    cairo_translate(cr, w2, h2);

    x = w/2, y = h/2;
    gtk_render_background(context, cr, -x, -y, w, h);

    x += borders.left;
    y += borders.top;
    gdouble fw = w + borders.left + borders.right;
    gdouble fh = h + borders.top + borders.bottom;
    gtk_render_frame(context, cr, -x, -y, fw, fh);

    if (priv->background != NULL) {
        x = cairo_image_surface_get_width(priv->background) / 2.0,
          y = cairo_image_surface_get_height(priv->background) / 2.0;
        cairo_set_source_surface(cr, priv->background, -x, -y);
        cairo_paint(cr);
    }

    return TRUE;
}

static void deepin_workspace_preview_entry_class_init (DeepinWorkspacePreviewEntryClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass* widget_class = (GtkWidgetClass*) klass;

    g_type_class_add_private (klass, sizeof (DeepinWorkspacePreviewEntryPrivate));
    widget_class->get_preferred_width = deepin_workspace_preview_entry_get_preferred_width;
    widget_class->get_preferred_height = deepin_workspace_preview_entry_get_preferred_height;
    widget_class->draw = deepin_workspace_preview_entry_draw;
    object_class->dispose = deepin_workspace_preview_entry_dispose;
}

GtkWidget* deepin_workspace_preview_entry_new(MetaWorkspace *ws)
{
    GtkWidget *widget = (GtkWidget*)g_object_new(DEEPIN_TYPE_WORKSPACE_PREVIEW_ENTRY, NULL);
    DeepinWorkspacePreviewEntry *self = DEEPIN_WORKSPACE_PREVIEW_ENTRY (widget);
    self->priv->workspace = ws;

    SET_STATE (self, GTK_STATE_FLAG_NORMAL);

    g_object_connect(G_OBJECT(deepin_message_hub_get()), 
            "signal::desktop-changed", on_desktop_changed, self,
            NULL);
    on_desktop_changed(deepin_message_hub_get(), self);

    return widget;
}

void deepin_workspace_preview_entry_set_select(DeepinWorkspacePreviewEntry* self, gboolean val)
{
    if (self->priv->selected != val) {
        self->priv->selected = val;
        GtkStateFlags state = self->priv->selected? GTK_STATE_FLAG_SELECTED: GTK_STATE_FLAG_NORMAL;

        SET_STATE (self, state);
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
}

MetaWorkspace* deepin_workspace_preview_entry_get_workspace(DeepinWorkspacePreviewEntry* self)
{
    return self->priv->workspace;
}

