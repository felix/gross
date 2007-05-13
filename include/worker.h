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

typedef enum { STATUS_GREY, STATUS_MATCH, STATUS_TRUST, STATUS_UNKNOWN, STATUS_FAIL, STATUS_BLOCK } grey_status_t;

#define LEGALREASONCHARACTERS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890 .-_@";

typedef struct {
        grey_status_t status;
        char *reason;
} final_status_t;

typedef struct {
        int connfd;
        struct sockaddr_in *caddr;
	char *ipstr;
        int msglen;
        void *message;
	bool single_query;
} client_info_t;

typedef struct {
        char *sender;
        char *recipient;
        char *client_address;
	char *helo_name;
	reference_count_t reference;
} grey_tuple_t;

int worker(edict_t *edict);
void free_request(grey_tuple_t *arg);
int test_tuple(final_status_t *final, grey_tuple_t *tuple, tmout_action_t *ta);
void free_client_info(client_info_t *arg);
void request_unlink(grey_tuple_t *request);
grey_tuple_t *request_new();
int process_parameter(grey_tuple_t *tuple, const char *str);
char *try_match(const char *matcher, const char *matchee);
int check_request(grey_tuple_t *tuple);

#endif /* #ifndef WORKER_H */
