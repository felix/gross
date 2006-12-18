/*
 * Copyright (c) 2006 Eino Tuominen <eino@utu.fi>
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

#define MAXCONNQ 5

enum grey_status_t { STATUS_GREY, STATUS_MATCH, STATUS_TRUST };

typedef struct {
        int connfd;
        struct sockaddr_in *caddr;
	char *ipstr;
#ifdef WORKER_PROTO_UDP
        int msglen;
        void *message;
#endif
} client_info_t;

typedef struct {
        char *sender;
        char *recipient;
        char *client_address;
} grey_tuple_t;

static void *postfix_policy_server(void *arg);
static void *worker(void *arg);
void free_request(grey_tuple_t *arg);
int test_tuple(grey_tuple_t *tuple, tmout_action_t *ta);
void free_client_info(client_info_t *arg);

/* function must be implemented in worker_[proto].c */
int handle_connection(client_info_t *arg);

#endif /* #ifndef WORKER_H */
