/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* deepin custom keybindings */

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

#ifndef DEEPIN_META_KEYBINDINGS_H
#define DEEPIN_META_KEYBINDINGS_H

#include <display.h>
#include <screen.h>
#include "keybindings.h"
#include "window-private.h"

void deepin_init_custom_handlers(MetaDisplay* display);
void do_preview_workspace(MetaDisplay *display, MetaScreen *screen,
        MetaWindow *window, guint32 timestamp,
        MetaKeyBinding *binding, gpointer user_data, 
        gboolean user_op);

#endif

