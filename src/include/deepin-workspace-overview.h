/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef _DEEPIN_WORKSPACE_OVERVIEW_H_
#define _DEEPIN_WORKSPACE_OVERVIEW_H_

#include <gtk/gtk.h>
#include <prefs.h>

#include "deepin-fixed.h"
#include "../core/workspace.h"
#include "deepin-cloned-widget.h"

G_BEGIN_DECLS

#define DEEPIN_TYPE_WORKSPACE_OVERVIEW             (deepin_workspace_overview_get_type ())
#define DEEPIN_WORKSPACE_OVERVIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEEPIN_TYPE_WORKSPACE_OVERVIEW, DeepinWorkspaceOverview))
#define DEEPIN_WORKSPACE_OVERVIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), DEEPIN_TYPE_WORKSPACE_OVERVIEW, DeepinWorkspaceOverviewClass))
#define DEEPIN_IS_WORKSPACE_OVERVIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEEPIN_TYPE_WORKSPACE_OVERVIEW))
#define DEEPIN_IS_WORKSPACE_OVERVIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), DEEPIN_TYPE_WORKSPACE_OVERVIEW))
#define DEEPIN_WORKSPACE_OVERVIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), DEEPIN_TYPE_WORKSPACE_OVERVIEW, DeepinWorkspaceOverviewClass))

typedef struct _DeepinWorkspaceOverviewClass DeepinWorkspaceOverviewClass;
typedef struct _DeepinWorkspaceOverview DeepinWorkspaceOverview;
typedef struct _DeepinWorkspaceOverviewPrivate DeepinWorkspaceOverviewPrivate;


struct _DeepinWorkspaceOverviewClass
{
	DeepinFixedClass parent_class;
};

struct _DeepinWorkspaceOverview
{
	DeepinFixed parent_instance;

	DeepinWorkspaceOverviewPrivate *priv;
};

GType deepin_workspace_overview_get_type (void) G_GNUC_CONST;
GtkWidget* deepin_workspace_overview_new(void);
void deepin_workspace_overview_populate(DeepinWorkspaceOverview* self,
        MetaWorkspace* ws);
void deepin_workspace_overview_set_show_all_windows(DeepinWorkspaceOverview*, gboolean);

gboolean deepin_workspace_overview_get_is_all_window_mode(DeepinWorkspaceOverview*);

/* initially, no window is focused on previewing */
void deepin_workspace_overview_focus_next(DeepinWorkspaceOverview*, gboolean);
MetaDeepinClonedWidget* deepin_workspace_overview_get_focused(DeepinWorkspaceOverview*);
void deepin_workspace_overview_handle_event(DeepinWorkspaceOverview* self,
        XIDeviceEvent* event, KeySym keysym, MetaKeyBindingAction action);
MetaWorkspace* deepin_workspace_overview_get_workspace(DeepinWorkspaceOverview*);

GdkWindow* deepin_workspace_overview_get_event_window(DeepinWorkspaceOverview*);
void deepin_workspace_overview_set_present_windows(DeepinWorkspaceOverview* self, GVariant* xids);

G_END_DECLS

#endif /* _DEEPIN_WORKSPACE_OVERVIEW_H_ */

