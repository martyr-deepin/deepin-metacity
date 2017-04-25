/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */


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
#include <util.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <cairo-xlib.h>
#include <screen.h>
#include <window.h>
#include <prefs.h>
#include "../core/workspace.h"
#include "../core/window-private.h"
#include "deepin-switch-previewer-simple.h"
#include "deepin-tabpopup.h"

struct _MetaDeepinSwitchPreviewerSimpleChild
{
    MetaWindow* window;
    gboolean hidden; // if original state is hidden or not
};

struct _MetaDeepinSwitchPreviewerSimplePrivate
{
    MetaScreen* screen;
    MetaWorkspace* active_workspace;
    GList *children;
    DeepinTabPopup* popup;
    MetaDeepinSwitchPreviewerSimpleChild* current;
};


G_DEFINE_TYPE_WITH_PRIVATE (MetaDeepinSwitchPreviewerSimple, meta_deepin_switch_previewer_simple, G_TYPE_OBJECT)

static void child_free(void* ptr)
{
    g_slice_free(MetaDeepinSwitchPreviewerSimpleChild, ptr);
}

static void meta_deepin_switch_previewer_simple_restore(MetaDeepinSwitchPreviewerSimple* self)
{
    MetaDeepinSwitchPreviewerSimplePrivate *priv = self->priv;
    MetaDeepinSwitchPreviewerSimpleChild* child = NULL;

    meta_display_grab(priv->screen->display);

    GList *l = priv->children;
    while (l) {
        child = (MetaDeepinSwitchPreviewerSimpleChild*)l->data;
        if (!child->hidden) {
            meta_window_set_showing(child->window, TRUE);
        }
        l = l->next;
    }

    meta_display_ungrab(priv->screen->display);
}

static void meta_deepin_switch_previewer_simple_dispose(GObject *object)
{
    MetaDeepinSwitchPreviewerSimple *self = META_DEEPIN_SWITCH_PREVIEWER_SIMPLE(object);
    MetaDeepinSwitchPreviewerSimplePrivate* priv = self->priv;

    if (priv->children) {
        if (priv->current && priv->current->window->type != META_WINDOW_DESKTOP) {
            meta_deepin_switch_previewer_simple_restore(self);
        }
        g_list_free_full(priv->children, child_free);
        priv->children = NULL;
    }

    G_OBJECT_CLASS(meta_deepin_switch_previewer_simple_parent_class)->dispose(object);
}

static void meta_deepin_switch_previewer_simple_class_init (MetaDeepinSwitchPreviewerSimpleClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = meta_deepin_switch_previewer_simple_dispose;
}

static void meta_deepin_switch_previewer_simple_init (MetaDeepinSwitchPreviewerSimple *self)
{
    self->priv = (MetaDeepinSwitchPreviewerSimplePrivate*)meta_deepin_switch_previewer_simple_get_instance_private (self);

    self->priv->children = NULL;
    self->priv->current = NULL;
}

GtkWidget* meta_deepin_switch_previewer_simple_new (DeepinTabPopup* popup)
{
    MetaDeepinSwitchPreviewerSimple* self =
        (MetaDeepinSwitchPreviewerSimple*)g_object_new(META_TYPE_DEEPIN_SWITCH_PREVIEWER_SIMPLE, NULL);

    MetaDeepinSwitchPreviewerSimplePrivate* priv = self->priv;
    priv->popup = popup;

    return (GtkWidget*)self;
}

void meta_deepin_switch_previewer_simple_populate(MetaDeepinSwitchPreviewerSimple* self)
{
    MetaDeepinSwitchPreviewerSimplePrivate* priv = self->priv;
    MetaDisplay* disp = meta_get_display();

    GList* l = priv->popup->entries;
    while (l) {
        DeepinTabEntry* te = (DeepinTabEntry*)l->data;
        MetaWindow* win = meta_display_lookup_x_window(disp, (Window)te->key);    

        if (win) {
            if (!priv->screen) {
                priv->screen = meta_window_get_screen(win);
                priv->active_workspace = priv->screen->active_workspace;
            }

            MetaDeepinSwitchPreviewerSimpleChild* child;
            child = g_slice_new0(MetaDeepinSwitchPreviewerSimpleChild);
            child->window = win;
            child->hidden = win->hidden;
            meta_window_set_showing(win, FALSE);

            priv->children = g_list_append(priv->children, child);
        }
        l = l->next;
    }
}

void meta_deepin_switch_previewer_simple_select(MetaDeepinSwitchPreviewerSimple* self,
        DeepinTabEntry* te)
{
    MetaDeepinSwitchPreviewerSimplePrivate *priv = self->priv;
    MetaDeepinSwitchPreviewerSimpleChild* child = NULL;

    GList *l = priv->children;
    while (l) {
        child = (MetaDeepinSwitchPreviewerSimpleChild*)l->data;
        if ((Window)te->key == child->window->xwindow) {
            break;
        }
        l = l->next;
    }

    if (child) {
        if (priv->current && priv->current->window->type != META_WINDOW_DESKTOP) {
                meta_window_set_showing(priv->current->window, FALSE);
        }
        priv->current = child;
        meta_window_set_showing(priv->current->window, TRUE);
    }
}


