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
#include "helper_dns.h"

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
default_cb(void *arg, int status, int timeouts, struct hostent *host);
#else
default_cb(void *arg, int status, struct hostent *host);
#endif
void cache_str(char *key, struct hostent *value);
int count_ptrs(char **ptr);

struct hostent *
hostent_deepcopy(struct hostent *src)
{
	int i, alias_count, addr_count = 0;

	struct hostent *dst = Malloc(sizeof(struct hostent));

	assert(src);
	
	if (src->h_name)
		dst->h_name = strdup(src->h_name);
	dst->h_addrtype = src->h_addrtype;
	dst->h_length = src->h_length;

	/* copy addresses */
	addr_count = count_ptrs(src->h_addr_list);
	dst->h_addr_list = Malloc((addr_count + 1) * sizeof(char *));
	for (i = 0; i < addr_count; i++) {
		dst->h_addr_list[i] = Malloc(src->h_length);
		memcpy(dst->h_addr_list[i], src->h_addr_list[i], src->h_length);
	}
	dst->h_addr_list[addr_count] = NULL;

	/* copy aliases */
	alias_count = count_ptrs(src->h_aliases);
	dst->h_aliases = Malloc((alias_count + 1) * sizeof(char *));
	for (i = 0; i < alias_count; i++)
		dst->h_aliases[i] = strdup(src->h_aliases[i]);
	dst->h_aliases[alias_count] = NULL;
	
	return dst;
}

void
free_hostent(struct hostent *host)
{
	int i = 0;

	assert(host);
	Free(host->h_name);
	/* free addresses */
	for (i = 0; NULL != host->h_addr_list[i]; i++)
		Free(host->h_addr_list[i]);
	Free(host->h_addr_list);
	/* free aliases */
	for (i = 0; NULL != host->h_aliases[i]; i++)
		Free(host->h_aliases[i]);
	Free(host->h_aliases);
	Free(host);
}

int 
count_ptrs(char **ptr)
{
	int i = 0;
	while (ptr[i++]);
	return i - 1 ; /* terminator (NULL) not counted */
}
	
static void
#if ARES_VERSION_MAJOR > 0 && ARES_VERSION_MINOR > 4
default_cb(void *arg, int status, int timeouts, struct hostent *host)
#else
default_cb(void *arg, int status, struct hostent *host)
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
		free_hostent(node->value);
	} else {
		node = Malloc(sizeof(cache_data_t));
	}
	node->key = key;
	node->value = hostent_deepcopy(value);
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
			free_hostent(node->value);
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
	dns_request_t request;

	request.type = HOSTBYNAME;
	request.query = (void *)name;
	request.callback = &default_cb;
	request.cba = &cba;

	entry = lookup_str(name);

	if (NULL == entry) {
		channel = ctx->dns_channel;
		cba.response_q = get_queue();
		/* send the request via pipe to wake up the select loop */
		size = write(ctx->dns_wake, &request, sizeof(request));
		if (size != sizeof(request))
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

struct hostent *
Gethostbyaddr_str(const char *addr, mseconds_t timeout)
{
        struct in_addr inaddr;
        int ret;

	assert(addr);
	ret = inet_pton(AF_INET, addr, &inaddr);
	if (ret < 0) {
		gerror("inet_pton");
		return NULL;
	} else if (0 == ret) {
		logstr(GLOG_ERROR, "invalid IP address: %s", addr);
		return NULL;
	} else {
		return Gethostbyaddr((char *)&inaddr, 0);
	}
	/* NOTREACHED */
	assert(0);
	return NULL;
}

struct hostent *
Gethostbyaddr(const char *addr, mseconds_t timeout)
{
	dns_cba_t cba;
	ares_channel *channel;
	size_t size;
	dns_reply_t reply;
	struct hostent *entry;
	dns_request_t request;
	char ipstr[INET_ADDRSTRLEN];
	const char *ptr;

	request.type = HOSTBYADDR;
	request.query = (void *)addr;
	request.callback = &default_cb;
	request.cba = &cba;

	ptr = inet_ntop(AF_INET, addr, ipstr, INET_ADDRSTRLEN);
	if (NULL == ptr) {
		gerror("inet_ntop");
		return NULL;
	}
	entry = lookup_str(ipstr);

	if (NULL == entry) {
		channel = ctx->dns_channel;
		cba.response_q = get_queue();
		/* send the request via pipe to wake up the select loop */
		size = write(ctx->dns_wake, &request, sizeof(request));
		if (size != sizeof(request))
			daemon_fatal("write");
		/* wait for the reply via message queue */
		size = get_msg_timed(cba.response_q, &reply, sizeof(dns_reply_t), timeout);
		release_queue(cba.response_q);

		if (size > 0) {
			if (reply.host)
				cache_str(strdup(ipstr), reply.host);
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
	struct timeval tv, *tvp;
        fd_set readers, writers;
	int nfds = 0;
	int count;
	ares_channel *channel;
	int pipefd[2];
	int ret;
	int dns_wake;
	ssize_t size;
	dns_request_t request;

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
		nfds = ares_fds(*channel, &readers, &writers);

		/* if ares has pending requests it knows the next timeout */
		tvp = &tv;
		if (nfds > 0)
			ares_timeout(*channel, NULL, tvp);
		else
			tvp = NULL;

		FD_SET(dns_wake, &readers);
		nfds = MAX(nfds, dns_wake + 1);
		
		count = select(nfds, &readers, &writers, NULL, tvp);
		if (count < 0) {
			daemon_fatal("select");
		} else {
			/* job to do or timeout */
			if (FD_ISSET(dns_wake, &readers)) {
				logstr(GLOG_INSANE, "helper_dns: received a wake up call");
				/* consume the wake up call */
				size = read(dns_wake, &request, sizeof(request));
				if (size < sizeof(request))
					daemon_fatal("read");
				switch (request.type) {
				case HOSTBYNAME:
					ares_gethostbyname(*channel, (char *)request.query, PF_INET, request.callback, request.cba);
					break;
				case HOSTBYADDR:
					ares_gethostbyaddr(*channel, (char *)request.query, 4, PF_INET, request.callback, request.cba);
					break;
				default:
					logstr(GLOG_ERROR, "helper_dns: unknown request");
					printf("Unknown request\n");
				}
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
