/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */


/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef __DEEPIN_ANIMATION_IMAGE_H__
#define __DEEPIN_ANIMATION_IMAGE_H__

#include <gtk/gtk.h>


G_BEGIN_DECLS

#define DEEPIN_TYPE_ANIMATION_IMAGE                  (deepin_animation_image_get_type ())
#define DEEPIN_ANIMATION_IMAGE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEEPIN_TYPE_ANIMATION_IMAGE, DeepinAnimationImage))
#define DEEPIN_ANIMATION_IMAGE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), DEEPIN_TYPE_ANIMATION_IMAGE, DeepinAnimationImageClass))
#define DEEPIN_IS_ANIMATION_IMAGE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEEPIN_TYPE_ANIMATION_IMAGE))
#define DEEPIN_IS_ANIMATION_IMAGE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), DEEPIN_TYPE_ANIMATION_IMAGE))
#define DEEPIN_ANIMATION_IMAGE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), DEEPIN_TYPE_ANIMATION_IMAGE, DeepinAnimationImageClass))


typedef struct _DeepinAnimationImage              DeepinAnimationImage;
typedef struct _DeepinAnimationImagePrivate       DeepinAnimationImagePrivate;
typedef struct _DeepinAnimationImageClass         DeepinAnimationImageClass;

struct _DeepinAnimationImage
{
  GtkWidget parent;

  /*< private >*/
  DeepinAnimationImagePrivate *priv;
};

struct _DeepinAnimationImageClass
{
  GtkEventBoxClass parent_class;

  /* Padding for future expansion */
  void (*_deepin_reserved1) (void);
  void (*_deepin_reserved2) (void);
  void (*_deepin_reserved3) (void);
  void (*_deepin_reserved4) (void);
};

GType      deepin_animation_image_get_type (void) G_GNUC_CONST;

GtkWidget* deepin_animation_image_new      ();
void deepin_animation_image_activate (DeepinAnimationImage *);
void deepin_animation_image_deactivate (DeepinAnimationImage *);
gboolean deepin_animation_image_get_activated (DeepinAnimationImage *self);

G_END_DECLS

#endif /* __DEEPIN_ANIMATION_IMAGE_H__ */


