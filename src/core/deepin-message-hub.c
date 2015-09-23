/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-message-hub.c
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

#include <config.h>
#include <string.h>
#include "deepin-message-hub.h"
#include "window-private.h"

static DeepinMessageHub* _the_hub = NULL;

struct _DeepinMessageHubPrivate
{
    guint disposed: 1;
};


enum
{
    SIGNAL_WINDOW_REMOVED,
    SIGNAL_WINDOW_ADDED,
    SIGNAL_WINDOW_DAMAGED,
    SIGNAL_DESKTOP_CHANGED,
    SIGNAL_SCREEN_RESIZED,
    SIGNAL_ABOUT_TO_CHANGE_WORKSPACE,
    SIGNAL_DRAG_END,

    LAST_SIGNAL
};


static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (DeepinMessageHub, deepin_message_hub, G_TYPE_OBJECT);

static void deepin_message_hub_init (DeepinMessageHub *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEEPIN_TYPE_MESSAGE_HUB, DeepinMessageHubPrivate);

    /* TODO: Add initialization code here */
}

static void deepin_message_hub_finalize (GObject *object)
{
    /* TODO: Add deinitalization code here */

    G_OBJECT_CLASS (deepin_message_hub_parent_class)->finalize (object);
}

void deepin_message_hub_window_added(MetaWindow* window)
{
    g_debug("%s: %s", __func__, window->desc);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_WINDOW_ADDED], 0, window);
}

void deepin_message_hub_window_removed(MetaWindow* window)
{
    g_debug("%s: %s", __func__, window->desc);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_WINDOW_REMOVED], 0, window);
}

//FIXME: my god, some windows constantly send damage ...
void deepin_message_hub_window_damaged(MetaWindow* window, XRectangle* rects, int n)
{
    if (window == NULL || window->unmanaging || window->withdrawn)
        return;

    gboolean surface_need_update = FALSE;

    MetaRectangle bound;
    meta_window_get_input_rect(window, &bound);

    for (int i = 0; i < n; i++) {
        double sx = (double)rects[i].width / bound.width,
               sy = (double)rects[i].height / bound.height;
        if (sx > 0.15 && sy > 0.15) {
            /*g_debug("big enough (%d,%d)", rects[i].width, rects[i].height);*/
            surface_need_update = TRUE;
            break;
        }
    }

    if (surface_need_update) {
        g_debug("%s: %s", __func__, window->desc);
        g_signal_emit(deepin_message_hub_get(),
                signals[SIGNAL_WINDOW_DAMAGED], 0, window);
    }
}

void deepin_message_hub_desktop_changed()
{
    g_debug("%s", __func__);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_DESKTOP_CHANGED], 0);
}

void deepin_message_hub_screen_resized(MetaScreen* screen)
{
    g_debug("%s", __func__);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_SCREEN_RESIZED], 0, screen);
}

void deepin_message_hub_window_about_to_change_workspace(
        MetaWindow* window, MetaWorkspace* workspace)
{
    g_debug("%s: move %s to workspace %s", __func__, window->desc, 
            meta_workspace_get_name(workspace));
    g_signal_emit(deepin_message_hub_get(),
            signals[SIGNAL_ABOUT_TO_CHANGE_WORKSPACE], 0, 
            window, workspace);
}
 
void deepin_message_hub_drag_end()
{
    g_debug("%s", __func__);
    g_signal_emit(deepin_message_hub_get(), signals[SIGNAL_DRAG_END], 0); 
}

static void deepin_message_hub_class_init (DeepinMessageHubClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (DeepinMessageHubPrivate));

    object_class->finalize = deepin_message_hub_finalize;

    signals[SIGNAL_WINDOW_REMOVED] = g_signal_new ("window-removed",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[SIGNAL_WINDOW_ADDED] = g_signal_new ("window-added",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[SIGNAL_WINDOW_DAMAGED] = g_signal_new ("window-damaged",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[SIGNAL_DESKTOP_CHANGED] = g_signal_new ("desktop-changed",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 0);

    signals[SIGNAL_SCREEN_RESIZED] = g_signal_new ("screen-resized",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[SIGNAL_ABOUT_TO_CHANGE_WORKSPACE] = g_signal_new (
            "about-to-change-workspace",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 2,
            G_TYPE_POINTER, G_TYPE_POINTER);

    signals[SIGNAL_DRAG_END] = g_signal_new (
            "drag-end",
            G_OBJECT_CLASS_TYPE (klass),
            G_SIGNAL_RUN_LAST, 0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 0);
}

DeepinMessageHub* deepin_message_hub_get()
{
    if (!_the_hub) {
        _the_hub = (DeepinMessageHub*)g_object_new(DEEPIN_TYPE_MESSAGE_HUB, NULL);
    }
    return _the_hub;
}

