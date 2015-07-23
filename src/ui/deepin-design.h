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

#ifndef DEEPIN_DESIGN_H
#define DEEPIN_DESIGN_H

/*
 * this file contains some design constants by deepin
 * these constants come from deepin-wm for consistency 
 */
#define SWITCHER_ITEM_PREFER_WIDTH  300
#define SWITCHER_ITEM_PREFER_HEIGHT 200
#define SWITCHER_ITEM_SHAPE_PADDING 10

/**
 * Prefer size for the inner item's rectangle.
 */
#define RECT_PREFER_WIDTH  (SWITCHER_ITEM_PREFER_WIDTH - SWITCHER_ITEM_SHAPE_PADDING * 2)
#define RECT_PREFER_HEIGHT (SWITCHER_ITEM_PREFER_HEIGHT - SWITCHER_ITEM_SHAPE_PADDING * 2)

#define POPUP_DELAY_TIMEOUT 150 /* milliseconds, keep popup window hidden when clicked alt-tab quickly*/
#define MIN_DELTA           100 /* milliseconds, repeat key pressing minimum delta time after popup shown */
#define POPUP_SCREEN_PADDING 20
#define POPUP_PADDING        36

#define SWITCHER_SELECT_ANIMATION_DURATION 280
#endif

