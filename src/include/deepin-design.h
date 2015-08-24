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

#include <gtk/gtk.h>

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
#define SWITCHER_PREVIEW_DURATION 280

#define SWITCHER_COLUMN_SPACING  20
#define SWITCHER_ROW_SPACING  20
#define SWITCHER_MIN_ITEMS_EACH_ROW  7
#define SWITCHER_MAX_ROWS  2
 
static const int ANIMATION_DURATION = 250;

/**
 * The percent value between workspace clones' horizontal offset and monitor's height.
 */
static const float HORIZONTAL_OFFSET_PERCENT = 0.03f;

/**
 * The percent value between flow workspace's top offset and monitor's height.
 */
static const float FLOW_CLONE_TOP_OFFSET_PERCENT = 0.24f;

/**
 * The percent value between distance of flow workspaces and its width.
 */
static const float FLOW_CLONE_DISTANCE_PERCENT = 0.10f;


/* WorkspaceThumbClone */
static const int WORKSPACE_NAME_WIDTH = 70;
static const int WORKSPACE_NAME_HEIGHT = 8;  // will pluse NAME_SHAPE_PADDING * 2 when using
static const int WORKSPACE_NAME_MAX_LENGTH = 32;

// distance between thumbnail workspace clone and workspace name field
static const int WORKSPACE_NAME_DISTANCE = 16;

// layout spacing for workspace name field
static const int WORKSPACE_NAME_SPACING = 5;

static const int THUMB_SHAPE_PADDING = 2;
static const int NAME_SHAPE_PADDING = 8;


/* WorkspaceThumbCloneContainer */
static const float WORKSPACE_WIDTH_PERCENT = 0.12f;

/**
 * The percent value between distance of thumbnail workspace clones and monitor's width.
 */
static const float SPACING_PERCENT = 0.02f;

static const int CLOSE_BUTTON_SIZE = 31;

//const int ANIMATION_DURATION = 500;
//const AnimationMode ANIMATION_MODE = AnimationMode.EASE_OUT_QUAD;


void calculate_preferred_size(gint entry_count, gint max_width,
        float* box_width, float* box_height, float* item_width,
        float* item_height, int* max_items_each_row);

GtkCssProvider* deepin_get_default_css_provider();

void deepin_setup_style_class(GtkWidget* widget, const char* class_name);

#endif

