/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef _DEEPIN_WORKSPACE_ADDER_H_
#define _DEEPIN_WORKSPACE_ADDER_H_

#include <gtk/gtk.h>
#include <prefs.h>

G_BEGIN_DECLS

#define DEEPIN_TYPE_WORKSPACE_ADDER             (deepin_workspace_adder_get_type ())
#define DEEPIN_WORKSPACE_ADDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEEPIN_TYPE_WORKSPACE_ADDER, DeepinWorkspaceAdder))
#define DEEPIN_WORKSPACE_ADDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), DEEPIN_TYPE_WORKSPACE_ADDER, DeepinWorkspaceAdderClass))
#define DEEPIN_IS_WORKSPACE_ADDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEEPIN_TYPE_WORKSPACE_ADDER))
#define DEEPIN_IS_WORKSPACE_ADDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), DEEPIN_TYPE_WORKSPACE_ADDER))
#define DEEPIN_WORKSPACE_ADDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), DEEPIN_TYPE_WORKSPACE_ADDER, DeepinWorkspaceAdderClass))

typedef struct _DeepinWorkspaceAdderClass DeepinWorkspaceAdderClass;
typedef struct _DeepinWorkspaceAdder DeepinWorkspaceAdder;
typedef struct _DeepinWorkspaceAdderPrivate DeepinWorkspaceAdderPrivate;


struct _DeepinWorkspaceAdderClass
{
	GtkEventBoxClass parent_class;
};

struct _DeepinWorkspaceAdder
{
	GtkEventBox parent_instance;

	DeepinWorkspaceAdderPrivate *priv;
};

GType deepin_workspace_adder_get_type (void) G_GNUC_CONST;
GtkWidget* deepin_workspace_adder_new();

G_END_DECLS

#endif /* _DEEPIN_WORKSPACE_ADDER_H_ */

