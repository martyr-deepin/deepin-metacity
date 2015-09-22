/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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


double deepin_linear (double t)
{
    return t;
}

/* From clutter-easing.c, based on Robert Penner's
 *  * infamous easing equations, MIT license.
 *   */
double ease_out_cubic (double t)
{
    double p = t - 1;
    return p * p * p + 1;
}

double ease_in_out_quad (double t)
{
    double p = t * 2.0;

    if (p < 1)
        return 0.5 * p * p;

    p -= 1;

    return -0.5 * (p * (p - 2) - 1);
}

double ease_out_quad (double t)
{
  double p = t;

  return -1.0 * p * (p - 2);
}

