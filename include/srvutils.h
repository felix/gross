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

#ifndef SRVUTILS_H
#define SRVUTILS_H

#include <pthread.h>
#include <syslog.h>

#include "common.h"

#define DATESTRLEN 26

#define EXIT_NOERROR 		0
#define EXIT_USAGE 		1
#define EXIT_FATAL		2
#define EXIT_PIDFILE_EXISTS 	3
#define EXIT_CONFIG		4

#define DETACH			1

enum logmsgtype_t
{
	LOG_TYPE = 0x10000,
	GLOG_EMERG = LOG_TYPE | LOG_EMERG,
	GLOG_ALERT = LOG_TYPE | LOG_ALERT,
	GLOG_CRIT = LOG_TYPE | LOG_CRIT,
	GLOG_ERROR = LOG_TYPE | LOG_ERR,
	GLOG_WARNING = LOG_TYPE | LOG_WARNING,
	GLOG_NOTICE = LOG_TYPE | LOG_NOTICE,
	GLOG_INFO = LOG_TYPE | LOG_INFO,
	GLOG_DEBUG = LOG_TYPE | LOG_DEBUG,
	GLOG_INSANE = LOG_TYPE | (LOG_DEBUG + 1),
	GLOG_FULL = 0x1ffff,
	ACCT_TYPE = 0x20000,
	ACCT_GREY = 0x20001,
	ACCT_MATCH = 0x20002,
	ACCT_TRUST = 0x20004,
	ACCT_DNS_TMOUT = 0x20008,
	ACCT_DNS_MATCH = 0x20010,
	ACCT_DNS_SKIP = 0x20020,
	ACCT_DNS_QUERY = 0x20040,
	ACCT_FULL = 0x2ffff,
	STATS_NONE = 0x40000,
	STATS_STATUS = 0x40001,
	STATS_STATUS_BEGIN = 0x40002,
	STATS_DELAY = 0x40004,
	STATS_DNSBL = 0x40008,
	STATS_FULL = 0x4ffff
};

enum
{ UPDATE = 1, ROTATE, ABSOLUTE_UPDATE, SYNC_AGGREGATE, UPDATE_OPER };
typedef enum
{ J_UNDEFINED, J_SUSPICIOUS, J_BLOCK, J_PASS } judgment_t;

#define MAXFD 			64
#define FILTER_SIZE 		((uint32_t)32)

#define ACTIVATE_SYNC_GUARD() sem_wait(ctx->sync_guard) 
#define RELEASE_SYNC_GUARD() sem_post(ctx->sync_guard)
#define ACTIVATE_BLOOM_GUARD() pthread_mutex_lock(&ctx->bloom_guard)
#define RELEASE_BLOOM_GUARD() pthread_mutex_unlock(&ctx->bloom_guard)

typedef struct
{
	long mtype;
	char mtext[MSGSZ];
} log_message_t;

typedef struct
{
	long mtype;
	char mtext[MSGSZ];
} update_message_t;

typedef struct
{
	long mtype;
	void *result;
} poolresult_message_t;

/* global context */
extern gross_ctx_t *ctx;

int logstr(int level, const char *fmt, ...);
int logmsg(log_message_t *mbuf);

int statstr(int level, const char *fmt, ...);

void daemon_shutdown(int return_code, const char *fmt, ...);
void daemon_fatal(const char *reason);
int connected(peer_t *peer);
bloom_ring_queue_t *build_bloom_ring(unsigned int num, bitindex_t num_bits);
void release_bloom_ring_queue(bloom_ring_queue_t *brq);
void daemonize(void);
void *Malloc(size_t size);
void *create_thread(thread_info_t *tinfo, int detach, void *(*routine) (void *), void *arg);
void register_check(thread_pool_t *pool, bool definitive);
char *ipstr(struct sockaddr_in *saddr);
void create_statefile(void);
void check_pidfile(void);
void create_pidfile(void);

#endif
