/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */


/**
 * Copyright (C) 2016 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef DEEPIN_SWITCH_PREVIEWER_SIMPLE_H
#define DEEPIN_SWITCH_PREVIEWER_SIMPLE_H

#include <gtk/gtk.h>
#include "deepin-tabpopup.h"

#define META_TYPE_DEEPIN_SWITCH_PREVIEWER_SIMPLE         (meta_deepin_switch_previewer_simple_get_type ())
#define META_DEEPIN_SWITCH_PREVIEWER_SIMPLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), META_TYPE_DEEPIN_SWITCH_PREVIEWER_SIMPLE, MetaDeepinSwitchPreviewerSimple))
#define META_DEEPIN_SWITCH_PREVIEWER_SIMPLE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    META_TYPE_DEEPIN_SWITCH_PREVIEWER_SIMPLE, MetaDeepinSwitchPreviewerSimpleClass))
#define META_IS_DEEPIN_SWITCH_PREVIEWER_SIMPLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), META_TYPE_DEEPIN_SWITCH_PREVIEWER_SIMPLE))
#define META_IS_DEEPIN_SWITCH_PREVIEWER_SIMPLE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    META_TYPE_DEEPIN_SWITCH_PREVIEWER_SIMPLE))
#define META_DEEPIN_SWITCH_PREVIEWER_SIMPLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  META_TYPE_DEEPIN_SWITCH_PREVIEWER_SIMPLE, MetaDeepinSwitchPreviewerSimpleClass))

typedef struct _MetaDeepinSwitchPreviewerSimple        MetaDeepinSwitchPreviewerSimple;
typedef struct _MetaDeepinSwitchPreviewerSimpleClass   MetaDeepinSwitchPreviewerSimpleClass;
typedef struct _MetaDeepinSwitchPreviewerSimplePrivate MetaDeepinSwitchPreviewerSimplePrivate;
typedef struct _MetaDeepinSwitchPreviewerSimpleChild   MetaDeepinSwitchPreviewerSimpleChild;

struct _MetaDeepinSwitchPreviewerSimple
{
    GtkContainer            parent;
    MetaDeepinSwitchPreviewerSimplePrivate *priv;
};

struct _MetaDeepinSwitchPreviewerSimpleClass
{
    GtkContainerClass parent_class;
};

GType      meta_deepin_switch_previewer_simple_get_type (void) G_GNUC_CONST;
GtkWidget* meta_deepin_switch_previewer_simple_new (DeepinTabPopup* popup);
void meta_deepin_switch_previewer_simple_populate(MetaDeepinSwitchPreviewerSimple* self);
void meta_deepin_switch_previewer_simple_put (MetaDeepinSwitchPreviewerSimple  *self, 
        GtkWidget *widget, gint x, gint y);
void meta_deepin_switch_previewer_simple_select(MetaDeepinSwitchPreviewerSimple* self,
        DeepinTabEntry*);

#endif

