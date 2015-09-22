/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-timeline.h
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

#ifndef _DEEPIN_TIMELINE_H_
#define _DEEPIN_TIMELINE_H_

#include <glib-object.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define DEEPIN_TYPE_TIMELINE             (deepin_timeline_get_type ())
#define DEEPIN_TIMELINE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEEPIN_TYPE_TIMELINE, DeepinTimeline))
#define DEEPIN_TIMELINE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), DEEPIN_TYPE_TIMELINE, DeepinTimelineClass))
#define DEEPIN_IS_TIMELINE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEEPIN_TYPE_TIMELINE))
#define DEEPIN_IS_TIMELINE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), DEEPIN_TYPE_TIMELINE))
#define DEEPIN_TIMELINE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), DEEPIN_TYPE_TIMELINE, DeepinTimelineClass))

typedef struct _DeepinTimelineClass DeepinTimelineClass;
typedef struct _DeepinTimeline DeepinTimeline;
typedef struct _DeepinTimelinePrivate DeepinTimelinePrivate;


struct _DeepinTimelineClass
{
    GObjectClass parent_class;

    /*< public >*/
    void (*started)     (DeepinTimeline *timeline);
    void (*stopped)     (DeepinTimeline *timeline);
    void (*paused)      (DeepinTimeline *timeline);
    void (*new_frame)   (DeepinTimeline *timeline, double pos);
};

struct _DeepinTimeline
{
    GObject parent_instance;

    DeepinTimelinePrivate *priv;
};

GType deepin_timeline_get_type (void) G_GNUC_CONST;


DeepinTimeline* deepin_timeline_new(void);

/* in milliseconds */
void deepin_timeline_set_duration(DeepinTimeline*, guint);
guint deepin_timeline_get_duration(DeepinTimeline*);

void deepin_timeline_set_repeat_count(DeepinTimeline*, gint);
gint deepin_timeline_get_repeat_count(DeepinTimeline*);

void deepin_timeline_set_delay(DeepinTimeline*, guint);
guint deepin_timeline_get_delay(DeepinTimeline*);

enum DeepinAnimationMode {
    DEEPIN_LINEAR,
    DEEPIN_EASE_IN_OUT_QUAD,
    DEEPIN_EASE_OUT_CUBIC,
    DEEPIN_EASE_OUT_QUAD,

    DEEPIN_N_ANIMATION_MODE
};

void deepin_timeline_set_progress_mode(DeepinTimeline*, enum DeepinAnimationMode);
enum DeepinAnimationMode deepin_timeline_get_progress_mode(DeepinTimeline*);

gboolean deepin_timeline_is_playing(DeepinTimeline*);

void deepin_timeline_set_clock(DeepinTimeline*, GdkFrameClock*);

void deepin_timeline_start(DeepinTimeline*);
void deepin_timeline_stop(DeepinTimeline*);
/*FIXME: do not impl yet */
void deepin_timeline_pause(DeepinTimeline*);

G_END_DECLS

#endif /* _DEEPIN_TIMELINE_H_ */

