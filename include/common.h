/* $Id$ */

/*
 * Copyright (c) 2006,2007,2008
 *               Eino Tuominen <eino@utu.fi>
 *               Antti Siira <antti@utu.fi>
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

#ifndef COMMON_H
#define COMMON_H

/* autoconf */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/*
 * common system includes
 */

/* socket(), inet_pton() etc */
#include <sys/types.h>
#include <sys/socket.h>
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#include <arpa/inet.h>

#include <assert.h>
#include <string.h>		/* memcpy(), memset() etc */
#include <stdlib.h>		/* malloc(), atoi() etc */
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <semaphore.h>
#include <time.h>

#ifdef HAVE_ARES_H
# include <ares.h>
# include <ares_version.h>
# define DNSBL
#endif

#ifdef HAVE_LIBMILTER
# define MILTER
#endif

#ifdef HAVE_SPF2_SPF_H
# define SPF
#endif
#if PROTOCOL == POSTFIX
# define WORKER_PROTO_TCP
#elif PROTOCOL == SJSMS
# define WORKER_PROTO_UDP
#else
# error "No PROTOCOL defined!"
#endif

/* what clock type to use */
#if defined USE_GETTIMEOFDAY
# define CLOCK_TYPE CLOCK_KLUDGE
#elif defined USE_CLOCK_MONOTONIC
# define CLOCK_TYPE CLOCK_MONOTONIC
#elif defined USE_CLOCK_HIGHRES
# define CLOCK_TYPE CLOCK_HIGHRES
#elif defined USE_CLOCK_REALTIME
# define CLOCK_TYPE CLOCK_REALTIME
#else
# error "No suitable clock type found (should not happen)"
#endif

/*
 * project includes 
 */
#include "bloom.h"
#include "stats.h"
#include "thread_pool.h"

/*
 * common defines and macros
 */
#define MSGSZ           1024
#define MAXLINELEN      MSGSZ
#define GROSSPORT	5525	/* default port for server */

#define STARTUP_SYNC ((uint32_t)0x00)
#define OPER_SYNC ((uint32_t)0x01)
#define AGGREGATE_SYNC ((uint32_t)0x02)

#define FLG_NODAEMON (int)0x0001
#define FLG_NOREPLICATE (int)0x0002
#define FLG_UPDATE_ALWAYS (int)0x0004
#define FLG_CREATE_STATEFILE (int)0x0008
#define FLG_DRYRUN (int)0x0010
#define FLG_SYSLOG (int)0x0020
#define FLG_CHECK_PIDFILE (int)0x0040
#define FLG_CREATE_PIDFILE (int)0x0080
#define FLG_MATCH_SHORTCUT (int)0x0100
#define FLG_RECONFIGURE_PENDING (int)0x0200

#define CHECK_DNSBL (int)0x0001
#define CHECK_BLOCKER (int)0x0002
#define CHECK_RANDOM (int)0x0004
#define CHECK_RHSBL (int)0x0008
#define CHECK_DNSWL (int)0x0010
#define CHECK_SPF (int)0x0020
#define CHECK_REVERSE (int)0x0040
#define CHECK_HELO (int)0x0080

#define PROTO_SJSMS (int)0x01
#define PROTO_POSTFIX (int)0x02
#define PROTO_MILTER (int)0x04

#define TMP_BUF_SIZE ((uint32_t)640)	/* 640 should be enough for everyone */
#define THREAD_STACK_SIZE ((size_t)(1024 * 1024))	/* one megabyte */

/* A few utility macros */
#define Free(a) { assert(a); free(a); a = NULL; }
#ifndef MAX
#define MAX(a,b) 	((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) 	((a) < (b) ? (a) : (b))
#endif

/*
 * common types
 */

#ifndef HAVE_BOOL
# ifndef bool
#  ifndef __bool_true_false_are_defined
#   define __bool_true_false_are_defined       1
typedef int bool;

#  define true 1
#  define false 0
#  endif /* __bool_true_false_are_defined */
# endif	/* bool */
#endif /* HAVE_BOOL */

typedef enum
{
	GREY_TUPLE_USER = 0,
	GREY_TUPLE_SERVER,
} greytupletype_t;

typedef struct peer_s
{
	struct sockaddr_in peer_addr;
	pthread_mutex_t peer_in_mutex;
	int peerfd_in;
	int peerfd_out;
	int connected;
} peer_t;

typedef struct sjsms_config_s
{
	char *responsegrey;
	char *responsematch;
	char *responsetrust;
	char *responseblock;
} sjsms_config_t;

typedef struct postfix_config_s
{
	char *responsegrey;
	char *responseblock;
} postfix_config_t;

typedef struct blocker_config_s
{
	struct sockaddr_in server;
	int weight;
} blocker_config_t;

#ifdef MILTER
typedef struct milter_config_s
{
	char *listen;
} milter_config_t;
#endif /* MILTER */

typedef struct
{
	struct sockaddr_in gross_host;
	struct sockaddr_in sync_host;
	struct sockaddr_in status_host;
	peer_t peer;
	int max_connq;
	time_t rotate_interval;
	time_t stat_interval;
	bitindex_t filter_size;
	unsigned int num_bufs;
	char *statefile;
	int loglevel;
	int syslogfacility;
	int statlevel;
	int flags;
	int checks;
	int grey_mask;
	int protocols;
	int greylist_delay;
	greytupletype_t grey_tuple;
	postfix_config_t postfix;
	sjsms_config_t sjsms;
	blocker_config_t blocker;
	mseconds_t query_timelimit;
	int grey_threshold;
	int block_threshold;
	int pool_maxthreads;
	char *grey_reason;
	char *block_reason;
	char *pidfile;
#ifdef MILTER
	milter_config_t milter;
#endif				/* MILTER */
} gross_config_t;

#ifdef DNSBL
typedef struct dnsbl_s
{
	const char *name;
	int weight;
	int tolerancecounter;
	struct dnsbl_s *next;	/* linked list */
} dnsbl_t;
#endif /* DNSBL */

typedef void (*tmout_action) (void *arg, mseconds_t timeused);

/* timeout action list */
typedef struct tmout_action_s
{
	mseconds_t timeout;	/* milliseconds */
	tmout_action action;
	void *arg;
	struct tmout_action_s *next;
} tmout_action_t;

typedef struct
{
	pthread_t *thread;
	/*time_t watchdog; */
} thread_info_t;

typedef struct
{
	thread_info_t bloommgr;
	thread_info_t syncmgr;
	thread_info_t postfix_server;
	thread_info_t sjsms_server;
	thread_info_t milter_server;
	thread_info_t helper_dns;
} thread_collection_t;

#define MAXCHECKS 128

typedef struct
{
	thread_pool_t *pool;
	bool definitive;
	char *name;
	void (*init_routine) (void *, pool_limits_t *);
	void *check_arg;
} check_t;

typedef struct statefile_info_s
{
	int fd;
} statefile_info_t;

typedef struct lock_s
{
	pthread_mutex_t mx;
	pthread_cond_t cv;
} g_lock_t;

typedef struct thread_locks_s
{
	sem_t *sync_guard;
        g_lock_t bloom_guard;
        g_lock_t update_guard;
        g_lock_t helper_dns_guard;
} thread_locks_t; 

typedef struct gross_ctx_s
{
	bloom_ring_queue_t *filter;
	int update_q;
	thread_locks_t locks;
	time_t *last_rotate;
#ifdef DNSBL
	dnsbl_t *dnsbl;
	dnsbl_t *dnswl;
	dnsbl_t *rhsbl;
	ares_channel *dns_channel;
	int dns_wake;
#endif				/* ENDBL */
	gross_config_t config;
	mmapped_brq_t *mmap_info;
	statefile_info_t *statefile_info;
	thread_collection_t process_parts;
	stats_t stats;
	check_t *checklist[MAXCHECKS];
	bool syslog_open;
} gross_ctx_t;

#ifndef HAVE_USECONDS_T
typedef unsigned long useconds_t;
#endif /* HAVE_USECONDS_T */

#ifdef GLOG_ERROR
# define gerror(a) logstr(GLOG_ERROR, "%s: %s", a, strerror(errno))
#else
# define gerror(a) perror(a)
#endif

extern int cleanup_in_progress;

#endif
