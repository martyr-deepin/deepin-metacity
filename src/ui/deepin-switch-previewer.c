/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */


/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#include <config.h>
#include <math.h>
#include <util.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <cairo-xlib.h>
#include <prefs.h>
#include "../core/workspace.h"
#include "deepin-design.h"
#include "deepin-switch-previewer.h"
#include "deepin-tabpopup.h"
#include "deepin-cloned-widget.h"
#include "deepin-stackblur.h"
#include "deepin-window-surface-manager.h"
#include "deepin-background-cache.h"

#define SCALE_FACTOR 0.9
#define BLUR_RADIUS 10.0

struct _MetaDeepinSwitchPreviewerChild
{
    GtkWidget *widget;
    gint x;
    gint y;
};

/*TODO: handle multiple screens */
struct _MetaDeepinSwitchPreviewerPrivate
{
    MetaScreen* screen;
    MetaWorkspace* active_workspace;
    GList *children;
    DeepinTabPopup* popup;
    MetaDeepinClonedWidget* current_preview;
    MetaDeepinClonedWidget* prev_preview;

    cairo_surface_t* desktop_surface;
};

enum {
    CHILD_PROP_0,
    CHILD_PROP_X,
    CHILD_PROP_Y
};

static GQuark _cloned_widget_key_quark = 0;

static void cloned_widget_set_key(GtkWidget* w, gpointer data)
{
    if (!_cloned_widget_key_quark) {
        _cloned_widget_key_quark = g_quark_from_static_string("cloned-widget-key");
    }
    g_object_set_qdata(G_OBJECT(w), _cloned_widget_key_quark, data);
}

static gpointer cloned_widget_get_key(GtkWidget* w)
{
    if (!_cloned_widget_key_quark) {
        _cloned_widget_key_quark = g_quark_from_static_string("cloned-widget-key");
    }
    return g_object_get_qdata(G_OBJECT(w), _cloned_widget_key_quark);
}

static void meta_deepin_switch_previewer_realize       (GtkWidget        *widget);
static void meta_deepin_switch_previewer_get_preferred_width  (GtkWidget *widget,
        gint      *minimum,
        gint      *natural);
static void meta_deepin_switch_previewer_get_preferred_height (GtkWidget *widget,
        gint      *minimum,
        gint      *natural);
static void meta_deepin_switch_previewer_size_allocate (GtkWidget        *widget,
        GtkAllocation    *allocation);
static gboolean meta_deepin_switch_previewer_draw      (GtkWidget        *widget,
        cairo_t          *cr);
static void meta_deepin_switch_previewer_add           (GtkContainer     *container,
        GtkWidget        *widget);
static void meta_deepin_switch_previewer_remove        (GtkContainer     *container,
        GtkWidget        *widget);
static void meta_deepin_switch_previewer_forall        (GtkContainer     *container,
        gboolean          include_internals,
        GtkCallback       callback,
        gpointer          callback_data);
static GType meta_deepin_switch_previewer_child_type   (GtkContainer     *container);

static void meta_deepin_switch_previewer_set_child_property (GtkContainer *container,
        GtkWidget    *child,
        guint         property_id,
        const GValue *value,
        GParamSpec   *pspec);
static void meta_deepin_switch_previewer_get_child_property (GtkContainer *container,
        GtkWidget    *child,
        guint         property_id,
        GValue       *value,
        GParamSpec   *pspec);

G_DEFINE_TYPE_WITH_PRIVATE (MetaDeepinSwitchPreviewer, meta_deepin_switch_previewer, GTK_TYPE_CONTAINER)

static void meta_deepin_switch_previewer_dispose(GObject *object)
{
    MetaDeepinSwitchPreviewer *self = META_DEEPIN_SWITCH_PREVIEWER(object);
    MetaDeepinSwitchPreviewerPrivate* priv = self->priv;

    if (priv->desktop_surface) {
        g_clear_pointer(&priv->desktop_surface, cairo_surface_destroy);
    }

    G_OBJECT_CLASS(meta_deepin_switch_previewer_parent_class)->dispose(object);
}

static void meta_deepin_switch_previewer_class_init (MetaDeepinSwitchPreviewerClass *klass)
{
    GtkWidgetClass *widget_class;
    GtkContainerClass *container_class;

    widget_class = (GtkWidgetClass*) klass;
    container_class = (GtkContainerClass*) klass;
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = meta_deepin_switch_previewer_dispose;
    widget_class->realize = meta_deepin_switch_previewer_realize;
    widget_class->get_preferred_width = meta_deepin_switch_previewer_get_preferred_width;
    widget_class->get_preferred_height = meta_deepin_switch_previewer_get_preferred_height;
    widget_class->size_allocate = meta_deepin_switch_previewer_size_allocate;
    widget_class->draw = meta_deepin_switch_previewer_draw;

    container_class->add = meta_deepin_switch_previewer_add;
    container_class->remove = meta_deepin_switch_previewer_remove;
    container_class->forall = meta_deepin_switch_previewer_forall;
    container_class->child_type = meta_deepin_switch_previewer_child_type;
    container_class->set_child_property = meta_deepin_switch_previewer_set_child_property;
    container_class->get_child_property = meta_deepin_switch_previewer_get_child_property;
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
}

static GType meta_deepin_switch_previewer_child_type (GtkContainer *container)
{
    return GTK_TYPE_WIDGET;
}

static void meta_deepin_switch_previewer_init (MetaDeepinSwitchPreviewer *self)
{
    self->priv = (MetaDeepinSwitchPreviewerPrivate*)meta_deepin_switch_previewer_get_instance_private (self);

    gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

    self->priv->children = NULL;
}

GtkWidget* meta_deepin_switch_previewer_new (DeepinTabPopup* popup)
{
    MetaDeepinSwitchPreviewer* self =
        (MetaDeepinSwitchPreviewer*)g_object_new(META_TYPE_DEEPIN_SWITCH_PREVIEWER, NULL);
    gtk_widget_set_app_paintable(GTK_WIDGET(self), TRUE);

    MetaDeepinSwitchPreviewerPrivate* priv = self->priv;
    priv->popup = popup;

    return (GtkWidget*)self;
}

void meta_deepin_switch_previewer_populate(MetaDeepinSwitchPreviewer* self)
{
    MetaDeepinSwitchPreviewerPrivate* priv = self->priv;
    MetaDisplay* disp = meta_get_display();

    GList* l = priv->popup->entries;
    while (l) {
        DeepinTabEntry* te = (DeepinTabEntry*)l->data;
        MetaWindow* win = meta_display_lookup_x_window(disp, (Window)te->key);    

        if (win) {
            if (!priv->screen) {
                priv->screen = meta_window_get_screen(win);
                priv->active_workspace = priv->screen->active_workspace;
            }
            GtkWidget* widget = meta_deepin_cloned_widget_new(win);
                    
            MetaRectangle r;
            meta_window_get_outer_rect(win, &r);
            gint w = r.width, h = r.height;
            meta_deepin_cloned_widget_set_size(META_DEEPIN_CLONED_WIDGET(widget),
                w, h);
            /* put around center */
            meta_deepin_switch_previewer_put(self, widget,
                    r.x + w/2, r.y + h/2);
            cloned_widget_set_key(widget, GINT_TO_POINTER(te->key));
        }
        l = l->next;
    }

    if (!priv->desktop_surface) {
        MetaWindow *desktop_win = NULL, *dock_win = NULL;

        GList* windows = priv->active_workspace->mru_list;
        while (windows != NULL) {
            MetaWindow *w = (MetaWindow*)windows->data;
            if (w->screen == priv->screen && w->type == META_WINDOW_DESKTOP) {
                desktop_win = w;
            }

            if (w->screen == priv->screen && w->type == META_WINDOW_DOCK) {
                dock_win = w;
            }

            if (desktop_win && dock_win) break;
            windows = windows->next;
        }

        MetaRectangle r1 = {0, 0, 0, 0}, r2 = {0, 0, 0, 0};
        cairo_surface_t* aux1 = NULL, *aux2 = NULL;

        if (desktop_win) {
            meta_window_get_outer_rect(desktop_win, &r1);
            aux1 = deepin_window_surface_manager_get_surface(desktop_win, 1.0); 
        }

        if (dock_win) {
            meta_window_get_outer_rect(dock_win, &r2);
            aux2 = deepin_window_surface_manager_get_surface(dock_win, 1.0); 
        }

        priv->desktop_surface = deepin_window_surface_manager_get_combined3(
                deepin_background_cache_get_surface(1.0), 
                aux1, r1.x, r1.y,
                aux2, r2.x, r2.y,
                1.0);
    }

    gtk_widget_queue_resize(GTK_WIDGET(self));
}

static MetaDeepinSwitchPreviewerChild* get_child (MetaDeepinSwitchPreviewer  *self,
        GtkWidget *widget)
{
    MetaDeepinSwitchPreviewerPrivate *priv = self->priv;
    MetaDeepinSwitchPreviewerChild *child;
    GList *children;

    for (children = priv->children; children; children = children->next) {
        child = (MetaDeepinSwitchPreviewerChild*) children->data;
        if (child->widget == widget)
            return child;
    }

    return NULL;
}

void meta_deepin_switch_previewer_put (MetaDeepinSwitchPreviewer  *self,
        GtkWidget *widget,
        gint       x,
        gint       y)
{
    MetaDeepinSwitchPreviewerPrivate *priv = self->priv;
    MetaDeepinSwitchPreviewerChild *child_info;

    g_return_if_fail (META_IS_DEEPIN_SWITCH_PREVIEWER (self));
    g_return_if_fail (GTK_IS_WIDGET (widget));

    child_info = g_new (MetaDeepinSwitchPreviewerChild, 1);
    child_info->widget = widget;
    child_info->x = x;
    child_info->y = y;

    gtk_widget_set_parent (widget, GTK_WIDGET (self));

    priv->children = g_list_append (priv->children, child_info);
}

static void meta_deepin_switch_previewer_move_internal (
        MetaDeepinSwitchPreviewer      *self,
        MetaDeepinSwitchPreviewerChild *child,
        gint           x,
        gint           y)
{
    g_return_if_fail (META_IS_DEEPIN_SWITCH_PREVIEWER (self));
    g_return_if_fail (gtk_widget_get_parent (child->widget) == GTK_WIDGET (self));

    gtk_widget_freeze_child_notify (child->widget);

    if (child->x != x) {
        child->x = x;
        gtk_widget_child_notify (child->widget, "x");
    }

    if (child->y != y) {
        child->y = y;
        gtk_widget_child_notify (child->widget, "y");
    }

    gtk_widget_thaw_child_notify (child->widget);

    if (gtk_widget_get_visible (child->widget) &&
            gtk_widget_get_visible (GTK_WIDGET (self)))
        gtk_widget_queue_resize (GTK_WIDGET (self));
}

void meta_deepin_switch_previewer_move (MetaDeepinSwitchPreviewer  *self,
        GtkWidget *widget,
        gint       x,
        gint       y)
{
    meta_deepin_switch_previewer_move_internal (self, get_child (self, widget), x, y);
}

static void meta_deepin_switch_previewer_set_child_property (GtkContainer *container,
        GtkWidget    *child,
        guint         property_id,
        const GValue *value,
        GParamSpec   *pspec)
{
    MetaDeepinSwitchPreviewer *self = META_DEEPIN_SWITCH_PREVIEWER (container);
    MetaDeepinSwitchPreviewerChild *fixed_child = get_child (self, child);

    switch (property_id)
    {
        case CHILD_PROP_X:
            meta_deepin_switch_previewer_move_internal (self,
                    fixed_child,
                    g_value_get_int (value),
                    fixed_child->y);
            break;
        case CHILD_PROP_Y:
            meta_deepin_switch_previewer_move_internal (self,
                    fixed_child,
                    fixed_child->x,
                    g_value_get_int (value));
            break;
        default:
            GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
            break;
    }
}

static void meta_deepin_switch_previewer_get_child_property (GtkContainer *container,
        GtkWidget    *child,
        guint         property_id,
        GValue       *value,
        GParamSpec   *pspec)
{
    MetaDeepinSwitchPreviewerChild *fixed_child;

    fixed_child = get_child (META_DEEPIN_SWITCH_PREVIEWER (container), child);

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

static void meta_deepin_switch_previewer_realize (GtkWidget *widget) 
{
    if (!gtk_widget_get_has_window (widget))
        GTK_WIDGET_CLASS (meta_deepin_switch_previewer_parent_class)->realize (widget);
}

static void meta_deepin_switch_previewer_get_preferred_width (GtkWidget *widget,
        gint* minimum, gint* natural)
{
    MetaDeepinSwitchPreviewer *self = META_DEEPIN_SWITCH_PREVIEWER (widget);
    MetaDeepinSwitchPreviewerPrivate *priv = self->priv;

    GdkScreen* screen = gtk_widget_get_screen(priv->popup->outline_window);
    *minimum = *natural = gdk_screen_get_width(screen);
}

static void meta_deepin_switch_previewer_get_preferred_height (GtkWidget *widget,
        gint* minimum, gint* natural)
{
    MetaDeepinSwitchPreviewer *self = META_DEEPIN_SWITCH_PREVIEWER (widget);
    MetaDeepinSwitchPreviewerPrivate *priv = self->priv;
    GdkScreen* screen = gtk_widget_get_screen(priv->popup->outline_window);
    *minimum = *natural = gdk_screen_get_height(screen);
}

static void meta_deepin_switch_previewer_size_allocate (GtkWidget* widget,
        GtkAllocation *allocation)
{
    MetaDeepinSwitchPreviewer *self = META_DEEPIN_SWITCH_PREVIEWER (widget);
    MetaDeepinSwitchPreviewerPrivate *priv = self->priv;
    MetaDeepinSwitchPreviewerChild *child;
    GtkAllocation child_allocation;
    GtkRequisition child_requisition;
    GList *children;

    gtk_widget_set_allocation (widget, allocation);

    for (children = priv->children; children; children = children->next) {
        child = (MetaDeepinSwitchPreviewerChild*)children->data;

        if (!gtk_widget_get_visible (child->widget))
            continue;

        gtk_widget_get_preferred_size (child->widget, &child_requisition, NULL);
        child_allocation.x = child->x - child_requisition.width/2;
        child_allocation.y = child->y - child_requisition.height/2;

        if (!gtk_widget_get_has_window (widget)) {
            child_allocation.x += allocation->x;
            child_allocation.y += allocation->y;
        }

        child_allocation.width = child_requisition.width;
        child_allocation.height = child_requisition.height;
        gtk_widget_size_allocate (child->widget, &child_allocation);
    }
}

static void meta_deepin_switch_previewer_add (GtkContainer *container,
        GtkWidget    *widget)
{
    meta_deepin_switch_previewer_put (META_DEEPIN_SWITCH_PREVIEWER (container), widget, 0, 0);
}

static void meta_deepin_switch_previewer_remove (GtkContainer *container,
        GtkWidget    *widget)
{
    MetaDeepinSwitchPreviewer *self = META_DEEPIN_SWITCH_PREVIEWER (container);
    MetaDeepinSwitchPreviewerPrivate *priv = self->priv;
    MetaDeepinSwitchPreviewerChild *child;
    GtkWidget *widget_container = GTK_WIDGET (container);
    GList *children;

    for (children = priv->children; children; children = children->next) {
        child = (MetaDeepinSwitchPreviewerChild*)children->data;

        if (child->widget == widget) {
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

static void meta_deepin_switch_previewer_forall (GtkContainer *container,
        gboolean      include_internals,
        GtkCallback   callback,
        gpointer      callback_data)
{
    MetaDeepinSwitchPreviewer *self = META_DEEPIN_SWITCH_PREVIEWER (container);
    MetaDeepinSwitchPreviewerPrivate *priv = self->priv;
    MetaDeepinSwitchPreviewerChild *child;
    GList *children;

    children = priv->children;
    while (children) {
        child = (MetaDeepinSwitchPreviewerChild*)children->data;
        children = children->next;

        (* callback) (child->widget, callback_data);
    }
}

static gboolean meta_deepin_switch_previewer_draw (GtkWidget *widget,
        cairo_t   *cr)
{
    MetaDeepinSwitchPreviewer *self = META_DEEPIN_SWITCH_PREVIEWER (widget);
    MetaDeepinSwitchPreviewerPrivate *priv = self->priv;

    cairo_save(cr);
    cairo_reset_clip(cr);

    GtkRequisition req;
    gtk_widget_get_preferred_size(widget, &req, NULL);

    cairo_rectangle_int_t cur_rect = {0, 0, 0, 0};
    cairo_rectangle_int_t r = {0, 0, req.width, req.height};
    cairo_region_t* reg = cairo_region_create_rectangle(&r);

    if (priv->current_preview) {
        MetaWindow* meta_win = meta_deepin_cloned_widget_get_window(
                priv->current_preview);

        if (meta_win->type != META_WINDOW_DESKTOP
            && meta_deepin_cloned_widget_get_alpha(priv->current_preview) > 0.2) {

            double sx = SCALE_FACTOR, sy = SCALE_FACTOR;
            gtk_widget_get_allocation(GTK_WIDGET(priv->current_preview), &r);
            r.x += r.width * (1-sx)/2; r.y += r.height * (1-sy)/2;
            r.width *= sx; r.height *= sy;
            cur_rect = r;
            cairo_region_subtract_rectangle(reg, &r);
        }
    }

    gdk_cairo_region(cr, reg);
    cairo_clip(cr);
    cairo_paint(cr);

    if (priv->desktop_surface) {
        cairo_set_source_surface(cr, priv->desktop_surface, 0, 0);
        cairo_paint(cr);
    }

    cairo_region_destroy(reg);
    cairo_restore(cr);

    if (priv->prev_preview && !meta_prefs_get_reduced_resources()) {
        cairo_save(cr);

        cairo_region_t* reg = NULL;
        if (priv->current_preview) {
            gtk_widget_get_allocation(GTK_WIDGET(priv->prev_preview), &r);
            reg = cairo_region_create_rectangle(&r);
            cairo_region_subtract_rectangle(reg, &cur_rect);
            gdk_cairo_region(cr, reg);
            cairo_clip(cr);
        }

        gtk_container_propagate_draw(GTK_CONTAINER(self),
                GTK_WIDGET(priv->prev_preview), cr);

        cairo_restore(cr);
        if (reg) cairo_region_destroy(reg);
    }

    gtk_container_propagate_draw(GTK_CONTAINER(self), GTK_WIDGET(priv->current_preview), cr);
    return TRUE;
}

void meta_deepin_switch_previewer_select(MetaDeepinSwitchPreviewer* self,
        DeepinTabEntry* te)
{
    MetaDeepinSwitchPreviewerPrivate* priv = self->priv;
    MetaDeepinSwitchPreviewerChild* child;
    MetaDeepinClonedWidget* w = NULL;

    GList* l = priv->children;
    while (l) {
        child = (MetaDeepinSwitchPreviewerChild*)l->data;
        if (te->key == cloned_widget_get_key(child->widget)) {
            w = (MetaDeepinClonedWidget*)child->widget;
            break;
        }
        l = l->next;
    }

    if (w) {
        if (priv->current_preview) {
            priv->prev_preview = priv->current_preview;
            MetaWindow* meta_win = meta_deepin_cloned_widget_get_window(
                    priv->prev_preview);
            if (meta_win->type != META_WINDOW_DESKTOP && !meta_prefs_get_reduced_resources()) {
                meta_deepin_cloned_widget_set_scale(priv->current_preview, 1.0, 1.0);
                meta_deepin_cloned_widget_set_alpha(priv->current_preview, 1.0);
                meta_deepin_cloned_widget_push_state(priv->current_preview);
            }
            meta_deepin_cloned_widget_set_scale(priv->current_preview, SCALE_FACTOR, SCALE_FACTOR);
            meta_deepin_cloned_widget_set_alpha(priv->current_preview, 0.0);

            meta_deepin_cloned_widget_unselect(priv->current_preview);
        } 

        priv->current_preview = w;
        MetaWindow* meta_win = meta_deepin_cloned_widget_get_window(w);
        if (meta_win->type != META_WINDOW_DESKTOP && !meta_prefs_get_reduced_resources()) {
            meta_deepin_cloned_widget_set_scale(w, SCALE_FACTOR, SCALE_FACTOR);
            meta_deepin_cloned_widget_set_alpha(w, 0.0);
            meta_deepin_cloned_widget_push_state(w);
        }
        meta_deepin_cloned_widget_set_scale(w, 1.0, 1.0);
        meta_deepin_cloned_widget_set_alpha(w, 1.0);

        meta_deepin_cloned_widget_select(w);
    }
}

