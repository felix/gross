/*
 * Copyright (c) 2006,2007,2008
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

/*
 * query Sophos blocker list. Idea for this came from Jesse Thompson,
 * <jesse.thompson@doit.wisc.edu> and code was written after viewing the
 * message Bernd Cappel <cappel@uni-duesseldorf.de> wrote on the
 * perlmx@ca.sophos.com mail list.
 */

#include "common.h"
#include "srvutils.h"
#include "utils.h"
#include "worker.h"

int
blocker(thread_pool_t *info, thread_ctx_t *thread_ctx, edict_t *edict)
{
	chkresult_t *result;
	int blocker;
	int ret;

	grey_tuple_t *request;
	const char *client_address;
	char buffer[MAXLINELEN] = {'\0'};
	struct timespec start, timeleft;

	request = (grey_tuple_t *) edict->job;
	client_address = request->client_address;
	assert(client_address);

	result = (chkresult_t *) Malloc(sizeof(chkresult_t));
	memset(result, 0, sizeof(*result));
	result->judgment = J_UNDEFINED;

	clock_gettime(CLOCK_TYPE, &start);
	mstotimespec(edict->timelimit, &timeleft);

	blocker = socket(AF_INET, SOCK_STREAM, 0);
	if (blocker < 0) {
		logstr(GLOG_ERROR, "blocker: socket: %s", strerror(errno));
		goto FINISH;
	}
	ret = connect(blocker, (struct sockaddr *) & ctx->config.blocker.server,
		      sizeof(struct sockaddr_in));
	if (ret < 0) {
		logstr(GLOG_ERROR, "blocker: connect: %s", strerror(errno));
		close(blocker);
		goto FINISH;
	}
	/* build a query string */
	snprintf(buffer, MAXLINELEN, "client_address=%s\n\n", request->client_address);
	buffer[MAXLINELEN - 1] = '\0';

	/* send the query */
	ret = writen(blocker, buffer, strlen(buffer));
	if (ret < 0) {
		logstr(GLOG_ERROR, "blocker: writen: %s", strerror(errno));
		close(blocker);
		goto FINISH;
	}
	ret = readline(blocker, &buffer, MAXLINELEN);
	if (ret < 0) {
		logstr(GLOG_ERROR, "blocker: readline: %s", strerror(errno));
		close(blocker);
		goto FINISH;
	}
	close(blocker);

	if (strncmp(buffer, "action=565 ", 11) == 0) {
		logstr(GLOG_DEBUG, "found match from blocker: %s", request->client_address);
		result->judgment = J_SUSPICIOUS;
		result->weight = 1;	/* FIXME: needs to be configurable */
	}
FINISH:
	send_result(edict, result);
	logstr(GLOG_DEBUG, "blocker returning");
	request_unlink(request);

	return 0;
}

void
blocker_init(pool_limits_t *limits)
{
	thread_pool_t *pool;

	/* initialize the thread pool */
	logstr(GLOG_INFO, "initializing Sophos blocker thread pool");
	pool = create_thread_pool("blocker", &blocker, limits, NULL);
	if (pool == NULL)
		daemon_perror("create_thread_pool");

	register_check(pool, false);
}
