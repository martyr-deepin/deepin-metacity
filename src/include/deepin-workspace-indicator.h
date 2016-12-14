/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef _DEEPIN_WORKSPACE_INDICATOR_H_
#define _DEEPIN_WORKSPACE_INDICATOR_H_

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <gtk/gtk.h>
#include <prefs.h>
#include "../core/workspace.h"

G_BEGIN_DECLS

#define DEEPIN_TYPE_WORKSPACE_INDICATOR             (deepin_workspace_indicator_get_type ())
#define DEEPIN_WORKSPACE_INDICATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEEPIN_TYPE_WORKSPACE_INDICATOR, DeepinWorkspaceIndicator))
#define DEEPIN_WORKSPACE_INDICATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), DEEPIN_TYPE_WORKSPACE_INDICATOR, DeepinWorkspaceIndicatorClass))
#define DEEPIN_IS_WORKSPACE_INDICATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEEPIN_TYPE_WORKSPACE_INDICATOR))
#define DEEPIN_IS_WORKSPACE_INDICATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), DEEPIN_TYPE_WORKSPACE_INDICATOR))
#define DEEPIN_WORKSPACE_INDICATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), DEEPIN_TYPE_WORKSPACE_INDICATOR, DeepinWorkspaceIndicatorClass))

typedef struct _DeepinWorkspaceIndicatorClass DeepinWorkspaceIndicatorClass;
typedef struct _DeepinWorkspaceIndicator DeepinWorkspaceIndicator;
typedef struct _DeepinWorkspaceIndicatorPrivate DeepinWorkspaceIndicatorPrivate;

struct _DeepinWorkspaceIndicatorClass
{
	GtkWindowClass parent_class;
};

struct _DeepinWorkspaceIndicator
{
	GtkWindow parent_instance;

	DeepinWorkspaceIndicatorPrivate *priv;
};

GType deepin_workspace_indicator_get_type (void) G_GNUC_CONST;
GtkWidget* deepin_workspace_indicator_new(MetaScreen*);
void deepin_workspace_indicator_free(DeepinWorkspaceIndicator *dwi);
void deepin_workspace_indicator_request_workspace_change(
        DeepinWorkspaceIndicator* self, MetaWorkspace* workspace);

G_END_DECLS

#endif /* _DEEPIN_WORKSPACE_INDICATOR_H_ */


