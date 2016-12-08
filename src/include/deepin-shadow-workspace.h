/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef _DEEPIN_SHADOW_WORKSPACE_H_
#define _DEEPIN_SHADOW_WORKSPACE_H_

#include <gtk/gtk.h>
#include <prefs.h>

#include "deepin-fixed.h"
#include "../core/workspace.h"
#include "deepin-cloned-widget.h"

G_BEGIN_DECLS

#define DEEPIN_TYPE_SHADOW_WORKSPACE             (deepin_shadow_workspace_get_type ())
#define DEEPIN_SHADOW_WORKSPACE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEEPIN_TYPE_SHADOW_WORKSPACE, DeepinShadowWorkspace))
#define DEEPIN_SHADOW_WORKSPACE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), DEEPIN_TYPE_SHADOW_WORKSPACE, DeepinShadowWorkspaceClass))
#define DEEPIN_IS_SHADOW_WORKSPACE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEEPIN_TYPE_SHADOW_WORKSPACE))
#define DEEPIN_IS_SHADOW_WORKSPACE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), DEEPIN_TYPE_SHADOW_WORKSPACE))
#define DEEPIN_SHADOW_WORKSPACE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), DEEPIN_TYPE_SHADOW_WORKSPACE, DeepinShadowWorkspaceClass))

typedef struct _DeepinShadowWorkspaceClass DeepinShadowWorkspaceClass;
typedef struct _DeepinShadowWorkspace DeepinShadowWorkspace;
typedef struct _DeepinShadowWorkspacePrivate DeepinShadowWorkspacePrivate;


struct _DeepinShadowWorkspaceClass
{
	DeepinFixedClass parent_class;
};

struct _DeepinShadowWorkspace
{
	DeepinFixed parent_instance;

	DeepinShadowWorkspacePrivate *priv;
};

GType deepin_shadow_workspace_get_type (void) G_GNUC_CONST;
GtkWidget* deepin_shadow_workspace_new(void);
void deepin_shadow_workspace_populate(DeepinShadowWorkspace* self,
        MetaWorkspace* ws);
void deepin_shadow_workspace_set_scale(DeepinShadowWorkspace*, gdouble);
gdouble deepin_shadow_workspace_get_scale(DeepinShadowWorkspace*);

void deepin_shadow_workspace_set_current(DeepinShadowWorkspace*, gboolean);
void deepin_shadow_workspace_set_thumb_mode(DeepinShadowWorkspace*, gboolean);
/* 
 * NOTE: this must be set before populate 
 */
void deepin_shadow_workspace_set_enable_drag(DeepinShadowWorkspace*, gboolean);
gboolean deepin_shadow_workspace_is_dragging(DeepinShadowWorkspace*);

gboolean deepin_shadow_workspace_get_is_thumb_mode(DeepinShadowWorkspace*);
gboolean deepin_shadow_workspace_get_is_current(DeepinShadowWorkspace*);

/* initially, no window is focused on previewing */
void deepin_shadow_workspace_focus_next(DeepinShadowWorkspace*, gboolean);
MetaDeepinClonedWidget* deepin_shadow_workspace_get_focused(DeepinShadowWorkspace*);
void deepin_shadow_workspace_handle_event(DeepinShadowWorkspace* self,
        XIDeviceEvent* event, KeySym keysym, MetaKeyBindingAction action);
MetaWorkspace* deepin_shadow_workspace_get_workspace(DeepinShadowWorkspace*);

GdkWindow* deepin_shadow_workspace_get_event_window(DeepinShadowWorkspace*);

void deepin_shadow_workspace_declare_name(DeepinShadowWorkspace* self);

G_END_DECLS

#endif /* _DEEPIN_SHADOW_WORKSPACE_H_ */

