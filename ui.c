/* Copyright © 2005-2019 Jakub Wilk <jwilk@jwilk.net>
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>

#include <curses.h>

#include "ui.h"

int dochttx_colors[8][8];

static void color_setup(void)
{
    start_color();
    short u = 1;
    for (short i = 0; i < 8; i++)
    for (short j = 0; j < 8; j++, u++) {
        init_pair(u, i, j);
        dochttx_colors[i][j] = COLOR_PAIR(u);
    }
}

void dochttx_ncurses_init(void)
{
    initscr();
    raw();
    noecho();
    nonl();
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

// vim:ts=4 sts=4 sw=4 et
