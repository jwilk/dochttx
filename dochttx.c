/* Copyright © 2005-2019 Jakub Wilk <jwilk@jwilk.net>
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <poll.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curses.h>

#include "autoconf.h"
#include "locale.h"
#include "region.h"
#include "render.h"
#include "ui.h"
#include "vbi.h"

static const char default_device[] = "/dev/vbi0";

static vbi_pgno cur_pgno = 0;
static vbi_subno cur_subno = 0;
static bool cur_drawn = true;

static void on_event_ttx_page(vbi_event *ev, void *data)
{
    assert(data == NULL);
    cur_pgno = ev->ev.ttx_page.pgno;
    cur_subno = ev->ev.ttx_page.subno;
    cur_drawn = false;
}

struct input {
    char text[7];
    size_t position;
    enum {
        INPUT_ERROR = -1,
        INPUT_NORMAL = 0,
        INPUT_VALIDATED,
        INPUT_VALIDATED_JUST_NOW,
    } status;
};

static void draw_input(const struct input *input)
{
    mvprintw(0, 43, "Look for:");
    mvhline(0, 53, '_', sizeof input->text - 1);
    switch (input->status) {
    case INPUT_ERROR:
        attrset(dochttx_colors[COLOR_WHITE][COLOR_RED]);
    case INPUT_VALIDATED:
    case INPUT_VALIDATED_JUST_NOW:
        attron(A_BOLD);
        break;
    case INPUT_NORMAL:
        attrset(A_NORMAL);
    }
    mvprintw(0, 53, input->text);
    attrset(A_NORMAL);
    move(0, 53 + input->position);
}

static void draw_looking_for(unsigned int pgno, unsigned int subno)
{
    char subnos[3] = "*";
    if (subno != VBI_ANY_SUBNO)
        sprintf(subnos, "%02x", subno);
    mvhline(2, 43, ' ', COLS - 43);
    mvprintw(2, 43, "Looking for %03X.%s", pgno, subnos);
}

static void draw_showing_page(vbi_decoder* dec, unsigned int pgno, unsigned int subno)
{
    vbi_subno maxsubno;
    vbi_classify_page(dec, pgno, &maxsubno, NULL);
    mvhline(4, 43, ' ', COLS - 43);
    mvprintw(4, 43, "Showing page %03x", pgno);
    if (maxsubno == 0)
        printw(", no subpages");
    else if (maxsubno == 0xFFFE || maxsubno == 0xFFFF)
        printw(", subpage %02x", subno);
    else if (maxsubno <= 0x3F7F)
        printw(", subpage %02x of %02x", subno, (unsigned int) maxsubno);
}

static void usage(FILE *fp)
{
    fprintf(fp, "Usage: dochttx [-d DEVICE] [-l LANG[,LANG...]]\n");
}

static void long_usage(FILE *fp)
{
    usage(fp);
    printf("\n"
        "Options:\n"
        "  -d DEVICE          VBI device to use (default: %s)\n"
        "  -l LANG[,LANG...]  use character sets for these languages\n"
        "                     (default: locale-dependent)\n"
        "  -h, --help         show this help message and exit\n"
        "  --version          show version information and exit\n",
        default_device
    );
}

static void print_version(void)
{
    unsigned int major, minor, micro;
    printf("%s\n", PACKAGE_STRING);
    vbi_version(&major, &minor, &micro);
    printf("+ ZVBI %u.%u.%u\n", major, minor, micro);
}

static int parse_pagespec(const char *pagespec, unsigned int *pgno, unsigned int *subno)
{
    int rc;
    static regex_t regexp;
    static bool regexp_initialized = false;
    if (!regexp_initialized) {
        rc = regcomp(&regexp,
                /* we don't use ranges such as 1-8 here,
                  * they're undefined outside the POSIX locale */
                "^[12345678]"
                "[0123456789abcdefABCDEF]{2}"
                "([.]([0123456789]|[01234567][0123456789]))?$",
                REG_EXTENDED | REG_NOSUB
            );
        assert(rc == 0);
        regexp_initialized = true;
    }
    rc = regexec(&regexp, pagespec, 0, NULL, 0);
    if (rc == REG_NOMATCH) {
        *pgno = *subno = 0;
        return -1;
    }
    *subno = VBI_ANY_SUBNO;
    rc = sscanf(pagespec, "%x.%x", pgno, subno);
    assert(rc == 1 || rc == 2);
    return 0;
}

int main(int argc, char **argv)
{
    int region = -1;
    const char *device = default_device;
    int opt;
    enum {
        OPT_DUMMY = CHAR_MAX,
        OPT_VERSION
    };
    static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, OPT_VERSION},
        {NULL, 0, NULL, 0}
    };
    while ((opt = getopt_long(argc, argv, "d:l:h", long_options, NULL)) != -1)
        switch (opt)
        {
        case 'd':
            device = optarg;
            break;
        case 'l':
            region = dochttx_region_for_lang(optarg);
            if (region < 0) {
                errno = EINVAL;
                perror("dochttx: -l");
                exit(EXIT_FAILURE);
            }
            break;
        case 'h':
            long_usage(stdout);
            exit(EXIT_SUCCESS);
        case OPT_VERSION:
            print_version();
            exit(EXIT_SUCCESS);
        default: /* '?' */
            usage(stderr);
            exit(EXIT_FAILURE);
        }
    if (optind != argc) {
        usage(stderr);
        exit(EXIT_FAILURE);
    }

    int rc = dochttx_locale_init();
    if (rc < 0) {
        perror("dochttx: locale initialization failed");
        return EXIT_FAILURE;
    }

    if (region < 0)
        region = dochttx_region_for_locale();
    if (region < 0)
        region = dochttx_region_for_lang("en");
    assert(region >= 0);

    struct dochttx_vbi_state* vbi = dochttx_vbi_open(device, region);
    if (vbi == NULL)
        return EXIT_FAILURE;

    dochttx_ncurses_init();

    mvvline(0, 41, ACS_VLINE, 25);
    for (int y = 0; y < 25; y++)
        mvhline(y, 0, ACS_BOARD, 41);
    mvhline(25, 0, ACS_HLINE, COLS);
    mvaddch(25, 41, ACS_BTEE);

    vbi_event_handler_register(vbi->dec, VBI_EVENT_TTX_PAGE, on_event_ttx_page, NULL);

    vbi_pgno req_pgno = 0x100;
    vbi_subno req_subno = VBI_ANY_SUBNO;
    draw_looking_for(req_pgno, req_subno);
    bool req_drawn = false;

    struct input input = {
        .text = {0},
        .position = 0,
        .status = INPUT_NORMAL,
    };
    draw_input(&input);

    refresh();

    while (true) {
        struct pollfd fds[2] = {
            { .fd = STDIN_FILENO, .events = POLLIN },
            { .fd = vbi->fd, .events = POLLIN, .revents = POLLIN },
        };
        nfds_t nfds = 1 + (vbi->fd >= 0);
        int timeout = (vbi->fd >= 0) ? -1 : 100;
        int rs = poll(fds, nfds, timeout);
        if (rs == -1) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (fds[1].revents)
            dochttx_vbi_read_data(vbi);
        if (fds[0].revents) {
            bool do_quit = false;
            int chr = getch();
            switch (chr) {
            case L'X'-L'@': // Ctrl + X
            case L'C'-L'@': // Ctrl + C
                do_quit = true;
                break;
            case KEY_LEFT:
                if (input.status == INPUT_VALIDATED_JUST_NOW)
                    input.status = INPUT_VALIDATED;
                if (input.position > 0)
                    input.position--;
                break;
            case KEY_RIGHT:
                if (input.status == INPUT_VALIDATED_JUST_NOW)
                    input.status = INPUT_VALIDATED;
                if (input.text[input.position] != '\0')
                    input.position++;
                break;
            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
                chr = chr - 'a' + 'A';
                /* fall through */
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                if (input.status == INPUT_VALIDATED_JUST_NOW) {
                    memset(input.text, 0, sizeof input.text);
                    input.text[0] = (char) chr;
                    input.position = 1;
                } else
                /* fall through */
            case '.':
                if (input.text[sizeof input.text - 2] == '\0') {
                    memmove(
                        input.text + input.position + 1,
                        input.text + input.position,
                        sizeof input.text - input.position
                    );
                    input.text[input.position++] = (char) chr;
                }
                input.status = INPUT_NORMAL;
                break;
            case KEY_DC:
                if (input.position >= sizeof input.text - 1)
                    break;
                input.position++;
                /* fall through */
            case KEY_BACKSPACE:
            case '\x7F':
            case '\b':
                if (input.position == 0)
                    break;
                memmove(
                    input.text + input.position - 1,
                    input.text + input.position,
                    sizeof input.text - input.position
                );
                input.position--;
                input.status = INPUT_NORMAL;
                break;
            case KEY_ENTER:
            case '\n':
            case '\r':
                {
                    unsigned int new_pgno, new_subno;
                    if (parse_pagespec(input.text, &new_pgno, &new_subno) >= 0) {
                        draw_looking_for(new_pgno, new_subno);
                        req_pgno = new_pgno;
                        req_subno = new_subno;
                        req_drawn = false;
                        input.status = INPUT_VALIDATED_JUST_NOW;
                    } else
                        input.status = INPUT_ERROR;
                }
                break;
            }
            if (do_quit)
                break;
        }
        if (!req_drawn) {
            vbi_subno shown_subno = dochttx_vbi_render(vbi->dec, req_pgno, req_subno, 25);
            if (shown_subno >= 0) {
                draw_showing_page(vbi->dec, req_pgno, shown_subno);
                req_drawn = true;
            }
        }
        if (!cur_drawn) {
            int lines = 1;
            if (req_pgno == cur_pgno && (req_subno == VBI_ANY_SUBNO || req_subno == cur_subno))
                lines = 25;
            vbi_subno shown_subno = dochttx_vbi_render(vbi->dec, cur_pgno, cur_subno, lines);
            if (shown_subno >= 0 && lines > 1)
                draw_showing_page(vbi->dec, cur_pgno, cur_subno);
            cur_drawn = true;
        }
        draw_input(&input);
        refresh();
    }
    dochttx_ncurses_quit();
    dochttx_vbi_close(vbi);
    dochttx_locale_quit();
    return EXIT_SUCCESS;
}

// vim:ts=4 sts=4 sw=4 et
