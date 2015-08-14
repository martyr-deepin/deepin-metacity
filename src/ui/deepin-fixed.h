/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef DEEPIN_FIXED_H
#define DEEPIN_FIXED_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DEEPIN_TYPE_FIXED                  (deepin_fixed_get_type ())
#define DEEPIN_FIXED(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEEPIN_TYPE_FIXED, DeepinFixed))
#define DEEPIN_FIXED_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), DEEPIN_TYPE_FIXED, DeepinFixedClass))
#define DEEPIN_IS_FIXED(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEEPIN_TYPE_FIXED))
#define DEEPIN_IS_FIXED_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), DEEPIN_TYPE_FIXED))
#define DEEPIN_FIXED_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), DEEPIN_TYPE_FIXED, DeepinFixedClass))

typedef struct _DeepinFixed              DeepinFixed;
typedef struct _DeepinFixedPrivate       DeepinFixedPrivate;
typedef struct _DeepinFixedClass         DeepinFixedClass;
typedef struct _DeepinFixedChild         DeepinFixedChild;

struct _DeepinFixed
{
  GtkContainer container;

  /*< private >*/
  DeepinFixedPrivate *priv;
};

struct _DeepinFixedClass
{
  GtkContainerClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

typedef struct _ChildAnimationInfo
{
    DeepinFixedChild* child;
    gint old_x;
    gint old_y;
    gint target_x;
    gint target_y;

    gboolean animation; /* in animation */

    gdouble current_pos;
    gdouble target_pos;

    gint64 start_time;
    gint64 last_time;
    gint64 end_time;

    guint tick_id;
} ChildAnimationInfo;

struct _DeepinFixedChild
{
  GtkWidget *widget;
  gint x;
  gint y;
};


GDK_AVAILABLE_IN_ALL
GType      deepin_fixed_get_type          (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GtkWidget* deepin_fixed_new               ();

/* x, y is considered center of widget, we need widget stand still around center */
GDK_AVAILABLE_IN_ALL
void       deepin_fixed_put               (DeepinFixed       *fixed,
                                        GtkWidget      *widget,
                                        gint            x,
                                        gint            y);
GDK_AVAILABLE_IN_ALL
void       deepin_fixed_move              (DeepinFixed       *fixed,
                                        GtkWidget      *widget,
                                        gint            x,
                                        gint            y, 
                                        gboolean        animate);


G_END_DECLS

#endif
