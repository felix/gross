/*
 * Copyright (c) 2006,2007 Eino Tuominen <eino@utu.fi>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef WORKER_H
#define WORKER_H

#include "thread_pool.h"

#define MAXCONNQ 5

enum grey_status_t { STATUS_GREY, STATUS_MATCH, STATUS_TRUST, STATUS_UNKNOWN, STATUS_FAIL };

typedef struct {
        int connfd;
        struct sockaddr_in *caddr;
	char *ipstr;
        int msglen;
        void *message;
} client_info_t;

typedef struct {
        char *sender;
        char *recipient;
        char *client_address;
} grey_tuple_t;

int worker(edict_t *edict);
void free_request(grey_tuple_t *arg);
int test_tuple(grey_tuple_t *tuple, tmout_action_t *ta);
void free_client_info(client_info_t *arg);

#endif /* #ifndef WORKER_H */
