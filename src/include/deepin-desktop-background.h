/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef _DEEPIN_DESKTOP_BACKGROUND_H_
#define _DEEPIN_DESKTOP_BACKGROUND_H_

#include <config.h>
#include <gtk/gtk.h>
#include "types.h"

G_BEGIN_DECLS

#define DEEPIN_TYPE_DESKTOP_BACKGROUND             (deepin_desktop_background_get_type ())
#define DEEPIN_DESKTOP_BACKGROUND(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEEPIN_TYPE_DESKTOP_BACKGROUND, DeepinDesktopBackground))
#define DEEPIN_DESKTOP_BACKGROUND_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), DEEPIN_TYPE_DESKTOP_BACKGROUND, DeepinDesktopBackgroundClass))
#define DEEPIN_IS_DESKTOP_BACKGROUND(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEEPIN_TYPE_DESKTOP_BACKGROUND))
#define DEEPIN_IS_DESKTOP_BACKGROUND_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), DEEPIN_TYPE_DESKTOP_BACKGROUND))
#define DEEPIN_DESKTOP_BACKGROUND_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), DEEPIN_TYPE_DESKTOP_BACKGROUND, DeepinDesktopBackgroundClass))

typedef struct _DeepinDesktopBackgroundClass DeepinDesktopBackgroundClass;
typedef struct _DeepinDesktopBackground DeepinDesktopBackground;
typedef struct _DeepinDesktopBackgroundPrivate DeepinDesktopBackgroundPrivate;


struct _DeepinDesktopBackgroundClass
{
	GtkWindowClass parent_class;
};

struct _DeepinDesktopBackground
{
	GtkWindow parent_instance;

	DeepinDesktopBackgroundPrivate *priv;
};

GType deepin_desktop_background_get_type (void) G_GNUC_CONST;
DeepinDesktopBackground* deepin_desktop_background_new(MetaScreen*);

G_END_DECLS

#endif /* _DEEPIN_DESKTOP_BACKGROUND_H_ */

