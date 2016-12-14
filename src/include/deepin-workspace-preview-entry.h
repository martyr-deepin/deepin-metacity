/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef _DEEPIN_WORKSPACE_PREVIEW_ENTRY_H_
#define _DEEPIN_WORKSPACE_PREVIEW_ENTRY_H_

#include <gtk/gtk.h>
#include <prefs.h>

#include "deepin-fixed.h"
#include "../core/workspace.h"
#include "deepin-cloned-widget.h"

G_BEGIN_DECLS

#define DEEPIN_TYPE_WORKSPACE_PREVIEW_ENTRY             (deepin_workspace_preview_entry_get_type ())
#define DEEPIN_WORKSPACE_PREVIEW_ENTRY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEEPIN_TYPE_WORKSPACE_PREVIEW_ENTRY, DeepinWorkspacePreviewEntry))
#define DEEPIN_WORKSPACE_PREVIEW_ENTRY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), DEEPIN_TYPE_WORKSPACE_PREVIEW_ENTRY, DeepinWorkspacePreviewEntryClass))
#define DEEPIN_IS_WORKSPACE_PREVIEW_ENTRY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEEPIN_TYPE_WORKSPACE_PREVIEW_ENTRY))
#define DEEPIN_IS_WORKSPACE_PREVIEW_ENTRY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), DEEPIN_TYPE_WORKSPACE_PREVIEW_ENTRY))
#define DEEPIN_WORKSPACE_PREVIEW_ENTRY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), DEEPIN_TYPE_WORKSPACE_PREVIEW_ENTRY, DeepinWorkspacePreviewEntryClass))

typedef struct _DeepinWorkspacePreviewEntryClass DeepinWorkspacePreviewEntryClass;
typedef struct _DeepinWorkspacePreviewEntry DeepinWorkspacePreviewEntry;
typedef struct _DeepinWorkspacePreviewEntryPrivate DeepinWorkspacePreviewEntryPrivate;


struct _DeepinWorkspacePreviewEntryClass
{
	DeepinFixedClass parent_class;
};

struct _DeepinWorkspacePreviewEntry
{
	DeepinFixed parent_instance;

	DeepinWorkspacePreviewEntryPrivate *priv;
};

GType deepin_workspace_preview_entry_get_type (void) G_GNUC_CONST;
GtkWidget* deepin_workspace_preview_entry_new(MetaWorkspace *);
void deepin_workspace_preview_entry_set_select(DeepinWorkspacePreviewEntry*, gboolean);
MetaWorkspace* deepin_workspace_preview_entry_get_workspace(DeepinWorkspacePreviewEntry*);


G_END_DECLS

#endif /* _DEEPIN_WORKSPACE_PREVIEW_ENTRY_H_ */


