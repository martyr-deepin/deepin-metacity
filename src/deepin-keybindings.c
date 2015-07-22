/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* deepin custom keybindings */

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

#include <glib.h>
#include <config.h>
#include <util.h>
#include <tabpopup.h>
#include <ui.h>
#include <screen.h>
#include "core/keybindings.h"
#include "core/window-private.h"

static unsigned int
get_primary_modifier (MetaDisplay *display,
                      unsigned int entire_binding_mask)
{
  /* The idea here is to see if the "main" modifier
   * for Alt+Tab has been pressed/released. So if the binding
   * is Alt+Shift+Tab then releasing Alt is the thing that
   * ends the operation. It's pretty random how we order
   * these.
   */
  unsigned int masks[] = { Mod5Mask, Mod4Mask, Mod3Mask,
                           Mod2Mask, Mod1Mask, ControlMask,
                           ShiftMask, LockMask };

  int i;

  i = 0;
  while (i < (int) G_N_ELEMENTS (masks))
    {
      if (entire_binding_mask & masks[i])
        return masks[i];
      ++i;
    }

  return 0;
}

static gboolean
primary_modifier_still_pressed (MetaDisplay *display,
                                unsigned int entire_binding_mask)
{
  unsigned int primary_modifier;
  int x, y, root_x, root_y;
  Window root, child;
  guint mask;
  MetaScreen *random_screen;
  Window      random_xwindow;

  primary_modifier = get_primary_modifier (display, entire_binding_mask);

  random_screen = display->screens->data;
  random_xwindow = random_screen->no_focus_window;
  XQueryPointer (display->xdisplay,
                 random_xwindow, /* some random window */
                 &root, &child,
                 &root_x, &root_y,
                 &x, &y,
                 &mask);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Primary modifier 0x%x full grab mask 0x%x current state 0x%x\n",
              primary_modifier, entire_binding_mask, mask);

  if ((mask & primary_modifier) == 0)
    return FALSE;
  else
    return TRUE;
}

static MetaGrabOp
tab_op_from_tab_type (MetaTabList type)
{
  switch (type)
    {
    case META_TAB_LIST_NORMAL:
      return META_GRAB_OP_KEYBOARD_TABBING_NORMAL;
    case META_TAB_LIST_DOCKS:
      return META_GRAB_OP_KEYBOARD_TABBING_DOCK;
    case META_TAB_LIST_GROUP:
      return META_GRAB_OP_KEYBOARD_TABBING_GROUP;
    }

  g_assert_not_reached ();

  return 0;
}
 
static MetaGrabOp
cycle_op_from_tab_type (MetaTabList type)
{
  switch (type)
    {
    case META_TAB_LIST_NORMAL:
      return META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL;
    case META_TAB_LIST_DOCKS:
      return META_GRAB_OP_KEYBOARD_ESCAPING_DOCK;
    case META_TAB_LIST_GROUP:
      return META_GRAB_OP_KEYBOARD_ESCAPING_GROUP;
    }

  g_assert_not_reached ();

  return 0;
}

static void
do_choose_window (MetaDisplay    *display,
                  MetaScreen     *screen,
                  MetaWindow     *event_window,
                  XEvent         *event,
                  MetaKeyBinding *binding,
                  gboolean        backward,
                  gboolean        show_popup)
{
  MetaTabList type = binding->handler->data;
  MetaWindow *initial_selection;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Tab list = %u show_popup = %d\n", type, show_popup);

  /* reverse direction if shift is down */
  if (event->xkey.state & ShiftMask)
    backward = !backward;

  initial_selection = meta_display_get_tab_next (display,
                                                 type,
                                                 screen,
                                                 screen->active_workspace,
                                                 NULL,
                                                 backward);

  /* Note that focus_window may not be in the tab chain, but it's OK */
  if (initial_selection == NULL)
    initial_selection = meta_display_get_tab_current (display,
                                                      type, screen,
                                                      screen->active_workspace);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Initially selecting window %s\n",
              initial_selection ? initial_selection->desc : "(none)");

  if (initial_selection != NULL)
    {
      if (binding->mask == 0)
        {
          /* If no modifiers, we can't do the "hold down modifier to keep
           * moving" thing, so we just instaswitch by one window.
           */
          meta_topic (META_DEBUG_FOCUS,
                      "Activating %s and turning off mouse_mode due to "
                      "switch/cycle windows with no modifiers\n",
                      initial_selection->desc);
          display->mouse_mode = FALSE;
          meta_window_activate (initial_selection, event->xkey.time);
        }
      else if (meta_display_begin_grab_op (display,
                                           screen,
                                           NULL,
                                           show_popup ?
                                           tab_op_from_tab_type (type) :
                                           cycle_op_from_tab_type (type),
                                           FALSE,
                                           FALSE,
                                           0,
                                           binding->mask,
                                           event->xkey.time,
                                           0, 0))
        {
          if (!primary_modifier_still_pressed (display,
                                               binding->mask))
            {
              /* This handles a race where modifier might be released
               * before we establish the grab. must end grab
               * prior to trying to focus a window.
               */
              meta_topic (META_DEBUG_FOCUS,
                          "Ending grab, activating %s, and turning off "
                          "mouse_mode due to switch/cycle windows where "
                          "modifier was released prior to grab\n",
                          initial_selection->desc);
              meta_display_end_grab_op (display, event->xkey.time);
              display->mouse_mode = FALSE;
              meta_window_activate (initial_selection, event->xkey.time);
            }
          else
            {
              meta_ui_tab_popup_select (screen->tab_popup,
                                        (MetaTabEntryKey) initial_selection->xwindow);

              if (show_popup)
                meta_ui_tab_popup_set_showing (screen->tab_popup,
                                               TRUE);
              else
                {
                  meta_window_raise (initial_selection);
                  initial_selection->tab_unminimized =
                    initial_selection->minimized;
                  meta_window_unminimize (initial_selection);
                }
            }
        }
    }
}

static void handle_switch(MetaDisplay *display, MetaScreen *screen,
        MetaWindow *window, XEvent *event,
        MetaKeyBinding *binding, gpointer user_data)
{
  gint backwards = (binding->handler->flags & META_KEY_BINDING_IS_REVERSED) != 0;

  do_choose_window (display, screen, window, event, binding, backwards, TRUE);
}

void deepin_init_custom_handlers()
{
	deepin_meta_override_keybinding_handler("switch-applications",
            handle_switch, NULL, NULL);
}
