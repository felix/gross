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
	pool_ctx_t *pool_ctx = NULL;
	edict_message_t message;
	edict_t *edict = NULL;
	mseconds_t timelimit;
	thread_ctx_t thread_ctx = { NULL };
	bool process;
	int waited;
	struct timespec start, end;
	bool idlecheck = false;

	pool_ctx = (pool_ctx_t *)arg;
	assert(pool_ctx->mx);
	assert(pool_ctx->routine);
	assert(pool_ctx->info);

	timelimit = pool_ctx->idle_time * SI_KILO;
		
	POOL_MUTEX_LOCK;
	pool_ctx->count_thread++;
	POOL_MUTEX_UNLOCK;

	logstr(GLOG_DEBUG, "threadpool '%s' thread #%d starting",
		pool_ctx->info->name, pool_ctx->count_thread);

	for (;;) {
		if (idlecheck) {
			/* check if there are too many idling threads */
			POOL_MUTEX_LOCK;
			if (idlecheck && pool_ctx->count_idle >= pool_ctx->max_idle) {
				pool_ctx->count_thread--;
				POOL_MUTEX_UNLOCK;
				logstr(GLOG_INFO, "threadpool '%s' thread shutting down",
					pool_ctx->info->name);
				/* run a cleanup routine if defined */
				if (thread_ctx.cleanup)
					thread_ctx.cleanup(thread_ctx.state);
				pthread_exit(NULL);
			} else {
				idlecheck = false;
				pool_ctx->count_idle++;
				POOL_MUTEX_UNLOCK;
			}
		} else {
			POOL_MUTEX_LOCK;
			pool_ctx->count_idle++;
			POOL_MUTEX_UNLOCK;
		}

		/* wait for new jobs */
		clock_gettime(CLOCK_TYPE, &start);
		ret = get_msg_timed(pool_ctx->info->work_queue_id, &message, sizeof(message.edict), 0, timelimit);
		clock_gettime(CLOCK_TYPE, &end);

		process = true;

		POOL_MUTEX_LOCK;
		pool_ctx->count_idle--;
		POOL_MUTEX_UNLOCK;

		if (ret > 0) {
			/* we've got a message */
			edict = message.edict;
			assert(edict->job);

			logstr(GLOG_DEBUG, "threadpool '%s' processing", pool_ctx->info->name);
	
			POOL_MUTEX_LOCK;
			if (pool_ctx->count_idle < 1) {
				/* We were the last idling thread, start another */
				if (pool_ctx->count_thread <= pool_ctx->max_thread || 0 == pool_ctx->max_thread) {
					logstr(GLOG_INFO, "threadpool '%s' starting another thread",
						pool_ctx->info->name);
					Pthread_create(NULL, &thread_pool, pool_ctx);
				} else {
					logstr(GLOG_ERROR, "threadpool '%s': maximum thread count (%d) reached",
						pool_ctx->info->name, pool_ctx->max_thread);
					process = false;
				}
			}
			POOL_MUTEX_UNLOCK;

			/* run the routine with args */
			if (process) 
				pool_ctx->routine(pool_ctx->info, &thread_ctx, edict);

			/* we are done */
			edict_unlink(edict);

			/*
			 * how long we had to wait for the job request?
			 * if the wait time * number of idling threads > max_idle_time,
			 * we may shut down a thread 
			 */
			waited = ms_diff(&end, &start);
			/* no mutex lock necessary */
			if (pool_ctx->count_idle * waited > timelimit)
				idlecheck = true;
		} else {
			/* timeout occurred */
			logstr(GLOG_INSANE, "threadpool '%s' idling", pool_ctx->info->name);
			idlecheck = true;
		}
	}
}

thread_pool_t *
create_thread_pool(const char *name, int (*routine)(thread_pool_t *, thread_ctx_t *, edict_t *),
	pool_limits_t *limits, void *arg)
{
	thread_pool_t *pool;
	pthread_mutex_t *pool_mx;
	pool_ctx_t *pool_ctx;
	int ret;

	/* init */
	pool = (thread_pool_t *)Malloc(sizeof(thread_pool_t));
	pool->work_queue_id = get_queue();
	if (pool->work_queue_id < 0) {
		Free(pool);
		return NULL;
	}

	pool->arg = arg;
	pool->name = name;

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
	pool_ctx->max_thread = limits ? limits->max_thread : 0; 
	pool_ctx->max_idle = limits ? limits->max_idle : 1;
	pool_ctx->idle_time = limits ? limits->idle_time : 60;

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

	/* increment reference counter */
	pthread_mutex_lock(&job->reference.mx);
	job->reference.count++;
	pthread_mutex_unlock(&job->reference.mx);

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
	pthread_mutex_init(&edict->reference.mx, NULL);
	edict->reference.count = 1;

	return edict;
}

void
edict_unlink(edict_t *edict)
{
	int ret;
        poolresult_message_t message;

	ret = pthread_mutex_lock(&edict->reference.mx);
	assert(0 == ret);
	assert(edict->reference.count > 0);

	if (--edict->reference.count == 0) {
		/* last reference */
		if (edict->resultmq > 0)
			while (release_queue(edict->resultmq) < 0) {
				/* queue wasn't emtpy */
				logstr(GLOG_INSANE, "queue not empty, flushing");
				ret = get_msg_timed(edict->resultmq, &message,
					sizeof(message.result), 0, -1);
				if (ret > 0) {
					assert(message.result);
					free((chkresult_t *)message.result);
					message.result = NULL;
				}
			}
		pthread_mutex_unlock(&edict->reference.mx);
		Free(edict);
	} else {
		pthread_mutex_unlock(&edict->reference.mx);
	}
}

void
send_result(edict_t *edict, void *result)
{
	int ret;
	poolresult_message_t message;

	message.result = result;
	
	ret = put_msg(edict->resultmq, &message, sizeof(message), 0);
	if (ret < 0)
		perror("send_result");
}
