/* Copyright © 2005, 2006, 2008 Jakub Wilk
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; version 2 dated June, 1991.
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ncursesw/ncurses.h>

#include "vbi.h"

short dochttx_vbi_colors[8] = 
{ COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW, 
  COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE };

struct dochttx_vbi_state *dochttx_vbi_open(char *dev, int region)
{
  struct dochttx_vbi_state *vbi;
  unsigned int services =
    VBI_SLICED_VBI_525 | VBI_SLICED_VBI_625 |
    VBI_SLICED_TELETEXT_B | VBI_SLICED_CAPTION_525 |
    VBI_SLICED_CAPTION_625 | VBI_SLICED_VPS |
    VBI_SLICED_WSS_625 | VBI_SLICED_WSS_CPR1204;
  do
  {
    vbi = malloc(sizeof(*vbi));
    if (vbi == NULL)
      break;
    memset(vbi, 0, sizeof(*vbi));
    vbi->dec = vbi_decoder_new();
    if (vbi->dec == NULL)
      break;
    vbi_teletext_set_default_region(vbi->dec, region);
    vbi->cap = vbi_capture_v4l2_new(dev, 16, &services, -1, &vbi->err, 0);
    if (vbi->cap == NULL)
      break;
    vbi->par = vbi_capture_parameters(vbi->cap);
    vbi->fd = vbi_capture_fd(vbi->cap);
    vbi->lines = (vbi->par->count[0] + vbi->par->count[1]);
    vbi->raw = malloc(vbi->lines * vbi->par->bytes_per_line);
    if (vbi->raw == NULL)
      break;
    vbi->sliced = malloc(vbi->lines * sizeof(vbi_sliced));
    if (vbi->sliced == NULL)
      break;
    vbi->tv.tv_sec = 1;
    vbi->tv.tv_usec = 0;
    return vbi;
  } 
  while (false);
  dochttx_vbi_close(vbi);
  fprintf(stderr, "vbi: open failed (%s)\n", dev);
  return NULL;
}

int dochttx_vbi_has_data(struct dochttx_vbi_state *vbi)
{
  int rc;
  rc = vbi_capture_read(vbi->cap, vbi->raw, vbi->sliced, &vbi->lines, &vbi->ts, &vbi->tv);
  vbi_decode(vbi->dec, vbi->sliced, vbi->lines, vbi->ts);
  return rc;
}

void dochttx_vbi_close(struct dochttx_vbi_state *vbi)
{
  if (vbi == NULL)
    return;
  free(vbi->sliced);
  free(vbi->raw);
  if (vbi->cap != NULL)
    vbi_capture_delete(vbi->cap);
  if (vbi->dec != NULL)
    vbi_decoder_delete(vbi->dec);
  free(vbi);
}

// vim:ts=2 sw=2 et
