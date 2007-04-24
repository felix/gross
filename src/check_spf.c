/*
 * Copyright (c) 2007
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

#include "common.h"
#include "check_spf.h"
#include "srvutils.h"
#include "utils.h"
#include "worker.h"

/* the cleanup routine */
int
cleanup_spfc(void *state)
{
	SPF_server_t *spf_server;

	spf_server = (SPF_server_t *)state;
        if (spf_server)
                SPF_server_free(spf_server);
	return 0;
}

int
spfc(thread_ctx_t *thread_ctx, edict_t *edict)
{
	struct timespec ts, start, now, timeleft;
	chkresult_t *result;
	grey_tuple_t *request;
        SPF_server_t *spf_server = NULL;
        SPF_request_t *spf_request = NULL;
        SPF_response_t *spf_response = NULL;
        SPF_response_t *spf_response_2mx = NULL;

	logstr(GLOG_DEBUG, "spfc called");

	request = (grey_tuple_t *)edict->job;
	assert(request);

	result = (chkresult_t *)Malloc(sizeof(chkresult_t));
	memset(result, 0, sizeof(*result));
        result->judgment = J_UNDEFINED;

	/* initialize if we are not yet initialized */
	if (NULL == thread_ctx->state) {
		/* Initialize */
		spf_server = SPF_server_new();
		if (NULL = spf_server) {
			logstr(GLOG_ERROR, "SPF_server_new failed");
			goto FINISH;
		}
		thread_ctx->state = spf_server;
		thread_ctx->cleanup = &cleanup_spfc;
	} else {
		spf_server = (SPF_server_t *)thread_ctx->state;
	}

	/* Now we are ready to query */
	spf_request = SPF_request_new(spf_server);
	
	ret = SPF_request_set_ipv4_str(spf_request, request->client_ip);
	if (ret) {
		logstr(GLOG_ERROR, "invalid IP address %s", request->client_ip);
		goto ERROR;
	}

	/* FIXME: no helo domain from clients yet */
	ret = SPF_request_set_helo_dom(spf_request, "");
	if (ret) {
		logstr(GLOG_ERROR, "invalid HELO domain: %s.", "");
		goto ERROR;
	}

	ret = SPF_request_set_env_from(spf_request, request->sender);
	if (ret) {
		logstr(GLOG_ERROR, "invalid envelope sender address %s", request->sender);
		goto ERROR;
	}

        SPF_request_query_mailfrom(spf_request, &spf_response);

	ret = SPF_response_result(spf_response);
	if (ret != SPF_RESULT_PASS) {
		SPF_request_query_rcptto(spf_request, &spf_response_2mx, request->recipient);
		ret = SPF_response_result(spf_response_2mx);
		if (ret == SPF_RESULT_PASS) {
		}
	}

ERROR:
        if (spf_request)
                SPF_request_free(spf_request);
FINISH:

	send_result(edict, result);
	
	logstr(GLOG_DEBUG, "spfc returning");
	request_unlink(request);

	return 0;
}

void
spf_init(pool_limits_t *limits)
{
	thread_pool_t *pool;

	/* initialize the thread pool */
        logstr(GLOG_INFO, "initializing spf checker thread pool");
	pool = create_thread_pool("spf", &spfc, limits);
        if (pool == NULL)
                daemon_perror("create_thread_pool");

	/* This is a definitive check */
	register_check(pool, true);
}
