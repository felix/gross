/* $Id$ */

/*
 * Copyright (c)2008
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

#include "common.h"
#include "srvutils.h"
#include "msgqueue.h"
#include "utils.h"
#include "lookup3.h"

#include <netdb.h>

/*
 * cache hash table
 * this is the simplest possible table: don't bother
 * about collisions, just write over.
 */
typedef struct cache_data_s {
	char *key;
	struct hostent *value;
	struct timespec savetime;
} cache_data_t;

#define HASHSIZE hashsize(10)
#define HASHMASK hashmask(10)
#define CACHETIME 60 		/* 1 min */
cache_data_t *hashtab[HASHSIZE] = { '\0' };
pthread_mutex_t cache_mx = PTHREAD_MUTEX_INITIALIZER;

#define CACHE_LOCK { pthread_mutex_lock(&cache_mx); }
#define CACHE_UNLOCK { pthread_mutex_unlock(&cache_mx); }

typedef struct dns_cba_s
{
	int response_q;
} dns_cba_t;

typedef struct dns_reply_s
{
	struct hostent *host;
} dns_reply_t;

/* internal functions */
struct hostent *lookup_str(const char *key);
struct hostent *hostent_deepcopy(struct hostent *src);
static void
#if ARES_VERSION_MAJOR > 0 && ARES_VERSION_MINOR > 4
Gethostbyname_cb(void *arg, int status, int timeouts, struct hostent *host);
#else
Gethostbyname_cb(void *arg, int status, struct hostent *host);
#endif
void cache_str(char *key, struct hostent *value);

struct hostent *
hostent_deepcopy(struct hostent *src)
{
	struct hostent *dst = Malloc(sizeof(struct hostent));

	assert(src);

	if (src->h_name)
		dst->h_name = strdup(src->h_name);
	return dst;
}

static void
#if ARES_VERSION_MAJOR > 0 && ARES_VERSION_MINOR > 4
Gethostbyname_cb(void *arg, int status, int timeouts, struct hostent *host)
#else
Gethostbyname_cb(void *arg, int status, struct hostent *host)
#endif
{
        dns_cba_t *cba;
	dns_reply_t *reply;

        cba = (dns_cba_t *)arg;
	reply = Malloc(sizeof(dns_reply_t));

        if (status == ARES_SUCCESS)
		reply->host = hostent_deepcopy(host);
	else
		reply->host = NULL;
	put_msg(cba->response_q, reply, sizeof(dns_reply_t));
}

void
cache_str(char *key, struct hostent *value)
{
	/* ub4 hashvalue = one_at_a_time(key, strlen(key)); */
	uint32_t hashvalue = hashfunc(key, strlen(key));

	cache_data_t *node;

	CACHE_LOCK;
	node = hashtab[hashvalue];
	/*
	 * if we find a collision, free the old one
	 * OTOH, the data must be copied when read as it
	 * can be freed() at any time.
	 */
	if (node != NULL) {
		Free(node->key);
		Free(node->value);
	} else {
		node = Malloc(sizeof(cache_data_t));
	}
	node->key = key;
	node->value = value;
	clock_gettime(CLOCK_TYPE, &node->savetime);
	hashtab[hashvalue] = node;
	CACHE_UNLOCK;
}

struct hostent *
lookup_str(const char *key)
{
	/* ub4 hashvalue = one_at_a_time(key, strlen(key)); */
	uint32_t hashvalue = hashfunc(key, strlen(key));
	struct hostent *host;

	cache_data_t *node;
	struct timespec now;

	CACHE_LOCK;
	node = hashtab[hashvalue];

	if (NULL == node) {
		CACHE_UNLOCK;
		return NULL;				/* not found */
	}

	if (strcmp(node->key, key) == 0) {
		/*
		 * we must do a deep copy as the data can be freed() at any time
		 * Also, make sure the data isn't stale
		 */
		clock_gettime(CLOCK_TYPE, &now);
		if (ms_diff(&now, &node->savetime) > CACHETIME * 1000) {
			/*
			 * stale, we don't have to remove the entry because
			 * collision later will update the data, but in
			 * case host is not found anymore will lead to unnecessary
			 * clock_gettime() calls
                         */
			Free(node->key);
			Free(node->value);
			Free(node);
			hashtab[hashvalue] = NULL;
			CACHE_UNLOCK;
			return NULL;
		} else {
			host = hostent_deepcopy(node->value);	/* found */
			CACHE_UNLOCK;
			return host;
		}
	} else {
		CACHE_UNLOCK;
		return NULL; 				/* collision */
	}
}

struct hostent *
Gethostbyname(const char *name, mseconds_t timeout)
{

	dns_cba_t cba;
	ares_channel *channel;
	size_t size;
	dns_reply_t reply;
	struct hostent *entry;

	entry = lookup_str(name);

	if (NULL == entry) {
		channel = ctx->dns_channel;
		cba.response_q = get_queue();
		ares_gethostbyname(*channel, name, PF_INET, &Gethostbyname_cb, &cba);
		/* send a byte via pipe to wake up the select loop */
		size = write(ctx->dns_wake, "w", 1);
		if (size != 1)
			daemon_fatal("write");
		/* wait for the reply via message queue */
		size = get_msg_timed(cba.response_q, &reply, sizeof(dns_reply_t), timeout);
		release_queue(cba.response_q);

		if (size > 0) {
			if (reply.host)
				cache_str(strdup(name), reply.host);
			return reply.host;
		} else
			return NULL;
	} else {
		return entry;
	}
}

void *
helper_dns(void *arg)
{
	struct timeval tv;
        fd_set readers, writers;
	int nfds = 0;
	int count;
	ares_channel *channel;
	int pipefd[2];
	int ret;
	int dns_wake;
	char buf[MAXLINELEN];
	ssize_t size;

        ret = pthread_mutex_lock(&ctx->locks.helper_dns_guard.mx);
        assert(ret == 0);

	channel = Malloc(sizeof(*channel));
	if (ares_init(channel) != ARES_SUCCESS)
		daemon_fatal("ares_init");

	ctx->dns_channel = channel;

	/*
	 * pipe for wakening helper from select()
	 */
	ret = pipe(pipefd);
	if (ret < 0)
		daemon_fatal("pipe");
	dns_wake = pipefd[0];
	ctx->dns_wake = pipefd[1];

	/* ready to serve */
	pthread_mutex_unlock(&ctx->locks.helper_dns_guard.mx);
	pthread_cond_signal(&ctx->locks.helper_dns_guard.cv);

	for (;;) {
		FD_ZERO(&readers);
		FD_ZERO(&writers);
		/* nfds = ares_fds(*channel, &readers, &writers); */
		FD_SET(dns_wake, &readers);
		nfds = MAX(nfds, dns_wake + 1);

		/* ares knows the next timeout */
		ares_timeout(*channel, NULL, &tv);
		
		printf("select\n");
		count = select(nfds, &readers, &writers, NULL, &tv);
		if (count < 0) {
			daemon_fatal("select");
		} else {
			/* job to do or timeout */
			if (FD_ISSET(dns_wake, &readers)) {
				logstr(GLOG_INSANE, "helper_dns: received a wake up call");
				printf("helper_dns: received a wake up call\n");
				/* consume the wake up call */
				size = read(dns_wake, buf, 1);
				if (size != 1)
					daemon_fatal("read");
			}
			/* clear out our wakening fd */
			FD_CLR(dns_wake, &readers);
			ares_process(*channel, &readers, &writers);
		}
			
	}
	/* never reached */
}

void
helper_dns_init()
{
	int ret;

	logstr(GLOG_INFO, "starting dns helper thread");
	
	pthread_mutex_init(&ctx->locks.helper_dns_guard.mx, NULL);
	pthread_cond_init(&ctx->locks.helper_dns_guard.cv, NULL);

	ret = pthread_mutex_lock(&ctx->locks.helper_dns_guard.mx);
	assert(ret == 0);

	create_thread(&ctx->process_parts.helper_dns, DETACH, &helper_dns, NULL);

	/* wait until the helper thread is ready to serve */
	ret = pthread_cond_wait(&ctx->locks.helper_dns_guard.cv, &ctx->locks.helper_dns_guard.mx);
	pthread_mutex_unlock(&ctx->locks.helper_dns_guard.mx);
}
