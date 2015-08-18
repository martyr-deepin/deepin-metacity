/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-window-surface-manager.h
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * metacity is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * metacity is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
/* clear surface for window */
void deepin_window_surface_manager_remove_window(MetaWindow*);

/* remove all caches inorder to get updated window preview */
/* need better way, e.g automatic idle update */
void deepin_window_surface_manager_flush();

G_END_DECLS

#endif /* _DEEPIN_WINDOW_SURFACE_MANAGER_H_ */

