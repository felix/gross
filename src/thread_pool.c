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
	thread_ctx_t thread_ctx = { NULL };
	watchdog_t *dogp;
	bool process;
	struct timespec now;
	int waited;
	int lastseenms;

	pool_ctx = (pool_ctx_t *)arg;
	assert(pool_ctx->mx);
	assert(pool_ctx->routine);
	assert(pool_ctx->info);

	POOL_MUTEX_LOCK;
	pool_ctx->count_thread++;

	if (pool_ctx->watchdog_time) {
		/* add the watchdog info for this thread */
		dogp = pool_ctx->wdlist;
		pool_ctx->wdlist = &thread_ctx.watchdog;
		pool_ctx->wdlist->next = dogp;
		clock_gettime(CLOCK_TYPE, &thread_ctx.watchdog.last_seen);
		thread_ctx.watchdog.tid = pthread_self();
	}

	/* idle check reference time */
	clock_gettime(CLOCK_TYPE, &pool_ctx->last_idle_check);
	POOL_MUTEX_UNLOCK;

	logstr(GLOG_DEBUG, "threadpool '%s' thread #%d starting%s", pool_ctx->info->name,
			pool_ctx->count_thread, pool_ctx->watchdog_time ? " watchdog enabled" : "");

	for (;;) {
		/* check if there are too many idling threads */
		POOL_MUTEX_LOCK;

		/* kick the watchdog */
		if (pool_ctx->watchdog_time)
			clock_gettime(CLOCK_TYPE, &thread_ctx.watchdog.last_seen);

		clock_gettime(CLOCK_TYPE, &now);
		waited = ms_diff(&now, &pool_ctx->last_idle_check);

		if (waited > IDLETIME) {
			/* update the reference time */
			clock_gettime(CLOCK_TYPE, &pool_ctx->last_idle_check);

			if (pool_ctx->watchdog_time) {
				/* check the watchdog status */
				dogp = pool_ctx->wdlist;
				while (dogp) {
					lastseenms = ms_diff(&now, &dogp->last_seen);
					if (lastseenms > pool_ctx->watchdog_time) {
						/* a stuck thread */
						logstr(GLOG_WARNING, "thread #%x of pool '%s' stuck, last seen %d ms ago.",
							(uint32_t)dogp->tid, pool_ctx->info->name, lastseenms);
						pthread_kill(dogp->tid, SIGALRM);
					}
					dogp = dogp->next;
				}
			}

			if  (pool_ctx->count_thread > 8 && pool_ctx->ewma_idle > pool_ctx->count_thread / 2) {
				/* prepare for shutdown */
				pool_ctx->count_thread--;
				/*
				 * update the moving average by decrementing it
			         * brutal, but efficient for the purpose
				 */
				pool_ctx->ewma_idle--;
				if (pool_ctx->watchdog_time) {
					/*
					 * remove thread from the watchdoglist
					 * do not Free(), the block is reserved from the stack 
					 */
					dogp = pool_ctx->wdlist;
					if (dogp->tid == pthread_self()) {
						/* first node */
						pool_ctx->wdlist = pool_ctx->wdlist->next;
					} else { 
						while (dogp->next) {
							if (dogp->next->tid == pthread_self()) {
								dogp->next = dogp->next->next;
								break;
							}
						dogp = dogp->next;
						}
					}
				}
				POOL_MUTEX_UNLOCK;
				logstr(GLOG_INFO, "threadpool '%s' thread shutting down",
					pool_ctx->info->name);
				/* run a cleanup routine if defined */
				if (thread_ctx.cleanup)
					thread_ctx.cleanup(thread_ctx.state);
				pthread_exit(NULL);
			}
		}
		/* update the moving average */
		EWMA(pool_ctx->ewma_idle, pool_ctx->count_idle);
		pool_ctx->count_idle++;
		POOL_MUTEX_UNLOCK;

		/* wait for new jobs */
		ret = get_msg_timed(pool_ctx->info->work_queue_id, &message, sizeof(message.edict), 0, IDLETIME);

		POOL_MUTEX_LOCK;
		/* kick the watchdog */
		if (pool_ctx->watchdog_time)
			clock_gettime(CLOCK_TYPE, &thread_ctx.watchdog.last_seen);
		pool_ctx->count_idle--;
		POOL_MUTEX_UNLOCK;

		process = true;

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
		} else {
			/* timeout occurred */
			logstr(GLOG_INSANE, "threadpool '%s' idling", pool_ctx->info->name);
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
		daemon_fatal("pthread_mutex_init");

	pool_ctx = (pool_ctx_t *)Malloc(sizeof(pool_ctx_t));

	pool_ctx->mx = pool_mx;
	pool_ctx->routine = routine;
	pool_ctx->info = pool;
	pool_ctx->count_thread = 0;
	pool_ctx->count_idle = 0;
	pool_ctx->ewma_idle = 0;
	pool_ctx->max_thread = limits ? limits->max_thread : 0; 
	pool_ctx->watchdog_time = limits ? limits->watchdog_time : 0; 	/* watchdog timer, 0 is disabled */
	pool_ctx->wdlist = NULL;

	/* start the first thread */
	Pthread_create(NULL, &thread_pool, pool_ctx);
	return pool;
}

/*
 * submit_job	- add a job request to the work queue
 */
int
submit_job(thread_pool_t *pool, edict_t *edict)
{
	edict_message_t message;

	message.mtype = 0;
	message.edict = edict;

	/* increment reference counter */
	pthread_mutex_lock(&edict->reference.mx);
	edict->reference.count++;
	pthread_mutex_unlock(&edict->reference.mx);

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
		gerror("send_result");
}
