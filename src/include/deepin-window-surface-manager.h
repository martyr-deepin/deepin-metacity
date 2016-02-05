/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef _DEEPIN_WINDOW_SURFACE_MANAGER_H_
#define _DEEPIN_WINDOW_SURFACE_MANAGER_H_

#include <gtk/gtk.h>
#include <cairo.h>
#include "types.h"

G_BEGIN_DECLS

#define DEEPIN_TYPE_WINDOW_SURFACE_MANAGER             (deepin_window_surface_manager_get_type ())
#define DEEPIN_WINDOW_SURFACE_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEEPIN_TYPE_WINDOW_SURFACE_MANAGER, DeepinWindowSurfaceManager))
#define DEEPIN_WINDOW_SURFACE_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), DEEPIN_TYPE_WINDOW_SURFACE_MANAGER, DeepinWindowSurfaceManagerClass))
#define DEEPIN_IS_WINDOW_SURFACE_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEEPIN_TYPE_WINDOW_SURFACE_MANAGER))
#define DEEPIN_IS_WINDOW_SURFACE_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), DEEPIN_TYPE_WINDOW_SURFACE_MANAGER))
#define DEEPIN_WINDOW_SURFACE_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), DEEPIN_TYPE_WINDOW_SURFACE_MANAGER, DeepinWindowSurfaceManagerClass))

typedef struct _DeepinWindowSurfaceManagerClass DeepinWindowSurfaceManagerClass;
typedef struct _DeepinWindowSurfaceManager DeepinWindowSurfaceManager;
typedef struct _DeepinWindowSurfaceManagerPrivate DeepinWindowSurfaceManagerPrivate;


struct _DeepinWindowSurfaceManagerClass
{
	GObjectClass parent_class;
};

struct _DeepinWindowSurfaceManager
{
	GObject parent_instance;

	DeepinWindowSurfaceManagerPrivate *priv;
};

GType deepin_window_surface_manager_get_type (void) G_GNUC_CONST;

/* skeleton */
DeepinWindowSurfaceManager* deepin_window_surface_manager_get(void);

/* get surface from window at scale */
cairo_surface_t* deepin_window_surface_manager_get_surface(MetaWindow*, double);

/* get combined surface of two windows, second is over first
 * the returned surafce inherit properties (format and size) of first parameter
 */
cairo_surface_t* deepin_window_surface_manager_get_combined_surface(
        MetaWindow*, MetaWindow*, int, int, double);

cairo_surface_t* deepin_window_surface_manager_get_combined3(
        cairo_surface_t*,
        cairo_surface_t*, int, int, 
        cairo_surface_t*, int, int, 
        double);

/* clear surface for window */
void deepin_window_surface_manager_remove_window(MetaWindow*);

/* remove all caches inorder to get updated window preview */
/* need better way, e.g automatic idle update */
void deepin_window_surface_manager_flush();

G_END_DECLS

#endif /* _DEEPIN_WINDOW_SURFACE_MANAGER_H_ */

