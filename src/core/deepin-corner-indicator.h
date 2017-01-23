/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef _DEEPIN_CORNER_INDICATOR_H_
#define _DEEPIN_CORNER_INDICATOR_H_

#include <config.h>
#include <gtk/gtk.h>
#include "screen-private.h"

G_BEGIN_DECLS

#define DEEPIN_TYPE_CORNER_INDICATOR             (deepin_corner_indicator_get_type ())
#define DEEPIN_CORNER_INDICATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEEPIN_TYPE_CORNER_INDICATOR, DeepinCornerIndicator))
#define DEEPIN_CORNER_INDICATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), DEEPIN_TYPE_CORNER_INDICATOR, DeepinCornerIndicatorClass))
#define DEEPIN_IS_CORNER_INDICATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEEPIN_TYPE_CORNER_INDICATOR))
#define DEEPIN_IS_CORNER_INDICATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), DEEPIN_TYPE_CORNER_INDICATOR))
#define DEEPIN_CORNER_INDICATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), DEEPIN_TYPE_CORNER_INDICATOR, DeepinCornerIndicatorClass))

typedef struct _DeepinCornerIndicatorClass DeepinCornerIndicatorClass;
typedef struct _DeepinCornerIndicator DeepinCornerIndicator;
typedef struct _DeepinCornerIndicatorPrivate DeepinCornerIndicatorPrivate;


struct _DeepinCornerIndicatorClass
{
	GtkWindowClass parent_class;
};

struct _DeepinCornerIndicator
{
	GtkWindow parent_instance;

	DeepinCornerIndicatorPrivate *priv;
};

GType deepin_corner_indicator_get_type (void) G_GNUC_CONST;

GtkWidget* deepin_corner_indicator_new (MetaScreen *, MetaScreenCorner, const char*, int, int);

G_END_DECLS

#endif /* _DEEPIN_CORNER_INDICATOR_H_ */

