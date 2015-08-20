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

#ifndef DEEPIN_TAB_WIDGET_H
#define DEEPIN_TAB_WIDGET_H

#include <gtk/gtk.h>
#include <cairo.h>

#define META_TYPE_DEEPIN_TAB_WIDGET         (meta_deepin_tab_widget_get_type ())
#define META_DEEPIN_TAB_WIDGET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), META_TYPE_DEEPIN_TAB_WIDGET, MetaDeepinTabWidget))
#define META_DEEPIN_TAB_WIDGET_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    META_TYPE_DEEPIN_TAB_WIDGET, MetaDeepinTabWidgetClass))
#define META_IS_DEEPIN_TAB_WIDGET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), META_TYPE_DEEPIN_TAB_WIDGET))
#define META_IS_DEEPIN_TAB_WIDGET_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    META_TYPE_DEEPIN_TAB_WIDGET))
#define META_DEEPIN_TAB_WIDGET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  META_TYPE_DEEPIN_TAB_WIDGET, MetaDeepinTabWidgetClass))

typedef struct _MetaDeepinTabWidget        MetaDeepinTabWidget;
typedef struct _MetaDeepinTabWidgetClass   MetaDeepinTabWidgetClass;
typedef struct _MetaDeepinTabWidgetPrivate MetaDeepinTabWidgetPrivate;

struct _MetaDeepinTabWidget
{
  GtkWidget            parent;
  MetaDeepinTabWidgetPrivate *priv;
};

struct _MetaDeepinTabWidgetClass
{
  GtkWidgetClass parent_class;
};

GType      meta_deepin_tab_widget_get_type (void) G_GNUC_CONST;
GtkWidget *meta_deepin_tab_widget_new      (cairo_surface_t*);
void       meta_deepin_tab_widget_select   (MetaDeepinTabWidget *);
void       meta_deepin_tab_widget_unselect (MetaDeepinTabWidget *);
void       meta_deepin_tab_widget_set_scale(MetaDeepinTabWidget*, gdouble);
gdouble    meta_deepin_tab_widget_get_scale(MetaDeepinTabWidget*);

#endif
