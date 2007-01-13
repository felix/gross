/*
 * Copyright (c) 2007 Eino Tuominen <eino@utu.fi>
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

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

typedef int mseconds_t;

typedef struct thread_pool_s {
	int work_queue_id;
	const char *name;	/* name of the pool for logging purposes */
} thread_pool_t;

typedef struct {
	int		count;
	pthread_mutex_t	mx;
} reference_count_t;

typedef struct edict_s {
        void *job;
        int resultmq;
	reference_count_t reference;
        mseconds_t timelimit;
} edict_t;

typedef struct pool_ctx_s {
	pthread_mutex_t *mx;
	int (*routine)(edict_t *);
	thread_pool_t *info; 	/* public info */
	int count_thread;	/* number of threads in the pool */
	int count_idle;		/* idling threads */
} pool_ctx_t;

/* message queue wrap for edicts */
typedef struct edict_message_s {
	long       mtype;
	edict_t    *edict;
} edict_message_t;

int submit_job(thread_pool_t *pool, edict_t *edict);
int submit_job_wait(thread_pool_t *pool, edict_t *edict);
thread_pool_t *create_thread_pool(const char *name, int (*routine)(edict_t *));
edict_t *edict_get();
edict_t *edict_get();
void send_result(edict_t *edict, void *result);
void edict_unlink(edict_t *edict);

#endif /* THREAD_POOL_H */
