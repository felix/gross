/*
 * Copyright (c) 2007 Eino Tuominen <eino@utu.fi>
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
 * This files implements thread pools. They are self contained groups
 * threads which automatically handle adding new threads to the pools
 * and removing idling threads from the pools.
 */

#include "common.h"
#include "msgqueue.h"
#include "srvutils.h"
#include "utils.h"
#include "thread_pool.h"

/* internals */
static void *thread_pool(void *arg);

/* macros */
#define POOL_MUTEX_LOCK { pthread_mutex_lock(pool_ctx->mx); }
#define POOL_MUTEX_UNLOCK { pthread_mutex_unlock(pool_ctx->mx); }

static void *
thread_pool(void *arg)
{
	int ret;
	pool_ctx_t *pool_ctx;
	edict_message_t message;
	edict_t *edict;
	time_t timelimit;
	
	pool_ctx = (pool_ctx_t *)arg;
	assert(pool_ctx->mx);
	assert(pool_ctx->routine);
	assert(pool_ctx->info);

	logstr(GLOG_DEBUG, "threadpool '%s' starting", pool_ctx->name);

	POOL_MUTEX_LOCK;
	pool_ctx->count_thread++;
	POOL_MUTEX_UNLOCK;

	for (;;) {
		/* wait for new jobs */
		timelimit = 60; /* one minute */

		POOL_MUTEX_LOCK;
		pool_ctx->count_idle++;
		POOL_MUTEX_UNLOCK;

		ret = get_msg_timed(pool_ctx->info->work_queue_id, &message, sizeof(message), 0, timelimit); 
		if (ret > 0) {
			/* we've got a message */
			edict = message.edict;
			assert(edict->job);

			logstr(GLOG_DEBUG, "threadpool '%s' processing", pool_ctx->name);

			POOL_MUTEX_LOCK;
			
			pool_ctx->count_idle--;
			if (pool_ctx->count_idle < 1) {
				/* We are the last idling thread, start another */
				logstr(GLOG_DEBUG, "threadpool '%s' starting another thread", pool_ctx->name);
				Pthread_create(NULL, &thread_pool, pool_ctx);
			}

			POOL_MUTEX_UNLOCK;

			/* run the routine with args */
			pool_ctx->routine(edict);

			/* check if caller waits for response */
			if (0 == edict->resultmq)
				free(edict);
		} else {
			/* timeout occurred */

			POOL_MUTEX_LOCK;

			logstr(GLOG_DEBUG, "threadpool '%s' notices it's idling", pool_ctx->name);

			pool_ctx->count_idle--;
			/* there should be at least one idling thread left */
			if (pool_ctx->count_idle > 1) {
				pool_ctx->count_thread--;
				POOL_MUTEX_UNLOCK;
				logstr(GLOG_DEBUG, "threadpool '%s' thread shutting down", pool_ctx->name);
				pthread_exit(NULL);
			}
			POOL_MUTEX_UNLOCK;
		}
	}
}

thread_pool_t *
create_thread_pool(const char *name, int (*routine)(edict_t *))
{
	thread_pool_t *pool;
	pthread_mutex_t *pool_mx;
	pool_ctx_t *pool_ctx;
	int ret;

	/* init */
	pool = (thread_pool_t *)Malloc(sizeof(thread_pool_t));
	pool->work_queue_id = get_queue();
	if (pool->work_queue_id < 0) {
		free(pool);
		return NULL;
	}

	pool_mx = (pthread_mutex_t *)Malloc(sizeof(pthread_mutex_t));
	ret = pthread_mutex_init(pool_mx, NULL);
	if (ret)
		daemon_perror("pthread_mutex_init");

	pool_ctx = (pool_ctx_t *)Malloc(sizeof(pool_ctx_t));

	pool_ctx->mx = pool_mx;
	pool_ctx->routine = routine;
	pool_ctx->info = pool;
	pool_ctx->count_thread = 0;
	pool_ctx->count_idle = 0;
	pool_ctx->name = name;

	/* start controller thread */
	Pthread_create(NULL, &thread_pool, pool_ctx);
	return pool;
}

/*
 * submit_job	- add a job to the work queue
 */
int
submit_job(thread_pool_t *pool, edict_t *job)
{
	edict_message_t message;

	message.mtype = 0;
	message.edict = job;

	return put_msg(pool->work_queue_id, &message, sizeof(message.edict), 0);
}

/*
 * edict_get	- convenience function for creating an edict
 */
edict_t *
edict_get(bool forget)
{
	edict_t *edict;
		

	edict = (edict_t *)Malloc(sizeof(edict_t));
	bzero(edict, sizeof(edict_t));

	/* reserve a message queue, if results are wanted */
	if (false == forget) {
		edict->resultmq = get_queue();
	}

	return edict;
}
