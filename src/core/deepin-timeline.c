/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * deepin-timeline.c
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

#include <gdk/gdk.h>
#include "deepin-timeline.h"
#include "deepin-ease.h"

struct _DeepinTimelinePrivate
{
    guint duration;
    guint delay;
    gint repeat_count;
    enum DeepinAnimationMode mode;

    gint playing: 1;
    gint paused: 1;

    GdkFrameClock* clock;

    gint64 start_time;
    gint64 last_time;
    gint64 end_time;

    guint tick_id;
    guint delay_id;
};

enum {
    PROP_0,
    PROP_DURATION,
    PROP_DELAY,
    PROP_REPEAT_COUNT,
    PROP_MODE,
    N_PROPERTIES
};

enum
{
    SIGNAL_STARTED,
    SIGNAL_STOPPED,
    SIGNAL_PAUSED,
    SIGNAL_NEW_FRAME,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static GParamSpec* property_specs[N_PROPERTIES] = {NULL, };
G_DEFINE_TYPE (DeepinTimeline, deepin_timeline, G_TYPE_OBJECT);

static void deepin_timeline_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    DeepinTimeline* self = DEEPIN_TIMELINE(object);

    switch (property_id)
    {
        case PROP_DURATION:
            deepin_timeline_set_duration(self, g_value_get_uint(value));
            break;

        case PROP_DELAY:
            deepin_timeline_set_delay(self, g_value_get_uint(value));
            break;

        case PROP_MODE:
            deepin_timeline_set_progress_mode(self,
                    (enum DeepinAnimationMode)g_value_get_int(value));
            break;

        case PROP_REPEAT_COUNT:
            deepin_timeline_set_repeat_count(self, g_value_get_int(value)); 
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void deepin_timeline_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    DeepinTimeline* self = DEEPIN_TIMELINE(object);
    DeepinTimelinePrivate* priv = self->priv;

    switch (property_id)
    {
        case PROP_DURATION:
            g_value_set_uint(value, priv->duration); break;

        case PROP_DELAY:
            g_value_set_uint(value, priv->delay); break;

        case PROP_REPEAT_COUNT:
            g_value_set_int(value, priv->repeat_count); break;

        case PROP_MODE:
            g_value_set_int(value, priv->mode); break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

struct EaseForMode {
    double (*func)(double);
} _ease_for_mode[] = {
    {deepin_linear},
    {ease_in_out_quad},
    {ease_out_cubic},
    {ease_out_quad},
};

static gboolean on_tick_callback(GdkFrameClock* clock, DeepinTimeline* self);

static void deepin_timeline_stop_animation(DeepinTimeline* self, gboolean abort)
{
    DeepinTimelinePrivate* priv = self->priv;
    if (priv->delay_id) {
        g_source_remove(priv->delay_id);
        priv->delay_id = 0;
    }

    gdk_frame_clock_end_updating(priv->clock);
    g_signal_handlers_disconnect_by_func(priv->clock, on_tick_callback, self);

    priv->playing = FALSE;
    priv->tick_id = 0;

    g_signal_emit(self, signals[SIGNAL_STOPPED], 0);
}

static gboolean on_tick_callback(GdkFrameClock* clock, DeepinTimeline* self)
{
    DeepinTimelinePrivate* priv = self->priv;

    gint64 now = gdk_frame_clock_get_frame_time(clock);

    gdouble duration = (now - priv->last_time) / 1000000.0;
    if (priv->last_time != priv->start_time && duration < 0.033) 
        return G_SOURCE_CONTINUE;
    priv->last_time = now;

    gdouble t = 1.0;
    if (now < priv->end_time) {
        t = (now - priv->start_time) / (double)(priv->end_time - priv->start_time);
    }
    t = _ease_for_mode[priv->mode].func(t);
    g_signal_emit(self, signals[SIGNAL_NEW_FRAME], 0, t);

    if (t >= 1.0) {
        deepin_timeline_stop_animation(self, FALSE);
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

static void deepin_timeline_prepare_animation(DeepinTimeline* self) 
{
    DeepinTimelinePrivate* priv = self->priv;
    g_assert(priv->tick_id == 0); 

    priv->start_time = gdk_frame_clock_get_frame_time(priv->clock);
    priv->last_time = priv->start_time;
    priv->end_time = priv->start_time + (priv->duration * 1000);

    priv->tick_id = g_signal_connect(G_OBJECT(priv->clock),
            "update", (GCallback)on_tick_callback, self);
    gdk_frame_clock_begin_updating(priv->clock);

    priv->playing = TRUE;
    g_signal_emit(self, signals[SIGNAL_STARTED], 0);
}


static void deepin_timeline_init (DeepinTimeline *deepin_timeline)
{
	deepin_timeline->priv = G_TYPE_INSTANCE_GET_PRIVATE (deepin_timeline, DEEPIN_TYPE_TIMELINE, DeepinTimelinePrivate);

	/* TODO: Add initialization code here */
}

static void deepin_timeline_finalize (GObject *object)
{
	/* TODO: Add deinitalization code here */

	G_OBJECT_CLASS (deepin_timeline_parent_class)->finalize (object);
}

static void deepin_timeline_class_init (DeepinTimelineClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DeepinTimelinePrivate));

	object_class->finalize = deepin_timeline_finalize;
    object_class->set_property = deepin_timeline_set_property;
    object_class->get_property = deepin_timeline_get_property;

    property_specs[PROP_DURATION] = g_param_spec_uint(
            "duration", "duration", "duration",
            0, UINT_MAX, 500,
            G_PARAM_READWRITE);

    property_specs[PROP_DELAY] = g_param_spec_uint(
            "delay", "delay", "delay",
            0, UINT_MAX, 0,
            G_PARAM_READWRITE);

    property_specs[PROP_REPEAT_COUNT] = g_param_spec_int(
            "repeat-count", "repeat count", "repeat count",
            INT_MIN, INT_MAX, 1,
            G_PARAM_READWRITE);

    property_specs[PROP_MODE] = g_param_spec_int(
            "mode", "mode", "mode",
            DEEPIN_LINEAR, DEEPIN_N_ANIMATION_MODE, DEEPIN_LINEAR,
            G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, property_specs);

    signals[SIGNAL_STARTED] = g_signal_new ("started",
            DEEPIN_TYPE_TIMELINE,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(DeepinTimelineClass, started),
            NULL, NULL, NULL,
            G_TYPE_NONE, 0, NULL);

    signals[SIGNAL_PAUSED] = g_signal_new ("paused",
            DEEPIN_TYPE_TIMELINE,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(DeepinTimelineClass, paused),
            NULL, NULL, NULL,
            G_TYPE_NONE, 0, NULL);
    
    signals[SIGNAL_NEW_FRAME] = g_signal_new ("new-frame",
            DEEPIN_TYPE_TIMELINE,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(DeepinTimelineClass, new_frame),
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_DOUBLE, NULL);
    
    signals[SIGNAL_STOPPED] = g_signal_new ("stopped",
            DEEPIN_TYPE_TIMELINE,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(DeepinTimelineClass, stopped),
            NULL, NULL, NULL,
            G_TYPE_NONE, 0, NULL);
}

DeepinTimeline* deepin_timeline_new(void)
{
    GObject* o = (GObject*)g_object_new(DEEPIN_TYPE_TIMELINE, NULL);

    return DEEPIN_TIMELINE(o);
}

void deepin_timeline_set_duration(DeepinTimeline* self, guint val)
{
    DeepinTimelinePrivate* priv = self->priv;
    priv->duration = val;
}

guint deepin_timeline_get_duration(DeepinTimeline* self)
{
    return self->priv->duration;
}

void deepin_timeline_set_repeat_count(DeepinTimeline* self, gint val)
{
    self->priv->repeat_count = val;
}

gint deepin_timeline_get_repeat_count(DeepinTimeline* self)
{
    return self->priv->repeat_count;
}

void deepin_timeline_set_delay(DeepinTimeline* self, guint val)
{
    self->priv->delay = val;
}

guint deepin_timeline_get_delay(DeepinTimeline* self)
{
    return self->priv->delay;
}

void deepin_timeline_set_progress_mode(DeepinTimeline* self,
        enum DeepinAnimationMode mode)
{
    self->priv->mode = mode;
}

enum DeepinAnimationMode deepin_timeline_get_progress_mode(DeepinTimeline* self)
{
    return self->priv->mode;
}

gboolean deepin_timeline_is_playing(DeepinTimeline* self)
{
    return self->priv->playing;
}

static gboolean on_timeout_start(DeepinTimeline* self)
{
    DeepinTimelinePrivate* priv = self->priv;

    priv->delay_id = 0;

    deepin_timeline_prepare_animation(self);

    return G_SOURCE_REMOVE;
}

void deepin_timeline_start(DeepinTimeline* self)
{
    DeepinTimelinePrivate* priv = self->priv;
    if (deepin_timeline_is_playing(self)) return;

    if (!priv->clock) return;

    if (priv->duration == 0 || priv->delay_id > 0) return;

    if (priv->delay) {
        priv->delay_id = g_timeout_add(priv->delay, 
                (GSourceFunc)on_timeout_start, self);
    } else {
        deepin_timeline_prepare_animation(self);
    }
}

void deepin_timeline_stop(DeepinTimeline* self)
{
    if (!deepin_timeline_is_playing(self)) return;

    deepin_timeline_stop_animation(self, TRUE);
}

void deepin_timeline_pause(DeepinTimeline* self)
{
    if (!deepin_timeline_is_playing(self)) return;
    self->priv->paused = TRUE;
}

void deepin_timeline_set_clock(DeepinTimeline* self, GdkFrameClock* clock)
{
    self->priv->clock = clock;
}

