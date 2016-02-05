/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#include <util.h>
#include "deepin-workspace-indicator.h"
#include "deepin-timeline.h"
#include "deepin-design.h"

static const int FADE_DURATION = 200;
static const int POPUP_TIMEOUT = 200;
static const int POPUP_MAX_WIDTH = 300;

struct _DeepinWorkspaceIndicatorPrivate
{
    int stage; /* 1 = fade in, 2 = fade out */
    DeepinTimeline* timeline;
    guint timeout_id;
};

G_DEFINE_TYPE(DeepinWorkspaceIndicator, deepin_workspace_indicator, GTK_TYPE_LABEL);

static void deepin_workspace_indicator_init(DeepinWorkspaceIndicator *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, DEEPIN_TYPE_WORKSPACE_INDICATOR,
            DeepinWorkspaceIndicatorPrivate);

	/* TODO: Add initialization code here */
}

static void deepin_workspace_indicator_finalize (GObject *object)
{
	/* TODO: Add deinitalization code here */

	G_OBJECT_CLASS (deepin_workspace_indicator_parent_class)->finalize (object);
}

static void on_started(DeepinTimeline* timeline, DeepinWorkspaceIndicator* self)
{
    /*meta_verbose("%s", __func__);*/
    DeepinWorkspaceIndicatorPrivate *priv = self->priv;

    gtk_widget_set_opacity(GTK_WIDGET(self), priv->stage == 2 ? 1.0: 0.0); 
}

static gboolean on_popup_timeout(DeepinWorkspaceIndicator* self)
{
    /*meta_verbose("%s", __func__);*/
    DeepinWorkspaceIndicatorPrivate *priv = self->priv;
    priv->timeout_id = 0;

    if (!priv->timeline) return G_SOURCE_REMOVE;

    deepin_timeline_start(priv->timeline);
    return G_SOURCE_REMOVE;
}

static void on_stopped(DeepinTimeline* timeline, DeepinWorkspaceIndicator* self)
{
    /*meta_verbose("%s", __func__);*/
    DeepinWorkspaceIndicatorPrivate *priv = self->priv;

    gtk_widget_set_opacity(GTK_WIDGET(self), priv->stage == 2 ? 0.0: 1.0); 

    if (priv->stage == 1) {
        priv->stage = 2;
        priv->timeout_id = g_timeout_add(POPUP_TIMEOUT,
                (GSourceFunc)on_popup_timeout, self);
    }
}

static void on_new_frame(DeepinTimeline* timeline, double pos,
        DeepinWorkspaceIndicator* self)
{
    DeepinWorkspaceIndicatorPrivate *priv = self->priv;

    double t = priv->stage == 2 ? 1.0 - pos : pos;
    /*meta_verbose("%s: pos %f", __func__, t);*/
    gtk_widget_set_opacity(GTK_WIDGET(self), t); 
}

static void deepin_workspace_indicator_map (GtkWidget *widget)
{
    DeepinWorkspaceIndicator *self = DEEPIN_WORKSPACE_INDICATOR (widget);
    DeepinWorkspaceIndicatorPrivate *priv = self->priv;

    GTK_WIDGET_CLASS (deepin_workspace_indicator_parent_class)->map (widget);

    priv->timeline = deepin_timeline_new();
    deepin_timeline_set_clock(priv->timeline, gtk_widget_get_frame_clock(widget));
    deepin_timeline_set_duration(priv->timeline, FADE_DURATION);
    deepin_timeline_set_progress_mode(priv->timeline, DEEPIN_EASE_OUT_CUBIC);

    g_object_connect(G_OBJECT(priv->timeline),
            "signal::started", on_started, self,
            "signal::new-frame", on_new_frame, self,
            "signal::stopped", on_stopped, self,
            NULL);
}

static void deepin_workspace_indicator_unmap (GtkWidget *widget)
{
    DeepinWorkspaceIndicator *self = DEEPIN_WORKSPACE_INDICATOR (widget);
    DeepinWorkspaceIndicatorPrivate *priv = self->priv;

    if (deepin_timeline_is_playing(priv->timeline)) {
        deepin_timeline_stop(priv->timeline);
    }

    if (priv->timeout_id) {
        g_source_remove(priv->timeout_id);
        priv->timeout_id = 0;
    }

    g_signal_handlers_disconnect_by_data(priv->timeline, self);
    g_clear_pointer(&priv->timeline, g_object_unref);

    GTK_WIDGET_CLASS (deepin_workspace_indicator_parent_class)->unmap (widget);
}

static void deepin_workspace_indicator_class_init (DeepinWorkspaceIndicatorClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DeepinWorkspaceIndicatorPrivate));

	object_class->finalize = deepin_workspace_indicator_finalize;

    widget_class->map = deepin_workspace_indicator_map;
    widget_class->unmap = deepin_workspace_indicator_unmap;
}

GtkWidget* deepin_workspace_indicator_new()
{
    GtkWidget* w = (GtkWidget*)g_object_new(DEEPIN_TYPE_WORKSPACE_INDICATOR, NULL);

    deepin_setup_style_class(w, "deepin-workspace-name");
    gtk_widget_set_opacity(w, 0.0); 
    return w;
}

void deepin_workspace_indicator_request_workspace_change(
        DeepinWorkspaceIndicator* self, MetaWorkspace* workspace)
{
    DeepinWorkspaceIndicatorPrivate* priv = self->priv;

    g_return_if_fail(priv->timeline != NULL);
    if (deepin_timeline_is_playing(priv->timeline)) {
        deepin_timeline_stop(priv->timeline);
    }

    if (priv->timeout_id) {
        g_source_remove(priv->timeout_id);
        priv->timeout_id = 0;
    }

    priv->stage = 1;

    char* text = NULL;
    const char* name = meta_workspace_get_name(workspace);
    int workspace_index = meta_workspace_index(workspace);
    if (!name || name[0] == '\0') {
        text = g_strdup_printf("%d-%s", workspace_index + 1, _("Workspace"));
    } else {
        text = g_strdup_printf("%d %s", workspace_index + 1, name);
    }

    gtk_label_set_text(GTK_LABEL(self), text);
    g_free(text);

    deepin_timeline_start(priv->timeline);
}

