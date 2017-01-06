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
#include <util.h>
#include <stdlib.h>
#include <math.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <prefs.h>
#include "../core/screen-private.h"
#include "../core/display-private.h"
#include "../core/workspace.h"
#include "deepin-design.h"
#include "deepin-message-hub.h"
#include "deepin-workspace-indicator.h"
#include "deepin-workspace-preview-entry.h"
#include "deepin-fixed.h"

//default
static int DWI_POPUP_TIMEOUT = 2000;

struct _DeepinWorkspaceIndicatorPrivate
{
    gint disposed: 1;
    guint timeout_id;

    MetaScreen *screen;

    GtkWidget *fixed;

    int child_spacing;
    int child_width;
    int child_height;

    DeepinWorkspacePreviewEntry *active_entry;
    GList *workspaces;
};

G_DEFINE_TYPE (DeepinWorkspaceIndicator, deepin_workspace_indicator, GTK_TYPE_WINDOW);

static void deepin_workspace_indicator_init (DeepinWorkspaceIndicator *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_WORKSPACE_INDICATOR, DeepinWorkspaceIndicatorPrivate);

    DeepinWorkspaceIndicatorPrivate* priv = self->priv;
    priv->workspaces = NULL;
}

static void deepin_workspace_indicator_dispose (GObject *object)
{
    DeepinWorkspaceIndicator* self = DEEPIN_WORKSPACE_INDICATOR(object);
    DeepinWorkspaceIndicatorPrivate* priv = self->priv;

    if (!priv->disposed) {
        priv->disposed = TRUE;

        if (priv->timeout_id) {
            g_source_remove(priv->timeout_id);
            priv->timeout_id = 0;
        }

        if (priv->workspaces) {
            g_list_free(priv->workspaces);
        }
    }

    G_OBJECT_CLASS (deepin_workspace_indicator_parent_class)->dispose (object);
}

static void deepin_workspace_indicator_finalize (GObject *object)
{
    /* TODO: Add deinitalization code here */

    G_OBJECT_CLASS (deepin_workspace_indicator_parent_class)->finalize (object);
}

static gboolean deepin_workspace_indicator_real_draw(GtkWidget *widget, cairo_t* cr)
{
    DeepinWorkspaceIndicatorPrivate* priv = DEEPIN_WORKSPACE_INDICATOR(widget)->priv;

    return GTK_WIDGET_CLASS(deepin_workspace_indicator_parent_class)->draw(widget, cr);
}

static void deepin_workspace_indicator_class_init (DeepinWorkspaceIndicatorClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS (klass);

    g_type_class_add_private (klass, sizeof (DeepinWorkspaceIndicatorPrivate));
    widget_class->draw = deepin_workspace_indicator_real_draw;

    object_class->finalize = deepin_workspace_indicator_finalize;
    object_class->dispose = deepin_workspace_indicator_dispose;
}

static void deepin_workspace_indicator_setup_style(DeepinWorkspaceIndicator *dwi)
{
    deepin_setup_style_class(dwi, "deepin-window-switcher");

    GList *l = dwi->priv->workspaces;
    while (l) {
        DeepinWorkspacePreviewEntry *dwpe = DEEPIN_WORKSPACE_PREVIEW_ENTRY(l->data);
        deepin_setup_style_class(GTK_WIDGET(dwpe), "deepin-workspace-thumb-clone");
        l = l->next;
    }
}

static void relayout(DeepinWorkspaceIndicator *self)
{
    DeepinWorkspaceIndicatorPrivate *priv = self->priv;

    GdkRectangle mon_geom;
    gint primary = gdk_screen_get_primary_monitor(gdk_screen_get_default());
    gdk_screen_get_monitor_geometry(gdk_screen_get_default(), primary, &mon_geom);

    int n = meta_screen_get_n_workspaces (priv->screen);
    int width = (priv->child_width + priv->child_spacing) * n + DWI_MARGIN_HORIZONTAL * 2 - priv->child_spacing;
    int height = priv->child_height + 2 * DWI_MARGIN_VERTICAL; 
    gtk_widget_set_size_request(priv->fixed, width, height);
    gtk_window_resize(GTK_WINDOW(self), width, height);
    gtk_window_move(GTK_WINDOW(self), 
            mon_geom.x + (mon_geom.width - width)/2,
            mon_geom.y + (mon_geom.height - height)/2);

    int index = 0;
    GList *l = priv->workspaces;
    while (l) {
        gtk_fixed_move(GTK_FIXED(priv->fixed), (GtkWidget*)l->data, 
                (priv->child_width + priv->child_spacing) * index + DWI_MARGIN_HORIZONTAL, DWI_MARGIN_VERTICAL);
        l = l->next;
        index++;
    }

    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void on_workspace_added(DeepinMessageHub* hub, gint index,
        DeepinWorkspaceIndicator* self)
{
    DeepinWorkspaceIndicatorPrivate *priv = self->priv;

    MetaWorkspace *ws = g_list_nth_data(priv->screen->workspaces, index);
    DeepinWorkspacePreviewEntry *dwpe = deepin_workspace_preview_entry_new(ws);
    deepin_setup_style_class(GTK_WIDGET(dwpe), "deepin-workspace-thumb-clone");

    if (ws == priv->screen->active_workspace) {
        if (priv->active_entry) {
            deepin_workspace_preview_entry_set_select(priv->active_entry, FALSE);
            priv->active_entry = NULL;
        }
    }
    gtk_fixed_put(GTK_FIXED(priv->fixed), GTK_WIDGET(dwpe),
            (priv->child_width + priv->child_spacing) * index + DWI_MARGIN_HORIZONTAL, DWI_MARGIN_VERTICAL);

    priv->workspaces = g_list_append(priv->workspaces, dwpe);
    relayout(self);
}

static void on_workspace_removed(DeepinMessageHub* hub, gint index,
        DeepinWorkspaceIndicator* self)
{
    DeepinWorkspaceIndicatorPrivate *priv = self->priv;
    GList *l = g_list_nth(priv->workspaces, index);
    priv->workspaces = g_list_remove_link(priv->workspaces, l);

    DeepinWorkspacePreviewEntry *dwpe = DEEPIN_WORKSPACE_PREVIEW_ENTRY(l->data);
    if (priv->active_entry == dwpe) priv->active_entry = NULL;
    gtk_container_remove(GTK_CONTAINER(priv->fixed), GTK_WIDGET(dwpe));
    g_list_free(l);

    if (priv->active_entry == dwpe) {
        priv->active_entry = dwpe;
    }


    relayout(self);
}

static void on_workspace_reordered(DeepinMessageHub* hub, gint index, 
        int new_index, DeepinWorkspaceIndicator* self)
{
    DeepinWorkspaceIndicatorPrivate *priv = self->priv;

    DeepinWorkspacePreviewEntry *dwpe_from = g_list_nth_data(priv->workspaces, index);

    priv->workspaces = g_list_remove(priv->workspaces, dwpe_from);
    priv->workspaces = g_list_insert(priv->workspaces, dwpe_from, new_index);

    relayout(self);
}

GtkWidget* deepin_workspace_indicator_new(MetaScreen* screen)
{
    GtkWidget* dwi = (GtkWidget*)g_object_new(DEEPIN_TYPE_WORKSPACE_INDICATOR,
            "type", GTK_WINDOW_POPUP, NULL);

    DeepinWorkspaceIndicator* self = DEEPIN_WORKSPACE_INDICATOR(dwi);
    DeepinWorkspaceIndicatorPrivate *priv = self->priv;

    gtk_window_set_keep_above(GTK_WINDOW(dwi), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(dwi), FALSE);
    gtk_window_set_resizable (GTK_WINDOW (dwi), TRUE);

    GdkScreen* gdkscreen = gdk_screen_get_default();
    GdkRectangle monitor_geom;
    gint primary = gdk_screen_get_primary_monitor(gdkscreen);
    gdk_screen_get_monitor_geometry(gdkscreen, primary, &monitor_geom);

    priv->child_spacing = monitor_geom.width * DWI_SPACING_PERCENT;
    priv->child_width = monitor_geom.width * DWI_WORKSPACE_SCALE;
    priv->child_height  = monitor_geom.height * DWI_WORKSPACE_SCALE;

    GdkVisual* visual = gdk_screen_get_rgba_visual (gdkscreen);
    if (visual)
        gtk_widget_set_visual (dwi, visual);

    MetaDisplay* display = meta_get_display();
    priv->screen = display->active_screen;
    int n = meta_screen_get_n_workspaces (priv->screen);
    int width = (priv->child_width + priv->child_spacing) * n + DWI_MARGIN_HORIZONTAL * 2 - priv->child_spacing;
    int height = priv->child_height + 2 * DWI_MARGIN_VERTICAL; 


    priv->fixed = gtk_fixed_new();
    g_object_set(G_OBJECT(priv->fixed), "margin", 0, NULL);
    gtk_widget_set_size_request(priv->fixed, width, height);
    gtk_container_add(GTK_CONTAINER(dwi), priv->fixed);
    
    GList *l = priv->screen->workspaces;
    while (l) {
        MetaWorkspace* ws = (MetaWorkspace*)l->data;
        {
            DeepinWorkspacePreviewEntry *dwpe = deepin_workspace_preview_entry_new(ws);

            if (ws == priv->screen->active_workspace) {
                priv->active_entry = dwpe;
                deepin_workspace_preview_entry_set_select(dwpe, TRUE);
            }

            priv->workspaces = g_list_append(priv->workspaces, dwpe);
        }

        l = l->next;
    }

    int index = 0;
    l = priv->workspaces;
    while (l) {
        gtk_fixed_put(GTK_FIXED(priv->fixed), (GtkWidget*)l->data, 
                (priv->child_width + priv->child_spacing) * index + DWI_MARGIN_HORIZONTAL, DWI_MARGIN_VERTICAL);
        l = l->next;
        index++;
    }

    deepin_workspace_indicator_setup_style(dwi);

    gtk_window_set_default_size(GTK_WINDOW(dwi), width, height);
    gtk_window_move (GTK_WINDOW (dwi), 
            monitor_geom.x + (monitor_geom.width - width) / 2,
            monitor_geom.y + (monitor_geom.height - height) / 2);

    g_object_connect(G_OBJECT(deepin_message_hub_get()),
            "signal::workspace-added", (GCallback)on_workspace_added, self,
            "signal::workspace-removed", (GCallback)on_workspace_removed, self,
            "signal::workspace-reordered", (GCallback)on_workspace_reordered, self,
            NULL);

    return dwi;
}

static gboolean on_timeout(DeepinWorkspaceIndicator *self)
{
    DeepinWorkspaceIndicatorPrivate *priv = self->priv;
    gtk_widget_hide(GTK_WIDGET(self));
    priv->timeout_id = 0;
    return G_SOURCE_REMOVE;
}

static void reset_timer(DeepinWorkspaceIndicator* self)
{
    DeepinWorkspaceIndicatorPrivate *priv = self->priv;

    if (priv->timeout_id) {
        g_source_remove(priv->timeout_id);
        priv->timeout_id = 0;
    }

    priv->timeout_id = g_timeout_add(DWI_POPUP_TIMEOUT, (GSourceFunc)on_timeout, self);
}

void deepin_workspace_indicator_request_workspace_change(
        DeepinWorkspaceIndicator* self, MetaWorkspace* workspace)
{
    DeepinWorkspaceIndicatorPrivate* priv = self->priv;

    reset_timer(self);

    gtk_widget_show_all(GTK_WIDGET(self));

    if (priv->active_entry != NULL) {
        deepin_workspace_preview_entry_set_select(priv->active_entry, FALSE);
        meta_verbose("%s: previous %d\n", __func__, 
                meta_workspace_index(deepin_workspace_preview_entry_get_workspace(priv->active_entry)));
    }

    GList *l = priv->workspaces;
    while (l) {
        DeepinWorkspacePreviewEntry *dwpe = DEEPIN_WORKSPACE_PREVIEW_ENTRY(l->data);
        if (deepin_workspace_preview_entry_get_workspace(dwpe) == workspace) {
            priv->active_entry = dwpe;
            meta_verbose("%s: current %d\n", __func__, 
                    meta_workspace_index(deepin_workspace_preview_entry_get_workspace(priv->active_entry)));
            deepin_workspace_preview_entry_set_select(dwpe, TRUE);
            break;
        }
        l = l->next;
    }

    gtk_widget_queue_draw(GTK_WIDGET(self));
}
