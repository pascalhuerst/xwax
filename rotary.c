/*
 * Copyright (C) 2024 Mark Hills <mark@xwax.org>
 *
 * This file is part of "xwax".
 *
 * "xwax" is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * "xwax" is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

/*
 * Specialised functions for the Novation Dicer controller
 *
 * The Dicer is a standard MIDI device, with buttons on input and the
 * corresponding LEDs on output. A single MIDI device consists of two
 * units, one for each turntable.
 *
 * Each unit has 5 buttons, but there are three 'pages' of buttons
 * controlled in the firmware, and then a shift mode for each. So we
 * see the full MIDI device as 60 possible buttons.
 */

#include <stdlib.h>

#include "controller.h"
#include "debug.h"
#include "deck.h"
#include "realtime.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

struct rotary
{
    struct deck *left, *right;
    int fd;
};

static int add_deck(struct controller *c, struct deck *k)
{
    struct rotary *r = c->local;

    if (r->left != NULL && r->right != NULL)
        return -1;

    if (r->left == NULL)
    {
        r->left = k;
    }
    else
    {
        r->right = k;
    }

    return 0;
}

static ssize_t pollfds(struct controller *c, struct pollfd *pe, size_t z)
{
    struct rotary *r = c->local;

    if (z < 1)
    {
        return -1;
    }

    pe[0].fd = r->fd;
    pe[0].events = POLLIN;

    return 1;
}

/*
 * Handler in the realtime thread, which polls on both input
 * and output
 */

static int realtime(struct controller *c)
{
    struct rotary *r = c->local;
    struct input_event event;
    ssize_t bytes_read;

    while ((bytes_read = read(r->fd, &event, sizeof(event))) > 0)
    {
        if (bytes_read < (ssize_t)sizeof(event))
        {
            perror("failed to read event");
            return -1;
        }

        printf("have event: type=%d, code=%d, value=%d\n", event.type, event.code, event.value);
    }

    // if (event.type == EV_REL && event.code == REL_DIAL)
    //{
    //     printf("Rotary encoder moved: %d\n", event.value);
    // }
    // else if (event.type == EV_KEY)
    //{
    //     printf("Button event: code=%d, value=%d\n", event.code, event.value);
    // }
    // else if (event.type == EV_SYN)
    //{
    //     printf("Sync event\n");
    // }

    return 0;
}

static void clear(struct controller *c)
{
    struct rotary *r = c->local;

    close(r->fd);
    free(c->local);
}

static struct controller_ops rotary_ops = {
    .add_deck = add_deck,
    .pollfds = pollfds,
    .realtime = realtime,
    .clear = clear,
};

int rotary_init(struct controller *c, struct rt *rt, const char *hw)
{
    struct rotary *r;

    r = malloc(sizeof *r);
    if (r == NULL)
    {
        perror("malloc");
        return -1;
    }

    r->left = NULL;
    r->right = NULL;

    r->fd = open(hw, O_RDONLY);
    if (r->fd < 0)
    {
        perror("failed to open device");
        goto fail;
    }

    // Empty the file
    while (1)
    {
        char buf[1];
        ssize_t bytes_read = read(r->fd, buf, 1);
        if (bytes_read <= 0)
        {
            break;
        }
    }


    if (controller_init(c, &rotary_ops, r, rt) == -1)
        goto fail;

    return 0;

fail:
    free(r);
    return -1;
}
