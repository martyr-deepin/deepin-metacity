/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */


/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef __DEEPIN_STATED_IMAGE_H__
#define __DEEPIN_STATED_IMAGE_H__

#include <gtk/gtk.h>


G_BEGIN_DECLS

#define DEEPIN_TYPE_STATED_IMAGE                  (deepin_stated_image_get_type ())
#define DEEPIN_STATED_IMAGE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEEPIN_TYPE_STATED_IMAGE, DeepinStatedImage))
#define DEEPIN_STATED_IMAGE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), DEEPIN_TYPE_STATED_IMAGE, DeepinStatedImageClass))
#define DEEPIN_IS_STATED_IMAGE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEEPIN_TYPE_STATED_IMAGE))
#define DEEPIN_IS_STATED_IMAGE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), DEEPIN_TYPE_STATED_IMAGE))
#define DEEPIN_STATED_IMAGE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), DEEPIN_TYPE_STATED_IMAGE, DeepinStatedImageClass))


typedef struct _DeepinStatedImage              DeepinStatedImage;
typedef struct _DeepinStatedImagePrivate       DeepinStatedImagePrivate;
typedef struct _DeepinStatedImageClass         DeepinStatedImageClass;
typedef enum _DeepinStatedImageState           DeepinStatedImageState;

struct _DeepinStatedImage
{
  GtkEventBox box;

  /*< private >*/
  DeepinStatedImagePrivate *priv;
};

struct _DeepinStatedImageClass
{
  GtkEventBoxClass parent_class;

  /* Padding for future expansion */
  void (*_deepin_reserved1) (void);
  void (*_deepin_reserved2) (void);
  void (*_deepin_reserved3) (void);
  void (*_deepin_reserved4) (void);
};

enum _DeepinStatedImageState
{
    DSINormal,
    DSIPrelight,
    DSIPressed
};

GType      deepin_stated_image_get_type (void) G_GNUC_CONST;

GtkWidget* deepin_stated_image_new_from_file      (const gchar     *filename);

G_END_DECLS

#endif /* __DEEPIN_STATED_IMAGE_H__ */

