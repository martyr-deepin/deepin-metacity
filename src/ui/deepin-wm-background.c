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
#include <gdk/gdkx.h>
#include <prefs.h>
#include "deepin-wm-background.h"
#include "../core/screen-private.h"
#include "../core/display-private.h"
#include "deepin-design.h"
#include "deepin-shadow-workspace.h"
#include "deepin-window-surface-manager.h"
#include "deepin-fixed.h"
#include "deepin-workspace-adder.h"

struct _DeepinWMBackgroundPrivate
{
    MetaScreen* screen;
    GdkScreen* gscreen;

    gint disposed: 1;

    GtkWidget* fixed;

    DeepinShadowWorkspace* active_workspace;
    GList* worskpaces;
    GList* worskpace_thumbs;
    DeepinWorkspaceAdder* adder;

    /* calculation cache */
    int top_offset;
    int bottom_offset;
    float scale;
    gint width, height; /* for workspace */
    gint thumb_width, thumb_height; /* for top thumbs */

    DeepinShadowWorkspace* hover_ws;
    GtkWidget* close_button; /* for hovered workspace thumb */
};


G_DEFINE_TYPE (DeepinWMBackground, deepin_wm_background, GTK_TYPE_WINDOW);

static inline gboolean _show_adder(MetaScreen* screen)
{
    return meta_screen_get_n_workspaces(screen) < MAX_WORKSPACE_NUM;
}

static inline gboolean _show_closer(MetaScreen* screen)
{
    return meta_screen_get_n_workspaces(screen) > 1;
}

static void _hide_close_button(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv= self->priv;

    if (priv->close_button) {
        gtk_widget_set_opacity(priv->close_button, 0.0);
        deepin_fixed_move(DEEPIN_FIXED(priv->fixed), priv->close_button,
                -100, -100,
                FALSE);
    }
}

static void _move_close_button_for(DeepinWMBackground* self,
        DeepinShadowWorkspace* ws_thumb)
{
    DeepinWMBackgroundPrivate* priv= self->priv;

    GtkAllocation alloc;
    gtk_widget_get_allocation(GTK_WIDGET(ws_thumb), &alloc);

    gint x = 0, y = 0;
    gtk_container_child_get(GTK_CONTAINER(priv->fixed), GTK_WIDGET(ws_thumb),
            "x", &x, "y", &y, NULL);

    deepin_fixed_raise(DEEPIN_FIXED(priv->fixed), priv->close_button);
    deepin_fixed_move(DEEPIN_FIXED(priv->fixed), priv->close_button,
            x + alloc.width/2,
            y - alloc.height/2,
            FALSE);
}

static gint find_by_meta_workspace(gconstpointer a, gconstpointer b)
{
    DeepinShadowWorkspace* ws = (DeepinShadowWorkspace*)a;
    MetaWorkspace* meta = (MetaWorkspace*)b;
    if (deepin_shadow_workspace_get_workspace(ws) == meta) {
        return 0;
    }

    return 1;
}

static DeepinShadowWorkspace* _find_workspace(GList* l, MetaWorkspace* next)
{
    GList* tmp = g_list_find_custom(l, next, find_by_meta_workspace);
    g_assert(tmp->data);
    return (DeepinShadowWorkspace*)tmp->data;
}

static void _create_workspace(DeepinWMBackground*);

static void _delete_workspace(DeepinWMBackground*, DeepinShadowWorkspace*);

static gboolean on_adder_pressed(GtkWidget* adder, GdkEvent* event,
        gpointer user_data);

static void redeclare_workspace_name(DeepinShadowWorkspace* ws, DeepinWMBackground* self)
{
    deepin_shadow_workspace_declare_name(ws);
}
    
static void redeclare_workspace_names(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    g_list_foreach(priv->worskpace_thumbs, (GFunc)redeclare_workspace_name, self);
}
    
static void relayout(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    gboolean adder_renewed = FALSE;

    if (!_show_adder(priv->screen) && priv->adder) {
        gtk_container_remove(GTK_CONTAINER(priv->fixed), GTK_WIDGET(priv->adder));
        priv->adder = NULL;

    } else if (_show_adder(priv->screen) && !priv->adder) {
        priv->adder = (DeepinWorkspaceAdder*)deepin_workspace_adder_new();
        gtk_widget_set_size_request(GTK_WIDGET(priv->adder), 
                priv->thumb_width, priv->thumb_height);
        g_signal_connect(GTK_WIDGET(priv->adder), 
                "button-release-event", (GCallback)on_adder_pressed, self);
        gtk_widget_show(GTK_WIDGET(priv->adder));

        adder_renewed = TRUE;
    }

    redeclare_workspace_names(self);

    GList *l = priv->screen->workspaces;
    gint current = g_list_index(priv->worskpaces, priv->active_workspace);
    if (current < 0) current = 0;
    meta_verbose("current: %d\n", current);
    
    GdkRectangle geom;
    gint monitor_index = gdk_screen_get_monitor_at_window(priv->gscreen,
            gtk_widget_get_window(GTK_WIDGET(self)));
    gdk_screen_get_monitor_geometry(priv->gscreen, monitor_index, &geom);
            

    gint i = 0, pad = FLOW_CLONE_DISTANCE_PERCENT * geom.width;
    l = priv->worskpaces;
    while (l) {
        gint x = (geom.width - priv->width) / 2 +  (i - current) * (priv->width + pad);
        deepin_fixed_move(DEEPIN_FIXED(priv->fixed), (GtkWidget*)l->data,
                x + priv->width/2, priv->top_offset + priv->height/2,
                FALSE);

        i++;
        l = l->next;
    }

    i = 0;
    l = priv->worskpace_thumbs;
    int thumb_spacing = geom.width * SPACING_PERCENT;
    
    gint count = g_list_length(priv->worskpace_thumbs) + _show_adder(priv->screen);
    int thumb_y = (int)(geom.height * HORIZONTAL_OFFSET_PERCENT);
    int thumb_x = (geom.width - count * (priv->thumb_width + thumb_spacing))/2;

    while (l) {
        int x = thumb_x + i * (priv->thumb_width + thumb_spacing);
        int real_height = 0;
        gtk_widget_get_preferred_height((GtkWidget*)l->data, &real_height, NULL);

        deepin_fixed_move(DEEPIN_FIXED(priv->fixed), (GtkWidget*)l->data,
                x + priv->thumb_width/2, thumb_y + real_height/2,
                FALSE);

        i++;
        l = l->next;
    }

    if (priv->adder) {
        int x = thumb_x + i * (priv->thumb_width + thumb_spacing);
        int y = thumb_y + priv->thumb_height/2;

        if (adder_renewed) {
            deepin_fixed_put(DEEPIN_FIXED(priv->fixed), (GtkWidget*)priv->adder,
                    x + priv->thumb_width/2, y);
        } else {
            deepin_fixed_move(DEEPIN_FIXED(priv->fixed), (GtkWidget*)priv->adder,
                    x + priv->thumb_width/2, y, FALSE);
        }
    }
}

static gboolean on_close_button_clicked(GtkWidget* widget,
               GdkEvent* event, gpointer data)
{
    meta_verbose("%s\n", __func__);
    DeepinWMBackground* self = (DeepinWMBackground*)data;
    DeepinWMBackgroundPrivate* priv = self->priv;

    gint index = g_list_index(priv->worskpace_thumbs, priv->hover_ws);
    DeepinShadowWorkspace* ws = g_list_nth(priv->worskpaces, index)->data;
    _delete_workspace(self, ws);

    return TRUE;
}

static gboolean on_close_button_leaved(GtkWidget* widget,
               GdkEvent* event, gpointer data)
{
    meta_verbose("%s\n", __func__);
    DeepinWMBackground* self = (DeepinWMBackground*)data;
    DeepinWMBackgroundPrivate* priv = self->priv;

    if (priv->hover_ws) {
        /* FIXME: there is a problem: when cloned is gets focused (so scaled up),
         * leave event will looks like as if it happened inside of cloned. need
         * a workaround */
        gint x, y;
        x = event->crossing.x_root;
        y = event->crossing.y_root;

        GtkAllocation alloc;
        gtk_widget_get_allocation(GTK_WIDGET(priv->hover_ws), &alloc);

        GdkRectangle r = {alloc.x, alloc.y, alloc.width, priv->thumb_height};
        if (x > r.x && x < r.x + r.width && y > r.y && y < r.y + r.height) {
            return FALSE;
        }
    }

    priv->hover_ws = NULL;
    _hide_close_button(self);
    return FALSE;
}

static void _create_close_button(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv= self->priv;

    priv->close_button = gtk_event_box_new();
    gtk_event_box_set_above_child(GTK_EVENT_BOX(priv->close_button), TRUE);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(priv->close_button), FALSE);

    GtkWidget* image = gtk_image_new_from_file(METACITY_PKGDATADIR "/close.png");
    gtk_container_add(GTK_CONTAINER(priv->close_button), image);

    deepin_fixed_put(DEEPIN_FIXED(priv->fixed), priv->close_button, -100, -100);
    gtk_widget_set_opacity(priv->close_button, 0.0);

    g_object_connect(G_OBJECT(priv->close_button), 
            "signal::leave-notify-event", on_close_button_leaved, self,
            "signal::button-release-event", on_close_button_clicked, self,
            NULL);
}

static gboolean on_workspace_released(DeepinShadowWorkspace* ws,
               GdkEvent* event, gpointer user_data)
{
    meta_verbose("%s\n", __func__);
    DeepinWMBackground* self = (DeepinWMBackground*)user_data;
    DeepinWMBackgroundPrivate* priv = self->priv;
    
    if (priv->active_workspace != ws) {
        MetaWorkspace* next = deepin_shadow_workspace_get_workspace(ws);
        deepin_wm_background_switch_workspace(self, next);
        return TRUE;
    } 

    return FALSE;
}

static gboolean on_workspace_thumb_released(DeepinShadowWorkspace* ws_thumb,
               GdkEvent* event, gpointer user_data)
{
    meta_verbose("%s\n", __func__);
    DeepinWMBackground* self = (DeepinWMBackground*)user_data;
    DeepinWMBackgroundPrivate* priv = self->priv;
    
    MetaWorkspace* next = deepin_shadow_workspace_get_workspace(ws_thumb);
    if (deepin_shadow_workspace_get_workspace(priv->active_workspace) != next) {
        deepin_wm_background_switch_workspace(self, next);
    } 
    return TRUE;
}

static gboolean on_workspace_thumb_leaved(DeepinShadowWorkspace* ws_thumb,
               GdkEvent* event, gpointer data)
{
    DeepinWMBackground* self = (DeepinWMBackground*)data;
    meta_verbose("%s\n", __func__);

    gint x, y;
    x = event->crossing.x_root;
    y = event->crossing.y_root;

    GtkAllocation alloc;
    gtk_widget_get_allocation(GTK_WIDGET(ws_thumb), &alloc);

    GdkRectangle r = {alloc.x, alloc.y, alloc.width, alloc.height};
    if (x > r.x && x < r.x + r.width && y > r.y && y < r.y + r.height) {
        return FALSE;
    }

    self->priv->hover_ws = NULL;
    _hide_close_button(self);
    return TRUE;
}

static gboolean _idle_show_close_button(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    if (priv->disposed) return G_SOURCE_REMOVE;

    if (_show_closer(priv->screen) && priv->hover_ws) {
        _move_close_button_for(self, priv->hover_ws);
        gtk_widget_set_opacity(priv->close_button, 1.0);
    }
    return G_SOURCE_REMOVE;
}

static gboolean on_workspace_thumb_entered(DeepinShadowWorkspace* ws_thumb,
               GdkEvent* event, gpointer data)
{
    DeepinWMBackground* self = DEEPIN_WM_BACKGROUND(data);
    meta_verbose("%s\n", __func__);

    self->priv->hover_ws = ws_thumb;
    g_idle_add((GSourceFunc)_idle_show_close_button, self);
    return TRUE;
}

static void deepin_wm_background_init (DeepinWMBackground *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_WM_BACKGROUND, DeepinWMBackgroundPrivate);

    DeepinWMBackgroundPrivate* priv = self->priv;
    priv->worskpaces = NULL;
}

static void deepin_wm_background_dispose (GObject *object)
{
    DeepinWMBackground* self = DEEPIN_WM_BACKGROUND(object);
    DeepinWMBackgroundPrivate* priv = self->priv;

    if (!priv->disposed) {
        priv->disposed = TRUE;
        g_list_free(priv->worskpaces);
        g_list_free(priv->worskpace_thumbs);
    }

    G_OBJECT_CLASS (deepin_wm_background_parent_class)->dispose (object);
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
    object_class->dispose = deepin_wm_background_dispose;
}

void deepin_wm_background_switch_workspace(DeepinWMBackground* self, 
        MetaWorkspace* next)
{
    DeepinWMBackgroundPrivate* priv = self->priv;
    DeepinShadowWorkspace* next_ws = _find_workspace(priv->worskpaces, next);
    DeepinShadowWorkspace* next_thumb = _find_workspace(priv->worskpace_thumbs, next);

    DeepinShadowWorkspace* current_thumb = _find_workspace(priv->worskpace_thumbs,
                deepin_shadow_workspace_get_workspace(priv->active_workspace));
    DeepinShadowWorkspace* current = priv->active_workspace;

    deepin_shadow_workspace_set_current(current, FALSE);
    deepin_shadow_workspace_set_current(current_thumb, FALSE);

    deepin_shadow_workspace_set_current(next_ws, TRUE);
    deepin_shadow_workspace_set_current(next_thumb, TRUE);

    priv->active_workspace = next_ws;

    GdkRectangle geom;
    gint monitor_index = gdk_screen_get_monitor_at_window(priv->gscreen,
            gtk_widget_get_window(GTK_WIDGET(self)));
    gdk_screen_get_monitor_geometry(priv->gscreen, monitor_index, &geom);

    gint i = 0,
         current_pos = g_list_index(priv->worskpaces, next_ws),
         pad = FLOW_CLONE_DISTANCE_PERCENT * geom.width;

    GList* l = priv->worskpaces;
    while (l) {
        gint x = (geom.width - priv->width) / 2 +  (i - current_pos) * (priv->width + pad);

        deepin_fixed_move(DEEPIN_FIXED(priv->fixed), (GtkWidget*)l->data,
                x + priv->width/2, priv->top_offset + priv->height/2, 
                FALSE);

        i++;
        l = l->next;
    }

    meta_workspace_activate(
            deepin_shadow_workspace_get_workspace(priv->active_workspace), 
            gtk_get_current_event_time());
}

static gboolean on_adder_pressed(GtkWidget* adder, GdkEvent* event, gpointer user_data)
{
    DeepinWMBackground* self = DEEPIN_WM_BACKGROUND(user_data);
    _create_workspace(self);
    return TRUE;
}

void deepin_wm_background_setup(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    GdkRectangle geom;

    gint monitor_index = gdk_screen_get_primary_monitor(priv->gscreen);
    gdk_screen_get_monitor_geometry(priv->gscreen, monitor_index, &geom);
            
    GtkWidget* bin = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(self), bin);

    priv->fixed = deepin_fixed_new();
    gtk_widget_set_size_request(priv->fixed, geom.width, geom.height);
    gtk_fixed_put(GTK_FIXED(bin), priv->fixed, geom.x, geom.y);

    priv->top_offset = (int)(geom.height * FLOW_CLONE_TOP_OFFSET_PERCENT);
    priv->bottom_offset = (int)(geom.height * HORIZONTAL_OFFSET_PERCENT);
    float scale = (float)(geom.height - priv->top_offset - priv->bottom_offset) / geom.height;

    priv->scale = scale;
    priv->width = geom.width * scale;
    priv->height = geom.height * scale;

    // calculate monitor width height ratio
    float monitor_whr = (float)geom.height / geom.width;
    priv->thumb_width = geom.width * WORKSPACE_WIDTH_PERCENT;
    priv->thumb_height = priv->thumb_width * monitor_whr;

    GList *l = priv->screen->workspaces;
    gint current = 0;

    while (l) {
        MetaWorkspace* ws = (MetaWorkspace*)l->data;
        {
            DeepinShadowWorkspace* dsw = 
                (DeepinShadowWorkspace*)deepin_shadow_workspace_new();
            deepin_shadow_workspace_set_enable_drag(dsw, TRUE);
            deepin_shadow_workspace_set_scale(dsw, scale);
            deepin_shadow_workspace_populate(dsw, ws);

            if (ws == priv->screen->active_workspace) {
                current = g_list_index(priv->screen->workspaces, l->data);
                priv->active_workspace = dsw;
                deepin_shadow_workspace_set_current(dsw, TRUE);
            }

            g_object_connect(G_OBJECT(dsw), 
                    "signal::button-release-event", on_workspace_released, self,
                    NULL);

            priv->worskpaces = g_list_append(priv->worskpaces, dsw);
        }

        /* for top workspace thumbnail */
        {
            DeepinShadowWorkspace* dsw = 
                (DeepinShadowWorkspace*)deepin_shadow_workspace_new();
            deepin_shadow_workspace_set_enable_drag(dsw, TRUE);
            deepin_shadow_workspace_set_thumb_mode(dsw, TRUE);
            deepin_shadow_workspace_set_scale(dsw, WORKSPACE_WIDTH_PERCENT);
            deepin_shadow_workspace_populate(dsw, ws);

            if (ws == priv->screen->active_workspace) {
                deepin_shadow_workspace_set_current(dsw, TRUE);
            }

            g_object_connect(G_OBJECT(dsw), 
                    "signal::enter-notify-event", on_workspace_thumb_entered, self,
                    "signal::leave-notify-event", on_workspace_thumb_leaved, self,
                    "signal::button-release-event", on_workspace_thumb_released, self,
                    NULL);

            priv->worskpace_thumbs = g_list_append(priv->worskpace_thumbs, dsw);
        }

        l = l->next;
    }

    if (_show_adder(priv->screen) && !priv->adder) {
        priv->adder = (DeepinWorkspaceAdder*)deepin_workspace_adder_new();
        gtk_widget_set_size_request(GTK_WIDGET(priv->adder), 
                priv->thumb_width, priv->thumb_height);
        g_signal_connect(GTK_WIDGET(priv->adder), 
                "button-release-event", (GCallback)on_adder_pressed, self);
    }

    gint i = 0, pad = FLOW_CLONE_DISTANCE_PERCENT * geom.width;
    l = priv->worskpaces;
    while (l) {
        gint x = (geom.width - priv->width) / 2 +  (i - current) * (priv->width + pad);
        deepin_fixed_put(DEEPIN_FIXED(priv->fixed), (GtkWidget*)l->data,
                x + priv->width/2, priv->top_offset + priv->height/2);

        i++;
        l = l->next;
    }

    i = 0;
    l = priv->worskpace_thumbs;
    int thumb_spacing = geom.width * SPACING_PERCENT;
    
    gint count = g_list_length(priv->worskpace_thumbs) + _show_adder(priv->screen);
    int thumb_y = (int)(geom.height * HORIZONTAL_OFFSET_PERCENT);
    int thumb_x = (geom.width - count * (priv->thumb_width + thumb_spacing))/2;

    while (l) {
        gtk_widget_show((GtkWidget*)l->data);

        int x = thumb_x + i * (priv->thumb_width + thumb_spacing);
        int real_height = 0;
        gtk_widget_get_preferred_height((GtkWidget*)l->data, &real_height, NULL);

        deepin_fixed_put(DEEPIN_FIXED(priv->fixed), (GtkWidget*)l->data,
                x + priv->thumb_width/2,
                thumb_y + real_height/2);

        i++;
        l = l->next;
    }

    if (priv->adder) {
        int x = thumb_x + i * (priv->thumb_width + thumb_spacing);
        deepin_fixed_put(DEEPIN_FIXED(priv->fixed), (GtkWidget*)priv->adder,
                x + priv->thumb_width/2,
                thumb_y + priv->thumb_height/2);
    }

    _create_close_button(self);

    gtk_window_move(GTK_WINDOW(self), geom.x, geom.y);
}

static gboolean on_deepin_wm_background_event(DeepinWMBackground* self,
        GdkEvent* ev, gpointer data)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    switch(ev->type) {
        case GDK_BUTTON_PRESS:
            break;
        case GDK_BUTTON_RELEASE:
            break;

        case GDK_KEY_PRESS:
        case GDK_KEY_RELEASE:
            return TRUE;

        default: break;
    }

    return FALSE;
}

GtkWidget* deepin_wm_background_new(MetaScreen* screen)
{
    GtkWidget* widget = (GtkWidget*)g_object_new(DEEPIN_TYPE_WM_BACKGROUND,
            "type", GTK_WINDOW_POPUP, NULL);
    deepin_setup_style_class(widget, "deepin-window-manager"); 

    DeepinWMBackground* self = DEEPIN_WM_BACKGROUND(widget);

    self->priv->screen = screen;
    MetaDisplay* display = meta_get_display();
    GdkDisplay* gdisplay = gdk_x11_lookup_xdisplay(display->xdisplay);
    self->priv->gscreen = gdk_display_get_default_screen(gdisplay);

    gtk_widget_realize (widget);

    gint w = gdk_screen_get_width(self->priv->gscreen),
         h = gdk_screen_get_height(self->priv->gscreen);
    gtk_window_set_default_size(GTK_WINDOW(widget), w, h);

    gtk_window_set_keep_above(GTK_WINDOW(widget), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(widget), FALSE);

    g_object_connect(G_OBJECT(widget),
            "signal::event", on_deepin_wm_background_event, NULL,
            NULL);

    return widget;
}

static void _delete_workspace(DeepinWMBackground* self,
        DeepinShadowWorkspace* ws)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    gboolean need_switch_active = FALSE;

    _hide_close_button(self);

    MetaWorkspace* workspace = deepin_shadow_workspace_get_workspace(ws);
    DeepinShadowWorkspace* ws_thumb = _find_workspace(priv->worskpace_thumbs, workspace);
    g_assert(ws_thumb != NULL);
    if (priv->hover_ws) priv->hover_ws = NULL;

    need_switch_active = (ws == priv->active_workspace);

    priv->worskpaces = g_list_remove(priv->worskpaces, ws);
    priv->worskpace_thumbs = g_list_remove(priv->worskpace_thumbs, ws_thumb);

    gtk_container_remove(GTK_CONTAINER(priv->fixed), (GtkWidget*)ws_thumb);
    gtk_container_remove(GTK_CONTAINER(priv->fixed), (GtkWidget*)ws);

    meta_prefs_change_workspace_name(meta_prefs_get_num_workspaces() - 1, "");
    meta_screen_remove_workspace(priv->screen, workspace);

    if (need_switch_active) {
        MetaWorkspace* next_ws = priv->screen->active_workspace;
        priv->active_workspace = _find_workspace(priv->worskpaces, next_ws);
        DeepinShadowWorkspace* current_thumb = _find_workspace(
                priv->worskpace_thumbs,
                deepin_shadow_workspace_get_workspace(priv->active_workspace));

        deepin_shadow_workspace_set_current(priv->active_workspace, TRUE);
        deepin_shadow_workspace_set_current(current_thumb, TRUE);
    }

    relayout(self);

    meta_workspace_activate(
            deepin_shadow_workspace_get_workspace(priv->active_workspace), 
            gtk_get_current_event_time());

    if (priv->hover_ws && _show_closer(priv->screen)) {
        _move_close_button_for(self, priv->hover_ws);
        gtk_widget_set_opacity(priv->close_button, 1.0);
    }
}

static void _create_workspace(DeepinWMBackground* self)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    meta_verbose("%s\n", __func__);
    if (meta_screen_get_n_workspaces(priv->screen) >= MAX_WORKSPACE_NUM) return;

    GdkRectangle geom;
    gint monitor_index = gdk_screen_get_monitor_at_window(priv->gscreen,
            gtk_widget_get_window(GTK_WIDGET(self)));
    gdk_screen_get_monitor_geometry(priv->gscreen, monitor_index, &geom);

    MetaWorkspace* new_ws = meta_screen_new_workspace(priv->screen);
    gint i = g_list_length(priv->worskpaces)-1,
         current = g_list_index(priv->worskpaces, priv->active_workspace),
         pad = FLOW_CLONE_DISTANCE_PERCENT * geom.width;

    {
        DeepinShadowWorkspace* dsw = 
            (DeepinShadowWorkspace*)deepin_shadow_workspace_new();
        deepin_shadow_workspace_set_enable_drag(dsw, TRUE);
        deepin_shadow_workspace_set_scale(dsw, priv->scale);
        deepin_shadow_workspace_populate(dsw, new_ws);
        gtk_widget_show(GTK_WIDGET(dsw));

        priv->worskpaces = g_list_append(priv->worskpaces, dsw);

        gint x = (geom.width - priv->width) / 2 +  (i - current) * (priv->width + pad);
        deepin_fixed_put(DEEPIN_FIXED(priv->fixed), (GtkWidget*)dsw,
                x + priv->width/2, priv->top_offset + priv->height/2);
    }

    /* for top workspace thumbnail */
    {
        DeepinShadowWorkspace* dsw = 
            (DeepinShadowWorkspace*)deepin_shadow_workspace_new();
        deepin_shadow_workspace_set_enable_drag(dsw, TRUE);
        deepin_shadow_workspace_set_thumb_mode(dsw, TRUE);
        deepin_shadow_workspace_set_scale(dsw, WORKSPACE_WIDTH_PERCENT);
        deepin_shadow_workspace_populate(dsw, new_ws);

        g_object_connect(G_OBJECT(dsw), 
                "signal::enter-notify-event", on_workspace_thumb_entered, self,
                "signal::leave-notify-event", on_workspace_thumb_leaved, self,
                "signal::button-release-event", on_workspace_thumb_released, self,
                NULL);

        gtk_widget_show(GTK_WIDGET(dsw));
        priv->worskpace_thumbs = g_list_append(priv->worskpace_thumbs, dsw);

        int thumb_spacing = geom.width * SPACING_PERCENT;
        gint count = g_list_length(priv->worskpace_thumbs) + 1;
        int thumb_y = (int)(geom.height * HORIZONTAL_OFFSET_PERCENT);
        int thumb_x = (geom.width - count * (priv->thumb_width + thumb_spacing))/2;

        int x = thumb_x + i * (priv->thumb_width + thumb_spacing);
        int real_height = 0;
        gtk_widget_get_preferred_height((GtkWidget*)dsw, &real_height, NULL);

        deepin_fixed_put(DEEPIN_FIXED(priv->fixed), (GtkWidget*)dsw,
                x + priv->thumb_width/2, 
                thumb_y + real_height/2);
    }

    priv->hover_ws = NULL;
    relayout(self);
    
    if (priv->hover_ws && _show_closer(priv->screen)) {
        _move_close_button_for(self, priv->hover_ws);
        gtk_widget_set_opacity(priv->close_button, 1.0);
    }

    meta_workspace_activate(new_ws, gtk_get_current_event_time());
    deepin_wm_background_switch_workspace(self, new_ws);
}

static void _handle_workspace_creation(DeepinWMBackground* self,
        XIDeviceEvent* event, KeySym keysym, MetaKeyBindingAction action)
{
    _create_workspace(self);
}

static void _handle_workspace_deletion(DeepinWMBackground* self,
        XIDeviceEvent* event, KeySym keysym, MetaKeyBindingAction action)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    g_assert(priv->active_workspace);
    _delete_workspace(self, priv->active_workspace);
}

static void _handle_workspace_goto(DeepinWMBackground* self,
        XIDeviceEvent* event, KeySym keysym, MetaKeyBindingAction action)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    gint index = keysym - XK_1;
    MetaWorkspace* current = deepin_shadow_workspace_get_workspace(priv->active_workspace); 
    MetaWorkspace* next = meta_screen_get_workspace_by_index(current->screen, index);
    if (next) {
        if (next == current) {
            //bouncing
            return;
        }

        deepin_wm_background_switch_workspace(self, next);
    }
}

static void _handle_workspace_switch(DeepinWMBackground* self, XIDeviceEvent* event,
        KeySym keysym, MetaKeyBindingAction action)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    MetaMotionDirection dir = keysym == XK_Left ? META_MOTION_LEFT:META_MOTION_RIGHT;
    MetaWorkspace* current = deepin_shadow_workspace_get_workspace(priv->active_workspace); 
    MetaWorkspace* next = meta_workspace_get_neighbor(current, dir);
    if (next) {
        if (next == current) {
            //bouncing
            return;
        }

        deepin_wm_background_switch_workspace(self, next);
    }
}

static struct wm_event_dispatch_ 
{
    KeySym trigger;
    void (*handler)(DeepinWMBackground* self, XIDeviceEvent* event,
            KeySym keysym, MetaKeyBindingAction action);

} dispatcher[] = {
    {XK_Left,           _handle_workspace_switch},
    {XK_Right,          _handle_workspace_switch},
    {XK_plus,           _handle_workspace_creation},
    {XK_equal,          _handle_workspace_creation},
    {XK_minus,          _handle_workspace_deletion},
    {XK_underscore,     _handle_workspace_deletion},
    {XK_1,             _handle_workspace_goto},
    {XK_2,             _handle_workspace_goto},
    {XK_3,             _handle_workspace_goto},
    {XK_4,             _handle_workspace_goto},
    {XK_5,             _handle_workspace_goto},
    {XK_6,             _handle_workspace_goto},
    {XK_7,             _handle_workspace_goto},
};

void deepin_wm_background_handle_event(DeepinWMBackground* self, XIDeviceEvent* event, 
        KeySym keysym, MetaKeyBindingAction action)
{
    DeepinWMBackgroundPrivate* priv = self->priv;

    /* HACK: this is really dirty, better come up with a better 
     * solution 
     */
    GtkWidget* w = gtk_grab_get_current();
    gboolean intercept = (w && GTK_IS_ENTRY(w));
    meta_verbose("%s: intercept %d\n", __func__, intercept);

    if (!intercept) {
        for (int i = 0, len = G_N_ELEMENTS(dispatcher); i < len; i++) {
            if (dispatcher[i].trigger == keysym) {
                dispatcher[i].handler(self, event, keysym, action);
                return;
            }
        }

    } 

    if (keysym == XK_F2) {
        // send this to active thumb for editting name
        DeepinShadowWorkspace* thumb = _find_workspace(priv->worskpace_thumbs,
                deepin_shadow_workspace_get_workspace(priv->active_workspace));
        g_assert(thumb != NULL);
        deepin_shadow_workspace_handle_event(thumb, event, keysym, action);

    } else {
        /* pass through to active workspace */
        deepin_shadow_workspace_handle_event(priv->active_workspace,
                event, keysym, action);
    }
}

