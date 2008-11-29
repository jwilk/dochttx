/* Copyright © 2005, 2006, 2008 Jakub Wilk
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; version 2 dated June, 1991.
 */

#include <ncursesw/ncurses.h>

#include "vbi.h"
#include "ui.h"

int dochttx_colors[8][8];

static void color_setup(void)
{
  start_color();
  int i, j, u;
  u = 1;
  for (i = 0; i < 8; i++)
  for (j = 0; j < 8; j++, u++)
  {
    init_pair(u, dochttx_vbi_colors[i], dochttx_vbi_colors[j]);
    dochttx_colors[i][j] = COLOR_PAIR(u);
  }
}

void dochttx_ncurses_init(void)
{
  initscr();
  raw(); noecho(); nonl();
  intrflush(NULL, FALSE);
  keypad(stdscr, TRUE);
  timeout(500);
  erase();
  color_setup();
}

void dochttx_ncurses_quit(void)
{
  erase();
  endwin();
}

// vim:ts=2 sw=2 et
