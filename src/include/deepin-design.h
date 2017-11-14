/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */


/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef DEEPIN_DESIGN_H
#define DEEPIN_DESIGN_H

#include <gtk/gtk.h>
#include "window.h"

/*
 * this file contains some design constants by deepin
 * these constants come from deepin-wm for consistency 
 */
void deepin_switcher_get_prefer_size(int *width, int *height);


/**
 * Prefer size for the inner item's rectangle.
 */
void deepin_switcher_get_inner_prefer_size(int *width, int *height);

#define POPUP_DELAY_TIMEOUT 150 /* milliseconds, keep popup window hidden when clicked alt-tab quickly*/
#define MIN_DELTA           100 /* milliseconds, repeat key pressing minimum delta time after popup shown */
#define POPUP_SCREEN_PADDING 20
#define POPUP_PADDING        36

#define SWITCHER_SELECT_ANIMATION_DURATION 280
#define SWITCHER_PREVIEW_DURATION 280

#define SWITCHER_ROW_SPACING  10
#define SWITCHER_MIN_ITEMS_EACH_ROW  7
#define SWITCHER_MAX_ROWS  2
 
static const int ANIMATION_DURATION = 250;

/**
 * The percent value between workspace clones' horizontal offset and monitor's height.
 */
static const float HORIZONTAL_OFFSET_PERCENT = 0.044f;

/**
 * The percent value between flow workspace's top offset and monitor's height.
 */
static const float FLOW_CLONE_TOP_OFFSET_PERCENT = 0.211f;

/**
 * The percent value between distance of flow workspaces and its width.
 */
static const float FLOW_CLONE_DISTANCE_PERCENT = 0.078f;

static const int THUMB_SHAPE_PADDING = 2;


/* WorkspaceThumbCloneContainer */
static const float WORKSPACE_WIDTH_PERCENT = 0.12f;

/**
 * The percent value between distance of thumbnail workspace clones and monitor's width.
 */
static const float SPACING_PERCENT = 0.02f;

static const int CLOSE_BUTTON_SIZE = 31;

//const int ANIMATION_DURATION = 500;
//const AnimationMode ANIMATION_MODE = AnimationMode.EASE_OUT_QUAD;

static const int MAX_WORKSPACE_NUM = 7; 

/**
 * for workspace indicator
 */
static const float DWI_WORKSPACE_SCALE = 0.10f;

/**
 * The distance measure in percentage of the monitor width between workspaces preview
 */
static const float DWI_SPACING_PERCENT = 0.0156f;

static const int DWI_MARGIN_HORIZONTAL = 22;
static const int DWI_MARGIN_VERTICAL   = 21;

// drag targets for cloned widget or workspace
enum {
    DRAG_TARGET_WINDOW = 1,
    DRAG_TARGET_WORKSPACE,
};

void calculate_preferred_size(gint entry_count, gint max_width,
        float* box_width, float* box_height, float* item_width,
        float* item_height, int* max_items_each_row);

GtkCssProvider* deepin_get_default_css_provider(void);

void deepin_setup_style_class(GtkWidget* widget, const char* class_name);
GdkPixbuf* meta_window_get_application_icon(MetaWindow* window, int icon_size);

#endif

