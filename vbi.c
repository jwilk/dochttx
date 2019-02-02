/* Copyright © 2005-2019 Jakub Wilk <jwilk@jwilk.net>
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vbi.h"

struct dochttx_vbi_state *dochttx_vbi_open(const char *dev, int region)
{
    struct dochttx_vbi_state *vbi;
    unsigned int services = VBI_SLICED_TELETEXT_B;
    const char *error = NULL;
    do {
        size_t lines;
        vbi = malloc(sizeof(*vbi));
        if (vbi == NULL)
            break;
        memset(vbi, 0, sizeof(*vbi));
        vbi->dec = vbi_decoder_new();
        if (vbi->dec == NULL)
            break;
        vbi_teletext_set_default_region(vbi->dec, region);
        vbi->cap = vbi_capture_v4l2_new(dev, 16, &services, -1, &vbi->err, 0);
        if (vbi->cap == NULL) {
            error = vbi->err;
            break;
        }
        vbi->par = vbi_capture_parameters(vbi->cap);
        vbi->fd = vbi_capture_fd(vbi->cap);
        lines = vbi->par->count[0] + vbi->par->count[1];
        vbi->raw = malloc(lines * vbi->par->bytes_per_line);
        if (vbi->raw == NULL)
            break;
        vbi->sliced = malloc(lines * sizeof(vbi_sliced));
        if (vbi->sliced == NULL)
            break;
        vbi->tv.tv_sec = 1;
        vbi->tv.tv_usec = 0;
        return vbi;
    }
    while (false);
    if (error == NULL)
        error = strerror(errno);
    dochttx_vbi_close(vbi);
    fprintf(stderr, "dochttx: %s\n", error);
    return NULL;
}

int dochttx_vbi_has_data(struct dochttx_vbi_state *vbi)
{
    int rc;
    int lines;
    rc = vbi_capture_read(vbi->cap, vbi->raw, vbi->sliced, &lines, &vbi->ts, &vbi->tv);
    vbi_decode(vbi->dec, vbi->sliced, lines, vbi->ts);
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

// vim:ts=4 sts=4 sw=4 et
