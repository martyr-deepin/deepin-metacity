/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DEEPIN_SWITCH_PREVIEWER_H
#define DEEPIN_SWITCH_PREVIEWER_H

#include <gtk/gtk.h>
#include "tabpopup-private.h"

#define META_TYPE_DEEPIN_SWITCH_PREVIEWER         (meta_deepin_switch_previewer_get_type ())
#define META_DEEPIN_SWITCH_PREVIEWER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), META_TYPE_DEEPIN_SWITCH_PREVIEWER, MetaDeepinSwitchPreviewer))
#define META_DEEPIN_SWITCH_PREVIEWER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    META_TYPE_DEEPIN_SWITCH_PREVIEWER, MetaDeepinSwitchPreviewerClass))
#define META_IS_DEEPIN_SWITCH_PREVIEWER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), META_TYPE_DEEPIN_SWITCH_PREVIEWER))
#define META_IS_DEEPIN_SWITCH_PREVIEWER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    META_TYPE_DEEPIN_SWITCH_PREVIEWER))
#define META_DEEPIN_SWITCH_PREVIEWER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  META_TYPE_DEEPIN_SWITCH_PREVIEWER, MetaDeepinSwitchPreviewerClass))

typedef struct _MetaDeepinSwitchPreviewer        MetaDeepinSwitchPreviewer;
typedef struct _MetaDeepinSwitchPreviewerClass   MetaDeepinSwitchPreviewerClass;
typedef struct _MetaDeepinSwitchPreviewerPrivate MetaDeepinSwitchPreviewerPrivate;
typedef struct _MetaDeepinSwitchPreviewerChild   MetaDeepinSwitchPreviewerChild;

struct _MetaDeepinSwitchPreviewer
{
    GtkContainer            parent;
    MetaDeepinSwitchPreviewerPrivate *priv;
};

struct _MetaDeepinSwitchPreviewerClass
{
    GtkContainerClass parent_class;
};

GType      meta_deepin_switch_previewer_get_type (void) G_GNUC_CONST;
GtkWidget* meta_deepin_switch_previewer_new (MetaTabPopup* popup);
void meta_deepin_switch_previewer_populate(MetaDeepinSwitchPreviewer* self);
void meta_deepin_switch_previewer_put (MetaDeepinSwitchPreviewer  *self, 
        GtkWidget *widget, gint x, gint y);
void meta_deepin_switch_previewer_select(MetaDeepinSwitchPreviewer* self,
        TabEntry*);

#endif
