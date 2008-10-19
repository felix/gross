/* $Id$ */

/*
 * Copyright (c) 2008
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
#include "utils.h"
#include "worker.h"
#include "helper_dns.h"

int
reverse(thread_pool_t *info, thread_ctx_t *thread_ctx, edict_t *edict)
{
	chkresult_t *result;
	struct hostent *canonicalhost, *reversehost;

	grey_tuple_t *request;
	const char *client_address;
        char buf[INET_ADDRSTRLEN];
	const char *ptr;

	request = (grey_tuple_t *)edict->job;
	client_address = request->client_address;
	assert(client_address);

	result = (chkresult_t *)Malloc(sizeof(chkresult_t));
	memset(result, 0, sizeof(*result));
	result->judgment = J_UNDEFINED;
	result->checkname = "reverse";

	reversehost = Gethostbyaddr_str(client_address, 0);
	if (reversehost) {
                logstr(GLOG_INSANE, "client_address (%s) has a PTR record (%s)",
                        client_address, reversehost->h_name);
		canonicalhost = Gethostbyname(reversehost->h_name, 0);
		if (canonicalhost) {
			ptr = inet_ntop(AF_INET, canonicalhost->h_addr_list[0], buf, INET_ADDRSTRLEN);
			assert(ptr);
			logstr(GLOG_INSANE, "client_ip (%s) canonical (%s)",
				client_address, buf);
			if (strcmp(buf, client_address)) {
				logstr(GLOG_DEBUG, "client_address (%s) not canonical (%s)",
					client_address, buf);
				result->judgment = J_SUSPICIOUS;
				result->weight = 1; /* FIXME */
			}
		} else {
			logstr(GLOG_DEBUG, "No A for PTR for client_ip (%s)", client_address);
			result->judgment = J_SUSPICIOUS;
			result->weight = 1;
		}
	} else {
		logstr(GLOG_DEBUG, "client_ip (%s) has no PTR record", client_address);
		result->judgment = J_SUSPICIOUS;
		result->weight = 1;
	}

	send_result(edict, result);
	logstr(GLOG_DEBUG, "reverse returning");
	request_unlink(request);

	return 0;
}

void
reverse_init(pool_limits_t *limits)
{
	thread_pool_t *pool;

	/* initialize the thread pool */
	logstr(GLOG_INFO, "initializing reverse thread pool");
	pool = create_thread_pool("reverse", &reverse, limits, NULL);
	if (pool == NULL)
		daemon_fatal("create_thread_pool");

	register_check(pool, false);
}
