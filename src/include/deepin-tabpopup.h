/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity tab popup window */


/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef DEEPIN_TABPOPUP_H
#define DEEPIN_TABPOPUP_H

/* Don't include gtk.h or gdk.h here */
#include "common.h"
#include "boxes.h"
#include <X11/Xlib.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "tabpopup.h" /* for MetaTabEntry def */

typedef struct _DeepinTabEntry DeepinTabEntry;
typedef struct _DeepinTabPopup DeepinTabPopup;
typedef void *DeepinTabEntryKey;

typedef struct _MetaDeepinSwitchPreviewer        MetaDeepinSwitchPreviewer;

struct _DeepinTabEntry
{
  DeepinTabEntryKey  key;
  char            *title;
  GtkWidget       *widget;
  GdkRectangle     rect;
  GdkRectangle     inner_rect;
  guint blank : 1;
};

struct _DeepinTabPopup
{
  GtkWidget *window;
  GList *current;
  GList *entries;
  DeepinTabEntry *current_selected_entry;
  GtkWidget *outline_window;
  int max_width;
  MetaDeepinSwitchPreviewer* previewer;
  GList *idle_relayout_ids;
};

DeepinTabPopup*   deepin_tab_popup_new          (const MetaTabEntry *entries,
                                                int                 screen_number,
                                                int                 entry_count,
                                                gboolean            show_desktop);
void            deepin_tab_popup_free         (DeepinTabPopup       *popup);
void            deepin_tab_popup_set_showing  (DeepinTabPopup       *popup,
                                                gboolean            showing);
void            deepin_tab_popup_forward      (DeepinTabPopup       *popup);
void            deepin_tab_popup_backward     (DeepinTabPopup       *popup);
DeepinTabEntryKey deepin_tab_popup_get_selected (DeepinTabPopup      *popup);
void            deepin_tab_popup_select       (DeepinTabPopup       *popup,
                                                DeepinTabEntryKey     key);


#endif

