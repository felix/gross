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

#include <netdb.h>

typedef struct dns_cba_s
{
	int response_q;
} dns_cba_t;

typedef struct dns_reply_s
{
	struct hostent *host;
} dns_reply_t;

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

        if (status == ARES_SUCCESS) {
		reply->host = hostent_deepcopy(host);
	} else {
		reply->host = NULL;
	}
	put_msg(cba->response_q, reply, sizeof(dns_reply_t));
}

struct hostent *
Gethostbyname(const char *name, mseconds_t timeout)
{

	dns_cba_t cba;
	ares_channel *channel;
	size_t size;
	dns_reply_t reply;
	int rq;

	channel = ctx->dns_channel;
	cba.response_q = get_queue();

	ares_gethostbyname(*channel, name, PF_INET, &Gethostbyname_cb, &cba);
	size = get_msg_timed(cba.response_q, &reply, sizeof(dns_reply_t), timeout);
	release_queue(cba.response_q);

	if (size > 0)
		return reply.host;
	else
		return NULL;
}
	

void *
helper_dns(void *arg)
{
	struct timeval tv;
        fd_set readers, writers;
	int nfds;
	int count;
	ares_channel *channel;

	channel = Malloc(sizeof(*channel));
	if (ares_init(channel) != ARES_SUCCESS)
		daemon_fatal("ares_init");

	ctx->dns_channel = channel;

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
	logstr(GLOG_INFO, "starting dns helper thread");
	create_thread(&ctx->process_parts.helper_dns, DETACH, &helper_dns, NULL);
}
