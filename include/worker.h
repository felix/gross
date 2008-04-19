/* $Id$ */

/*
 * Copyright (c) 2006,2007,2008
 *               Eino Tuominen <eino@utu.fi>
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
#include "srvutils.h"

#define MAXCONNQ 5

typedef enum
{ STATUS_GREY, STATUS_MATCH, STATUS_TRUST, STATUS_UNKNOWN, STATUS_FAIL, STATUS_BLOCK } grey_status_t;

#define LEGALREASONCHARACTERS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890 .-_@";

typedef struct chkresult_s
{
	bool definitive;
	bool wait;
	int weight;
	judgment_t judgment;
	char *reason;
	const char *checkname;
} chkresult_t;

typedef struct check_match_s
{
	const char *name;
	int weight;
	struct check_match_s *next;	/* linked list */
} check_match_t;

typedef struct querylog_entry_s
{
	int action;
	int delay;
	int totalweight;
	const char *proto;
	const char *client_ip;
	const char *sender;
	const char *recipient;
	const char *helo;
	check_match_t *match;
} querylog_entry_t;

typedef struct final_status_s
{
	char *reason;
	grey_status_t status;
	querylog_entry_t querylog_entry;
	struct timespec starttime;
} final_status_t;

typedef struct client_info_s
{
	int connfd;
	struct sockaddr_in *caddr;
	char *ipstr;
	int msglen;
	void *message;
	bool single_query;
} client_info_t;

typedef struct grey_tuple_s
{
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
void record_match(querylog_entry_t *q, chkresult_t *r);
final_status_t *init_status(const char *proto);
void querylogwrite(querylog_entry_t *q);
void finalize(final_status_t *status);
void querylogwrite(querylog_entry_t *q);
void update_delay_stats(querylog_entry_t *q);

#endif /* #ifndef WORKER_H */
