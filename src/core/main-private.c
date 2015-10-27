/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity main() */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2006 Elijah Newren
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

#define _GNU_SOURCE
#define _SVID_SOURCE /* for putenv() and some signal-related functions */

#include <config.h>
#include "main.h"

/**
 * The exit code we'll return to our parent process when we eventually die.
 */
static MetaExitCode meta_exit_code = META_EXIT_SUCCESS;

/**
 * Handle on the main loop, so that we have an easy way of shutting Metacity
 * down.
 */
static GMainLoop *meta_main_loop = NULL;

/**
 * If set, Metacity will spawn an identical copy of itself immediately
 * before quitting.
 */
static gboolean meta_restart_after_quit = FALSE;


void
meta_init_loop()
{
  meta_main_loop = g_main_loop_new (NULL, FALSE);
}

void
meta_run_loop()
{
  g_main_loop_run (meta_main_loop);
}

/**
 * Stops Metacity. This tells the event loop to stop processing; it is rather
 * dangerous to use this rather than meta_restart() because this will leave
 * the user with no window manager. We generally do this only if, for example,
 * the session manager asks us to; we assume the session manager knows what
 * it's talking about.
 *
 * \param code The success or failure code to return to the calling process.
 */
void
meta_quit (MetaExitCode code)
{
  meta_exit_code = code;

  if (g_main_loop_is_running (meta_main_loop))
    g_main_loop_quit (meta_main_loop);
}

/**
 * Restarts Metacity. In practice, this tells the event loop to stop
 * processing, having first set the meta_restart_after_quit flag which
 * tells Metacity to spawn an identical copy of itself before quitting.
 * This happens on receipt of a _METACITY_RESTART_MESSAGE client event.
 */
void
meta_restart (void)
{
  meta_restart_after_quit = TRUE;
  meta_quit (META_EXIT_SUCCESS);
}

gboolean
meta_is_restart_after_quit()
{
  return meta_restart_after_quit;
}

MetaExitCode 
meta_get_exit_code()
{
  return meta_exit_code;
}
