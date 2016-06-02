/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/
#ifndef DEEPIN_MENU_H
#define DEEPIN_MENU_H

#include <gtk/gtk.h>
#include "frames.h"

struct _DeepinWindowMenu
{
    MetaFrames *frames;
    Window client_xwindow;
    GtkWidget *menu;
    MetaWindowMenuFunc func;
    gpointer data;
    MetaMenuOp ops;
    MetaMenuOp insensitive;
    GDBusProxy* manager;
    GDBusProxy* menu_interface;
};

DeepinWindowMenu* deepin_window_menu_new  (MetaFrames         *frames,
                                           MetaMenuOp          ops,
                                           MetaMenuOp          insensitive,
                                           Window              client_xwindow,
                                           unsigned long       active_workspace,
                                           int                 n_workspaces,
                                           MetaWindowMenuFunc  func,
                                           gpointer            data);
void            deepin_window_menu_popup  (DeepinWindowMenu     *menu,
                                           int                 root_x,
                                           int                 root_y,
                                           int                 button,
                                           guint32             timestamp);
void            deepin_window_menu_free   (DeepinWindowMenu     *menu);


#endif

