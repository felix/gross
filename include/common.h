/*
 * Copyright (c) 2006,2007
 *                    Eino Tuominen <eino@utu.fi>
 *                    Antti Siira <antti@utu.fi>
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

#ifndef bool
  typedef int bool;
# define true 1
# define false 0
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
#include <string.h> 	/* memcpy(), memset() etc */
#include <stdlib.h>	/* malloc(), atoi() etc */
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <semaphore.h>
#include <time.h>

#ifdef HAVE_ARES_H
# include <ares.h>
# define DNSBL
#endif

#if PROTOCOL == POSTFIX
# define WORKER_PROTO_TCP
#elif PROTOCOL == SJSMS
# define WORKER_PROTO_UDP
#else
# error "No PROTOCOL defined!"
#endif

/* what clock type to use */
#if HAVE_DECL_CLOCK_MONOTONIC
# define CLOCK_TYPE CLOCK_MONOTONIC
#elif HAVE_DECL_CLOCK_HIGHRES
# define CLOCK_TYPE CLOCK_HIGHRES
#else
# ifndef HAVE_CLOCK_GETTIME
#  define CLOCK_TYPE CLOCK_KLUDGE
# else
#  error No suitable clock type found (CLOCK_MONOTONIC or CLOCK_HIGHRES)
# endif
#endif

/*
 * project includes 
 */
#include "bloom.h"
#include "stats.h"

/*
 * common defines and macros
 */
#define MSGSZ           1024
#define MAXLINELEN      MSGSZ
#define GROSSPORT	1111	/* default port for server */

#define STARTUP_SYNC ((uint32_t)0x00)
#define OPER_SYNC ((uint32_t)0x01)
#define AGGREGATE_SYNC ((uint32_t)0x02)

#define FLG_NODAEMON (int)0x01
#define FLG_NOREPLICATE (int)0x02
#define FLG_UPDATE_ALWAYS (int)0x04
#define FLG_CREATE_STATEFILE (int)0x08
#define FLG_DRYRUN (int)0x10
#define FLG_SYSLOG (int)0x20

#ifndef MAX
#define MAX(a,b) 	((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) 	((a) < (b) ? (a) : (b))
#endif

/*
 * common types
 */
typedef struct {
	struct sockaddr_in peer_addr;
	pthread_mutex_t peer_in_mutex;
	int peerfd_in;
	int peerfd_out;
	int connected;
} peer_t;

#if PROTOCOL == SJSMS
typedef struct {
	char *responsegrey;
	char *responsematch;
	char *responsetrust;
} sjsms_config_t;
#endif /* PROTOCOL == SJSMS */

typedef struct {
	struct sockaddr_in gross_host;
	struct sockaddr_in sync_host;
	struct sockaddr_in status_host;
	peer_t peer;
	int max_connq;
	int max_threads;
	time_t rotate_interval;
	time_t stat_interval;
	bitindex_t filter_size;
	unsigned int num_bufs;
	char *statefile;
	int loglevel;
	int syslogfacility;
        int statlevel;
	int acctmask;
	int flags;
#if PROTOCOL == SJSMS
	sjsms_config_t sjsms;
#endif /* PROTOCOL == SJSMS */
} gross_config_t;

#ifdef DNSBL
typedef struct dnsbl_s {
        const char *name;
        int weight;
	int tolerancecounter;
        struct dnsbl_s *next; /* linked list */
} dnsbl_t;
#endif /* DNSBL */

typedef int mseconds_t;
typedef void (*tmout_action)(void *arg, mseconds_t timeused);

/* timeout action list */
typedef struct tmout_action_s {
        mseconds_t timeout;             /* milliseconds */
        tmout_action action;
        void *arg;
        struct tmout_action_s *next;
} tmout_action_t;

typedef struct {
  pthread_t* thread;
  time_t watchdog;
} thread_info_t;

typedef struct {
  thread_info_t bloommgr;
  thread_info_t syncmgr;
  thread_info_t worker;
} thread_collection_t;

typedef struct {
        bloom_ring_queue_t *filter;
        int log_q;
        int update_q;
        sem_t* sync_guard;
        pthread_mutex_t bloom_guard;
        time_t* last_rotate;
#ifdef DNSBL
        dnsbl_t *dnsbl;
#endif /* ENDBL */
        gross_config_t config;
        mmapped_brq_t *mmap_info;
        thread_collection_t process_parts;
        stats_t stats;
} gross_ctx_t;

#ifndef HAVE_USECONDS_T
typedef unsigned long useconds_t;
#endif /* HAVE_USECONDS_T */

extern int cleanup_in_progress;

#endif
