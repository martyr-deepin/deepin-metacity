/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-message-hub.h
 * Copyright (C) 2015 Sian Cao <yinshuiboy@gmail.com>
 *
 * deepin metacity is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * deepin metacity is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DEEPIN_MESSAGE_HUB_H_
#define _DEEPIN_MESSAGE_HUB_H_

#include <config.h>
#include <gtk/gtk.h>
#include "types.h"
#include "boxes.h"
#include "../core/workspace.h"

G_BEGIN_DECLS

#define DEEPIN_TYPE_MESSAGE_HUB             (deepin_message_hub_get_type ())
#define DEEPIN_MESSAGE_HUB(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEEPIN_TYPE_MESSAGE_HUB, DeepinMessageHub))
#define DEEPIN_MESSAGE_HUB_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), DEEPIN_TYPE_MESSAGE_HUB, DeepinMessageHubClass))
#define DEEPIN_IS_MESSAGE_HUB(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEEPIN_TYPE_MESSAGE_HUB))
#define DEEPIN_IS_MESSAGE_HUB_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), DEEPIN_TYPE_MESSAGE_HUB))
#define DEEPIN_MESSAGE_HUB_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), DEEPIN_TYPE_MESSAGE_HUB, DeepinMessageHubClass))

typedef struct _DeepinMessageHubClass DeepinMessageHubClass;
typedef struct _DeepinMessageHub DeepinMessageHub;
typedef struct _DeepinMessageHubPrivate DeepinMessageHubPrivate;


struct _DeepinMessageHubClass
{
	GObjectClass parent_class;
};

struct _DeepinMessageHub
{
	GObject parent_instance;

	DeepinMessageHubPrivate *priv;
};

GType deepin_message_hub_get_type (void) G_GNUC_CONST;

/* singleton */
DeepinMessageHub* deepin_message_hub_get();

void deepin_message_hub_window_removed(MetaWindow*);
void deepin_message_hub_window_added(MetaWindow*);
void deepin_message_hub_window_damaged(MetaWindow*, XRectangle*, int);
void deepin_message_hub_desktop_changed(void);
void deepin_message_hub_screen_resized(MetaScreen* screen);
void deepin_message_hub_window_about_to_change_workspace(MetaWindow*, MetaWorkspace*);
void deepin_message_hub_drag_end(void);

G_END_DECLS

#endif /* _DEEPIN_MESSAGE_HUB_H_ */

