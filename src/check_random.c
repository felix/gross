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

/*
 * This is a check for debug purposes. It will randomly return 
 * different results.
 */

#include "common.h"
#include "srvutils.h"
#include "utils.h"
#include "worker.h"

/* the cleanup routine */
int
cleanup_random(void *state)
{
	int *foo;

	foo = state;	
	Free(foo);
	return 0;
}

int 
randomc(thread_pool_t *info, thread_ctx_t *thread_ctx, edict_t *edict)
{
	chkresult_t *result;
	int r;
        grey_tuple_t *request;
        const char *client_address;

	/* Check if the random number generator has been initialized */
	if (NULL == thread_ctx->state) {
		thread_ctx->state = Malloc(sizeof(int));
		srand(time(NULL));
		/* register the cleanup procedure */
		thread_ctx->cleanup = &cleanup_random;
	}


        request = (grey_tuple_t *)edict->job;
        client_address = request->client_address;
        assert(client_address);

	result = (chkresult_t *)Malloc(sizeof(chkresult_t));
	memset(result, 0, sizeof(*result));
	result->judgment = J_UNDEFINED;

	r = rand();
	if ((r % 7) == 0) {
		logstr(GLOG_DEBUG, "random pass: %s", request->client_address);
		result->judgment = J_PASS;
	} else if ((r % 5) == 0) {
		logstr(GLOG_DEBUG, "random block: %s", request->client_address);
		result->judgment = J_BLOCK;
		result->reason = strdup("This is just a random block.");
	} else if ((r % 3) == 0) {
		logstr(GLOG_DEBUG, "random suspect: %s", request->client_address);
		result->judgment = J_SUSPICIOUS;
		result->weight = 1; 	/* FIXME: needs to be configurable */
	}

	send_result(edict, result);
	logstr(GLOG_DEBUG, "blocker returning");
	request_unlink(request);
	
	return 0;
}

void
random_init(pool_limits_t *limits)
{
	thread_pool_t *pool;

	/* initialize the thread pool */
        logstr(GLOG_INFO, "initializing random check thread pool");
	pool = create_thread_pool("random", &randomc, limits, NULL);
        if (pool == NULL)
                daemon_perror("create_thread_pool");

	register_check(pool, true);
}
