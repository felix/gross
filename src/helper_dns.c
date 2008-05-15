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

#include <netdb.h>

typedef  unsigned long  int  ub4;

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

#define HASHSIZE 0x0000ffff

cache_data_t *hashtab[HASHSIZE] = { '\0' };

ub4 mask = HASHSIZE;

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
ub4 one_at_a_time(const char *key, ub4 len);
struct hostent *hostent_deepcopy(struct hostent *src);
static void
#if ARES_VERSION_MAJOR > 0 && ARES_VERSION_MINOR > 4
Gethostbyname_cb(void *arg, int status, int timeouts, struct hostent *host);
#else
Gethostbyname_cb(void *arg, int status, struct hostent *host);
#endif
void cache_str(char *key, struct hostent *value);

/*
 * A simple hash function from http://www.burtleburtle.net/bob/hash/doobs.html
 */
ub4
one_at_a_time(const char *key, ub4 len)
{
	ub4   hash, i;
	for (hash=0, i=0; i<len; ++i) {
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return (hash & mask);
} 

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
	ub4 hashvalue = one_at_a_time(key, strlen(key));
	cache_data_t *node;

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
}

struct hostent *
lookup_str(const char *key)
{
	ub4 hashvalue = one_at_a_time(key, strlen(key));
	cache_data_t *node;
	void *reply;

	node = hashtab[hashvalue];

	if (NULL == node)
		return NULL;				/* not found */

	if (strcmp(node->key, key) == 0) {
		/* we must do a deep copy as the data can be freed() at any time */
		return hostent_deepcopy(node->value);	/* found */
	} else {
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
	int rq;
	struct hostent *entry;

	entry = lookup_str(name);

	if (NULL == entry) {
		channel = ctx->dns_channel;
		cba.response_q = get_queue();

		ares_gethostbyname(*channel, name, PF_INET, &Gethostbyname_cb, &cba);
		size = get_msg_timed(cba.response_q, &reply, sizeof(dns_reply_t), timeout);
		release_queue(cba.response_q);

		if (size > 0) {
			cache_str(strdup(name), reply.host);
			return reply.host;
		} else
			return NULL;
	} else {
		cache_str(strdup(name), entry);
		return entry;
	}
}

void *
helper_dns(void *arg)
{
	struct timeval tv;
        fd_set readers, writers;
	int nfds;
	int count;
	ares_channel *channel;
	int ret;

        ret = pthread_mutex_lock(&ctx->locks.helper_dns_guard.mx);
        assert(ret == 0);

	channel = Malloc(sizeof(*channel));
	if (ares_init(channel) != ARES_SUCCESS)
		daemon_fatal("ares_init");

	ctx->dns_channel = channel;

	/* ready to serve */
	pthread_mutex_unlock(&ctx->locks.helper_dns_guard.mx);
	pthread_cond_signal(&ctx->locks.helper_dns_guard.cv);

	for (;;) {
		FD_ZERO(&readers);
		FD_ZERO(&writers);
		nfds = ares_fds(*channel, &readers, &writers);

		if (nfds == 0)
		ares_timeout(*channel, NULL, &tv);
		
		count = select(nfds, &readers, &writers, NULL, &tv);
		ares_process(*channel, &readers, &writers);
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

	/* wait until helper thread is ready to serve */
	ret = pthread_cond_wait(&ctx->locks.helper_dns_guard.cv, &ctx->locks.helper_dns_guard.mx);
	pthread_mutex_unlock(&ctx->locks.helper_dns_guard.mx);
}
