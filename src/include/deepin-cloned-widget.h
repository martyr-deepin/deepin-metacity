/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DEEPIN_CLONED_WIDGET_H
#define DEEPIN_CLONED_WIDGET_H

#include <gtk/gtk.h>
#include "../core/window-private.h"

#define META_TYPE_DEEPIN_CLONED_WIDGET         (meta_deepin_cloned_widget_get_type ())
#define META_DEEPIN_CLONED_WIDGET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), META_TYPE_DEEPIN_CLONED_WIDGET, MetaDeepinClonedWidget))
#define META_DEEPIN_CLONED_WIDGET_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    META_TYPE_DEEPIN_CLONED_WIDGET, MetaDeepinClonedWidgetClass))
#define META_IS_DEEPIN_CLONED_WIDGET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), META_TYPE_DEEPIN_CLONED_WIDGET))
#define META_IS_DEEPIN_CLONED_WIDGET_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    META_TYPE_DEEPIN_CLONED_WIDGET))
#define META_DEEPIN_CLONED_WIDGET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  META_TYPE_DEEPIN_CLONED_WIDGET, MetaDeepinClonedWidgetClass))

typedef struct _MetaDeepinClonedWidget        MetaDeepinClonedWidget;
typedef struct _MetaDeepinClonedWidgetClass   MetaDeepinClonedWidgetClass;
typedef struct _MetaDeepinClonedWidgetPrivate MetaDeepinClonedWidgetPrivate;

struct _MetaDeepinClonedWidget
{
  GtkWidget            parent;
  MetaDeepinClonedWidgetPrivate *priv;
};

struct _MetaDeepinClonedWidgetClass
{
  GtkWidgetClass parent_class;
};

enum {
    DRAG_TARGET_WINDOW,
};

GType meta_deepin_cloned_widget_get_type(void) G_GNUC_CONST;
GtkWidget *meta_deepin_cloned_widget_new(MetaWindow*);
void meta_deepin_cloned_widget_select(MetaDeepinClonedWidget *);
void meta_deepin_cloned_widget_unselect(MetaDeepinClonedWidget *);

gboolean meta_deepin_cloned_widget_is_mouse_over(MetaDeepinClonedWidget*);

/* for animation */
void meta_deepin_cloned_widget_set_scale(MetaDeepinClonedWidget*, gdouble, gdouble);
void meta_deepin_cloned_widget_get_scale(MetaDeepinClonedWidget*, gdouble*, gdouble*);
void meta_deepin_cloned_widget_set_scale_x(MetaDeepinClonedWidget*, gdouble);
void meta_deepin_cloned_widget_set_scale_y(MetaDeepinClonedWidget*, gdouble);

void meta_deepin_cloned_widget_set_blur_radius(MetaDeepinClonedWidget*, gdouble);

void meta_deepin_cloned_widget_set_alpha(MetaDeepinClonedWidget*, gdouble);
gdouble meta_deepin_cloned_widget_get_alpha(MetaDeepinClonedWidget*);

void meta_deepin_cloned_widget_set_rotate(MetaDeepinClonedWidget*, gdouble);
gdouble meta_deepin_cloned_widget_get_rotate(MetaDeepinClonedWidget*);

void meta_deepin_cloned_widget_translate(MetaDeepinClonedWidget*, gdouble, gdouble);
void meta_deepin_cloned_widget_get_translate(MetaDeepinClonedWidget*, gdouble*, gdouble*);
void meta_deepin_cloned_widget_translate_x(MetaDeepinClonedWidget*, gdouble);
void meta_deepin_cloned_widget_translate_y(MetaDeepinClonedWidget*, gdouble);

void meta_deepin_cloned_widget_set_size(MetaDeepinClonedWidget*, gdouble, gdouble);
void meta_deepin_cloned_widget_get_size(MetaDeepinClonedWidget*, gdouble*, gdouble*);

void meta_deepin_cloned_widget_set_render_frame(MetaDeepinClonedWidget*, gboolean);

/* setup animaiton */
void meta_deepin_cloned_widget_push_state(MetaDeepinClonedWidget*);
/* start animation up */
void meta_deepin_cloned_widget_pop_state(MetaDeepinClonedWidget*);

cairo_surface_t* meta_deepin_cloned_widget_get_snapshot(MetaDeepinClonedWidget*);
MetaWindow* meta_deepin_cloned_widget_get_window(MetaDeepinClonedWidget*);

GdkWindow* meta_deepin_cloned_widget_get_event_window(MetaDeepinClonedWidget*);

/* sensitive makes clone responsive to events */
void meta_deepin_cloned_widget_set_sensitive(MetaDeepinClonedWidget*, gboolean);

#endif
