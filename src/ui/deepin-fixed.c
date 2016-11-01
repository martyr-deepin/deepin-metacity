
/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include <config.h>
#include <util.h>
#include "deepin-design.h"
#include "deepin-fixed.h"
#include "deepin-ease.h"

struct _DeepinFixedPrivate
{
    GList *children;
    int  animation_duration;
};

enum
{
    SIGNAL_MOVE_FINISHED,
    SIGNAL_MOVE_CANCELLED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static void deepin_fixed_move_internal (DeepinFixed      *fixed,
        DeepinFixedChild *child,
        gint           x,
        gint           y);

static void deepin_fixed_end_animation(DeepinFixed* self, ChildAnimationInfo* ai)
{
    g_assert(ai->tick_id != 0);

    DeepinFixedChild* child = ai->child;

    g_signal_emit(self, signals[SIGNAL_MOVE_FINISHED], 0, child);
    child->ai = NULL;
    deepin_fixed_move_internal(self, child, ai->target_x, ai->target_y);

    gtk_widget_remove_tick_callback(GTK_WIDGET(self), ai->tick_id);
}

static gboolean on_tick_callback(DeepinFixed* self, GdkFrameClock* clock, 
        gpointer data)
{
    DeepinFixedPrivate* priv = self->priv;
    ChildAnimationInfo* ai = (ChildAnimationInfo*)data;

    gint64 now = gdk_frame_clock_get_frame_time(clock);

    gdouble duration = (now - ai->last_time) / 1000000.0;
    if (ai->last_time != ai->start_time && duration < 0.033) return G_SOURCE_CONTINUE;
    ai->last_time = now;

    gdouble t = 1.0;
    if (now < ai->end_time) {
        t = (now - ai->start_time) / (gdouble)(ai->end_time - ai->start_time);
    }
    t = ease_out_cubic(t);
    ai->current_pos = t * ai->target_pos;
    if (ai->current_pos > ai->target_pos) ai->current_pos = ai->target_pos;

    deepin_fixed_move_internal(self, ai->child,
            ai->target_x * ai->current_pos + ai->old_x * (1 - ai->current_pos),
            ai->target_y * ai->current_pos + ai->old_y * (1 - ai->current_pos));

    if (ai->current_pos >= ai->target_pos) {
        deepin_fixed_end_animation(self, ai);
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

static void deepin_fixed_prepare_animation(DeepinFixed* self, ChildAnimationInfo* ai) 
{
    if (!gtk_widget_get_realized(GTK_WIDGET(self))) {
        gtk_widget_realize(GTK_WIDGET(self));
    }

    DeepinFixedPrivate* priv = self->priv;

    ai->target_pos = 1.0;
    ai->current_pos = 0.0;

    ai->start_time = gdk_frame_clock_get_frame_time(
            gtk_widget_get_frame_clock(GTK_WIDGET(self)));
    ai->last_time = ai->start_time;
    ai->end_time = ai->start_time + (priv->animation_duration * 1000);

    ai->tick_id = gtk_widget_add_tick_callback(GTK_WIDGET(self),
            (GtkTickCallback)on_tick_callback, ai, g_free);
}

enum {
    CHILD_PROP_0,
    CHILD_PROP_X,
    CHILD_PROP_Y
};

static void deepin_fixed_realize       (GtkWidget        *widget);
static void deepin_fixed_get_preferred_width  (GtkWidget *widget,
        gint      *minimum,
        gint      *natural);
static void deepin_fixed_get_preferred_height (GtkWidget *widget,
        gint      *minimum,
        gint      *natural);
static void deepin_fixed_size_allocate (GtkWidget        *widget,
        GtkAllocation    *allocation);
static gboolean deepin_fixed_draw      (GtkWidget        *widget,
        cairo_t          *cr);
static void deepin_fixed_add           (GtkContainer     *container,
        GtkWidget        *widget);
static void deepin_fixed_remove        (GtkContainer     *container,
        GtkWidget        *widget);
static void deepin_fixed_forall        (GtkContainer     *container,
        gboolean          include_internals,
        GtkCallback       callback,
        gpointer          callback_data);
static GType deepin_fixed_child_type   (GtkContainer     *container);

static void deepin_fixed_set_child_property (GtkContainer *container,
        GtkWidget    *child,
        guint         property_id,
        const GValue *value,
        GParamSpec   *pspec);
static void deepin_fixed_get_child_property (GtkContainer *container,
        GtkWidget    *child,
        guint         property_id,
        GValue       *value,
        GParamSpec   *pspec);

G_DEFINE_TYPE_WITH_PRIVATE (DeepinFixed, deepin_fixed, GTK_TYPE_CONTAINER)

static void deepin_fixed_dispose(GObject *object)
{
    /*DeepinFixed *self = DEEPIN_FIXED(object);*/

    G_OBJECT_CLASS(deepin_fixed_parent_class)->dispose(object);
}

    static void
deepin_fixed_class_init (DeepinFixedClass *class)
{
    GtkWidgetClass *widget_class;
    GtkContainerClass *container_class;

    widget_class = (GtkWidgetClass*) class;
    container_class = (GtkContainerClass*) class;
    GObjectClass *gobject_class = G_OBJECT_CLASS(class);

    gobject_class->dispose = deepin_fixed_dispose;

    widget_class->realize = deepin_fixed_realize;
    widget_class->get_preferred_width = deepin_fixed_get_preferred_width;
    widget_class->get_preferred_height = deepin_fixed_get_preferred_height;
    widget_class->size_allocate = deepin_fixed_size_allocate;
    widget_class->draw = deepin_fixed_draw;

    container_class->add = deepin_fixed_add;
    container_class->remove = deepin_fixed_remove;
    container_class->forall = deepin_fixed_forall;
    container_class->child_type = deepin_fixed_child_type;
    container_class->set_child_property = deepin_fixed_set_child_property;
    container_class->get_child_property = deepin_fixed_get_child_property;
    gtk_container_class_handle_border_width (container_class);

    gtk_container_class_install_child_property (container_class,
            CHILD_PROP_X,
            g_param_spec_int ("x",
                ("X position"),
                ("X position of child widget"),
                G_MININT, G_MAXINT, 0,
                G_PARAM_READWRITE));

    gtk_container_class_install_child_property (container_class,
            CHILD_PROP_Y,
            g_param_spec_int ("y",
                ("Y position"),
                ("Y position of child widget"),
                G_MININT, G_MAXINT, 0,
                G_PARAM_READWRITE));

    signals[SIGNAL_MOVE_FINISHED] = g_signal_new ("move-finished",
            DEEPIN_TYPE_FIXED,
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[SIGNAL_MOVE_CANCELLED] = g_signal_new ("move-cancelled",
            DEEPIN_TYPE_FIXED,
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static GType deepin_fixed_child_type (GtkContainer *container)
{
    return GTK_TYPE_WIDGET;
}

static void deepin_fixed_init (DeepinFixed *fixed)
{
    fixed->priv = (DeepinFixedPrivate*)deepin_fixed_get_instance_private (fixed);

    gtk_widget_set_has_window (GTK_WIDGET (fixed), FALSE);

    fixed->priv->children = NULL;
    fixed->priv->animation_duration = 280;
}

/**
 * deepin_fixed_new:
 *
 * Creates a new #DeepinFixed.
 *
 * Returns: a new #DeepinFixed.
 */
GtkWidget* deepin_fixed_new ()
{
    DeepinFixed* fixed = (DeepinFixed*)g_object_new (DEEPIN_TYPE_FIXED, NULL);
    return (GtkWidget*)fixed;
}

static DeepinFixedChild* get_child (DeepinFixed  *fixed,
        GtkWidget *widget)
{
    DeepinFixedPrivate *priv = fixed->priv;
    GList *children;

    for (children = priv->children; children; children = children->next)
    {
        DeepinFixedChild *child;

        child = children->data;

        if (child->widget == widget)
            return child;
    }

    return NULL;
}

/**
 * deepin_fixed_put:
 * @fixed: a #DeepinFixed.
 * @widget: the widget to add.
 * @x: the horizontal position to place the widget at.
 * @y: the vertical position to place the widget at.
 *
 * Adds a widget to a #DeepinFixed container at the given position.
 */
void deepin_fixed_put (DeepinFixed  *fixed,
        GtkWidget *widget,
        gint       x,
        gint       y)
{
    DeepinFixedPrivate *priv = fixed->priv;
    DeepinFixedChild *child_info;

    g_return_if_fail (DEEPIN_IS_FIXED (fixed));
    g_return_if_fail (GTK_IS_WIDGET (widget));

    child_info = g_new (DeepinFixedChild, 1);
    child_info->widget = widget;
    child_info->x = x;
    child_info->y = y;
    child_info->ai = NULL;

    gtk_widget_set_parent (widget, GTK_WIDGET (fixed));

    priv->children = g_list_append (priv->children, child_info);
}

static void deepin_fixed_move_internal (DeepinFixed      *fixed,
        DeepinFixedChild *child,
        gint           x,
        gint           y)
{
    g_return_if_fail (DEEPIN_IS_FIXED (fixed));
    g_return_if_fail (gtk_widget_get_parent (child->widget) == GTK_WIDGET (fixed));

    gtk_widget_freeze_child_notify (child->widget);

    if (child->x != x)
    {
        child->x = x;
        gtk_widget_child_notify (child->widget, "x");
    }

    if (child->y != y)
    {
        child->y = y;
        gtk_widget_child_notify (child->widget, "y");
    }

    gtk_widget_thaw_child_notify (child->widget);

    if (gtk_widget_get_visible (child->widget) &&
            gtk_widget_get_visible (GTK_WIDGET (fixed)))
        gtk_widget_queue_resize (GTK_WIDGET (fixed));
}

/**
 * deepin_fixed_move:
 * @fixed: a #DeepinFixed.
 * @widget: the child widget.
 * @x: the horizontal position to move the widget to.
 * @y: the vertical position to move the widget to.
 *
 * Moves a child of a #DeepinFixed container to the given position.
 */
void deepin_fixed_move (DeepinFixed  *fixed,
        GtkWidget *widget,
        gint       x,
        gint       y,
        gboolean   animate)
{
    DeepinFixedChild* child = get_child(fixed, widget);
    if (!child) return;

    if (child->ai) {
        deepin_fixed_end_animation(fixed, child->ai);
    } 

    if (animate) {
        ChildAnimationInfo* ai = g_new0(ChildAnimationInfo, 1);
        ai->child = child;
        ai->target_x = x;
        ai->target_y = y;
        ai->old_x = ai->child->x;
        ai->old_y = ai->child->y;

        child->ai = ai;
        deepin_fixed_prepare_animation(fixed, ai);

    } else {
        deepin_fixed_move_internal (fixed, child, x, y);
    }
}

static void deepin_fixed_cancel_animation_internal(DeepinFixed* fixed,
        DeepinFixedChild* child)
{
    if (child->ai) {
        ChildAnimationInfo* ai = child->ai;
        g_assert(ai->tick_id != 0);

        g_signal_emit(fixed, signals[SIGNAL_MOVE_CANCELLED], 0, child);
        child->ai = NULL;
        deepin_fixed_move_internal(fixed, child, ai->target_x, ai->target_y);

        gtk_widget_remove_tick_callback(GTK_WIDGET(fixed), ai->tick_id);
    } 
}

void deepin_fixed_cancel_pending_animation(DeepinFixed *fixed,
                                        GtkWidget *widget)
{
    DeepinFixedPrivate* priv = fixed->priv;
    if (widget == NULL) {
        DeepinFixedChild *child;
        GList *children;

        for(children = priv->children; children; children = children->next) {
            child = (DeepinFixedChild*)children->data;
            deepin_fixed_cancel_animation_internal(fixed, child);
        }

    } else {
        DeepinFixedChild* child = get_child(fixed, widget);
        deepin_fixed_cancel_animation_internal(fixed, child);
    }
}

void deepin_fixed_raise(DeepinFixed *fixed, GtkWidget *widget)
{
    DeepinFixedPrivate* priv = fixed->priv;
    DeepinFixedChild* child = get_child(fixed, widget);

    g_return_if_fail (DEEPIN_IS_FIXED (fixed));
    g_return_if_fail (gtk_widget_get_parent (child->widget) == GTK_WIDGET (fixed));

    gint id = g_list_index(priv->children, child);
    priv->children = g_list_remove(priv->children, child);
    priv->children = g_list_append(priv->children, child);
}

static void deepin_fixed_set_child_property (GtkContainer *container,
        GtkWidget    *child,
        guint         property_id,
        const GValue *value,
        GParamSpec   *pspec)
{
    DeepinFixed *fixed = DEEPIN_FIXED (container);
    DeepinFixedChild *fixed_child;

    fixed_child = get_child (fixed, child);

    switch (property_id)
    {
        case CHILD_PROP_X:
            deepin_fixed_move_internal (fixed,
                    fixed_child,
                    g_value_get_int (value),
                    fixed_child->y);
            break;
        case CHILD_PROP_Y:
            deepin_fixed_move_internal (fixed,
                    fixed_child,
                    fixed_child->x,
                    g_value_get_int (value));
            break;
        default:
            GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
            break;
    }
}

static void deepin_fixed_get_child_property (GtkContainer *container,
        GtkWidget    *child,
        guint         property_id,
        GValue       *value,
        GParamSpec   *pspec)
{
    DeepinFixedChild *fixed_child;

    fixed_child = get_child (DEEPIN_FIXED (container), child);

    switch (property_id)
    {
        case CHILD_PROP_X:
            g_value_set_int (value, fixed_child->x);
            break;
        case CHILD_PROP_Y:
            g_value_set_int (value, fixed_child->y);
            break;
        default:
            GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
            break;
    }
}

static void deepin_fixed_realize (GtkWidget *widget)
{
    GtkAllocation allocation;
    GdkWindow *window;
    GdkWindowAttr attributes;
    gint attributes_mask;

    if (!gtk_widget_get_has_window (widget))
        GTK_WIDGET_CLASS (deepin_fixed_parent_class)->realize (widget);
    else
    {
        meta_verbose("%s: alloc window", __func__);
        gtk_widget_set_realized (widget, TRUE);

        gtk_widget_get_allocation (widget, &allocation);

        attributes.window_type = GDK_WINDOW_CHILD;
        attributes.x = allocation.x;
        attributes.y = allocation.y;
        attributes.width = allocation.width;
        attributes.height = allocation.height;
        attributes.wclass = GDK_INPUT_OUTPUT;
        attributes.visual = gtk_widget_get_visual (widget);
        attributes.event_mask = gtk_widget_get_events (widget);
        attributes.event_mask |= GDK_EXPOSURE_MASK 
            | GDK_BUTTON_PRESS_MASK 
            | GDK_BUTTON_RELEASE_MASK 
            | GDK_ENTER_NOTIFY_MASK
            | GDK_LEAVE_NOTIFY_MASK;

        attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

        window = gdk_window_new (gtk_widget_get_parent_window (widget),
                &attributes, attributes_mask);
        gtk_widget_set_window (widget, window);
        gtk_widget_register_window (widget, window);

        gtk_style_context_set_background (gtk_widget_get_style_context (widget),
                window);
    }
}

static void deepin_fixed_get_preferred_width (GtkWidget *widget,
        gint      *minimum,
        gint      *natural)
{
    DeepinFixed *fixed = DEEPIN_FIXED (widget);
    DeepinFixedPrivate *priv = fixed->priv;
    DeepinFixedChild *child;
    GList *children;
    gint child_min, child_nat;

    *minimum = 0;
    *natural = 0;

    GdkScreen* screen = gtk_widget_get_screen(widget);
    GdkRectangle mon_geom;
    gint primary = gdk_screen_get_primary_monitor(screen);
    gdk_screen_get_monitor_geometry(screen, primary, &mon_geom);

    gint max_width = mon_geom.width - POPUP_SCREEN_PADDING * 2 - POPUP_PADDING * 2;
    float box_width = 0;
    calculate_preferred_size(g_list_length(priv->children), max_width,
            &box_width, NULL, NULL, NULL, NULL);
    *minimum = *natural = box_width;
}

static void deepin_fixed_get_preferred_height (GtkWidget *widget,
        gint      *minimum,
        gint      *natural)
{
    DeepinFixed *fixed = DEEPIN_FIXED (widget);
    DeepinFixedPrivate *priv = fixed->priv;
    DeepinFixedChild *child;
    GList *children;
    gint child_min, child_nat;

    GdkScreen* screen = gtk_widget_get_screen(widget);
    GdkRectangle mon_geom;
    gint primary = gdk_screen_get_primary_monitor(screen);
    gdk_screen_get_monitor_geometry(screen, primary, &mon_geom);

    gint max_width = mon_geom.height - POPUP_SCREEN_PADDING * 2 - POPUP_PADDING * 2;
    float box_height = 0;
    calculate_preferred_size(g_list_length(priv->children), max_width,
            NULL, &box_height, NULL, NULL, NULL);
    *minimum = *natural = box_height;
}

static void deepin_fixed_size_allocate (GtkWidget     *widget,
        GtkAllocation *allocation)
{
    DeepinFixed *fixed = DEEPIN_FIXED (widget);
    DeepinFixedPrivate *priv = fixed->priv;
    DeepinFixedChild *child;
    GtkAllocation child_allocation;
    GtkRequisition child_requisition;
    GList *children;

    gtk_widget_set_allocation (widget, allocation);

    if (gtk_widget_get_has_window (widget))
    {
        if (gtk_widget_get_realized (widget))
            gdk_window_move_resize (gtk_widget_get_window (widget),
                    allocation->x,
                    allocation->y,
                    allocation->width,
                    allocation->height);
    }

    for (children = priv->children; children; children = children->next) {
        child = children->data;

        /*if (!gtk_widget_get_visible (child->widget))*/
            /*continue;*/

        gtk_widget_get_preferred_size (child->widget, &child_requisition, NULL);
        child_allocation.x = child->x - child_requisition.width/2;
        child_allocation.y = child->y - child_requisition.height/2;

        if (!gtk_widget_get_has_window (widget))
        {
            child_allocation.x += allocation->x;
            child_allocation.y += allocation->y;
        }

        child_allocation.width = child_requisition.width;
        child_allocation.height = child_requisition.height;
        gtk_widget_size_allocate (child->widget, &child_allocation);
    }
}

static void deepin_fixed_add (GtkContainer *container,
        GtkWidget    *widget)
{
    deepin_fixed_put (DEEPIN_FIXED (container), widget, 0, 0);
}

static void deepin_fixed_remove (GtkContainer *container,
        GtkWidget    *widget)
{
    DeepinFixed *fixed = DEEPIN_FIXED (container);
    DeepinFixedPrivate *priv = fixed->priv;
    DeepinFixedChild *child;
    GtkWidget *widget_container = GTK_WIDGET (container);
    GList *children;

    for (children = priv->children; children; children = children->next)
    {
        child = children->data;

        if (child->widget == widget)
        {
            gboolean was_visible = gtk_widget_get_visible (widget);

            gtk_widget_unparent (widget);

            priv->children = g_list_remove_link (priv->children, children);
            g_list_free (children);
            g_free (child);

            if (was_visible && gtk_widget_get_visible (widget_container))
                gtk_widget_queue_resize (widget_container);

            break;
        }
    }
}

static void deepin_fixed_forall (GtkContainer *container,
        gboolean      include_internals,
        GtkCallback   callback,
        gpointer      callback_data)
{
    DeepinFixed *fixed = DEEPIN_FIXED (container);
    DeepinFixedPrivate *priv = fixed->priv;
    DeepinFixedChild *child;
    GList *children;

    children = priv->children;
    while (children)
    {
        child = children->data;
        children = children->next;

        (* callback) (child->widget, callback_data);
    }
}

static gboolean deepin_fixed_draw (GtkWidget *widget,
        cairo_t   *cr)
{
    DeepinFixed *fixed = DEEPIN_FIXED (widget);
    DeepinFixedPrivate *priv = fixed->priv;
    DeepinFixedChild *child;
    GList *list;

    for (list = priv->children;
            list;
            list = list->next)
    {
        child = list->data;

        gtk_container_propagate_draw (GTK_CONTAINER (fixed),
                child->widget,
                cr);
    }

    return FALSE;
}

