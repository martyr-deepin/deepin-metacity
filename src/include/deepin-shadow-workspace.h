/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-shadow-workspace.h
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

// true to do presentation animation during show up
void deepin_shadow_workspace_set_presentation(DeepinShadowWorkspace*, gboolean);
void deepin_shadow_workspace_set_current(DeepinShadowWorkspace*, gboolean);
void deepin_shadow_workspace_set_thumb_mode(DeepinShadowWorkspace*, gboolean);
/* show all windows of all workspaces on the screen, 
 * NOTE: this must be set before populate 
 **/
void deepin_shadow_workspace_set_show_all_windows(DeepinShadowWorkspace*, gboolean);

/* frozen workspace when do animation makes it faster */
void deepin_shadow_workspace_set_frozen(DeepinShadowWorkspace*, gboolean);

gboolean deepin_shadow_workspace_get_is_thumb_mode(DeepinShadowWorkspace*);
gboolean deepin_shadow_workspace_get_is_current(DeepinShadowWorkspace*);
gboolean deepin_shadow_workspace_get_is_freezed(DeepinShadowWorkspace*);
gboolean deepin_shadow_workspace_get_is_all_window_mode(DeepinShadowWorkspace*);

/* initially, no window is focused on previewing */
void deepin_shadow_workspace_focus_next(DeepinShadowWorkspace*, gboolean);
MetaDeepinClonedWidget* deepin_shadow_workspace_get_focused(DeepinShadowWorkspace*);
void deepin_shadow_workspace_handle_event(DeepinShadowWorkspace* self,
        XIDeviceEvent* event, KeySym keysym, MetaKeyBindingAction action);
MetaWorkspace* deepin_shadow_workspace_get_workspace(DeepinShadowWorkspace*);

GdkWindow* deepin_shadow_workspace_get_event_window(DeepinShadowWorkspace*);

G_END_DECLS

#endif /* _DEEPIN_SHADOW_WORKSPACE_H_ */

