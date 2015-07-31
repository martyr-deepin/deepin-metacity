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

#ifndef TAP_POPUP_PRIVATE_H
#define TAP_POPUP_PRIVATE_H

#include "tabpopup.h"

typedef struct _TabEntry TabEntry;
typedef struct _MetaDeepinSwitchPreviewer        MetaDeepinSwitchPreviewer;

struct _TabEntry
{
  MetaTabEntryKey  key;
  char            *title;
  GdkPixbuf       *icon, *dimmed_icon;
  GtkWidget       *widget;
  GdkRectangle     rect;
  GdkRectangle     inner_rect;
  guint blank : 1;
};

struct _MetaTabPopup
{
  GtkWidget *window;
  GList *current;
  GList *entries;
  TabEntry *current_selected_entry;
  GtkWidget *outline_window;
  MetaDeepinSwitchPreviewer* previewer;
  gboolean outline;
};

#endif


