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
 * This file implements a message queue environment. First, you must initialize
 * queues by calling queue_init(). Then, use get_queue() to get a message queue id,
 * and then put_msg() to add messages to the created queue and get_msg() to
 * receive messages from the queue. All functions are re-entrant and thread safe.
 */

#include "common.h"
#include "msgqueue.h"
#include "srvutils.h"
#include "utils.h"

#define GLOBAL_QUEUE_LOCK { assert(pthread_mutex_lock(&global_queue_lk) == 0); }
#define GLOBAL_QUEUE_UNLOCK { pthread_mutex_unlock(&global_queue_lk); }

/* prototypes of internals */
msgqueue_t *queuebyid(int msqid);
void *delay(void *arg);
int put_msg_raw(msgqueue_t *mq, msg_t *msg);
msg_t *get_msg_raw(msgqueue_t *mq, mseconds_t timeout);
int set_delay_status(int msqid, int state);
void queue_realloc(void);
msgqueue_t *try_available(void);
struct timespec *peek_msg_timestamp(msgqueue_t *mq);

/* array of queues */
msgqueue_t **queues;
msgqueue_t *metaqueue;
int queuespace = 1;
int numqueues = 0;
bool initialized = false;

pthread_mutex_t global_queue_lk = PTHREAD_MUTEX_INITIALIZER;

/*
 *  queue_realloc	- doubles the space reservation for message queues
 *  			  not thread safe, the caller MUST hold GLOBAL_QUEUE_LOCK
 */
void
queue_realloc(void)
{
	size_t queuesize;

	/* queues must be initialized first */
	if (queuespace == 0) queuespace = 1;

	logstr(GLOG_DEBUG, "doubling the space for message queues from %d to %d", queuespace, queuespace * 2);

	queuesize = queuespace * sizeof(msgqueue_t *);

	/* double the size of the array */
	queues = realloc(queues, queuesize * 2);
	queuespace *= 2;
}


/*
 * queuebyid	- returns pointer to the queue referred by queue id
 */
msgqueue_t *
queuebyid(int msqid)
{
	msgqueue_t *mq;

	/*
 	 * we must lock the mutex because queues array might be replaced 
	 */
	GLOBAL_QUEUE_LOCK;

	mq = queues[msqid];

	GLOBAL_QUEUE_UNLOCK;

	return mq;
}

/* 
 * get_delay_queue	- Builds up a virtual message queue that
 * imposes a constant delay to message deliveries. Delay queue 
 * consists of two message queues and a handler thread 
 */
int
get_delay_queue(const struct timespec *ts)
{
	int putqid, getqid;
	queue_info_t *queue_info;
	int *impose_delay;

	if (! ts) {
		errno = EINVAL;
		return -1;
	}

	queue_info = Malloc(sizeof(queue_info_t));

	putqid = get_queue(); /* for put_msg() */
	getqid = get_queue(); /* for get_msg() */

	impose_delay = Malloc(sizeof(int));

	*impose_delay = 1;

	queue_info->inq = queuebyid(putqid);
	assert(queue_info->inq != NULL);
	queue_info->inq->delay_ts = ts;
	queue_info->inq->impose_delay = impose_delay;
	queue_info->outq = queuebyid(getqid);
	assert(queue_info->outq != NULL);
	queue_info->outq->delay_ts = ts;
	queue_info->outq->impose_delay = impose_delay;

	queue_info->inq->delaypair = queue_info->outq;
	queue_info->outq->delaypair = queue_info->inq;

	create_thread(NULL, DETACH, &delay, (void *)queue_info);

	return putqid;
}

void *
delay(void *arg) {
	queue_info_t *queue_info;
	msg_t *msg;
	struct timespec sleeptime, reftime, sleepleft, now, *msgtimestamp;
	int ret;
	
	logstr(GLOG_DEBUG, "delay queue manager thread starting");

	queue_info = (queue_info_t *)arg;

	for ( ; ; ) {
		logstr(GLOG_INSANE, "waiting for messages");

		msgtimestamp = peek_msg_timestamp(queue_info->inq);

		if (msgtimestamp && *queue_info->inq->impose_delay &&
				queue_info->inq->delay_ts &&
				( queue_info->inq->delay_ts->tv_sec || 
				queue_info->inq->delay_ts->tv_nsec)) {
			clock_gettime(CLOCK_TYPE, &now);
			ts_sum(&reftime, msgtimestamp, queue_info->inq->delay_ts);
			ret = ts_diff(&sleeptime, &reftime, &now);

			if (ret == 0) {
				/* we enter here only if reftime is in the future */
				do {
					logstr(GLOG_INSANE, "reftime in future, sleeping for %d.%d seconds",
						sleeptime.tv_sec, sleeptime.tv_nsec);
					ret = nanosleep(&sleeptime, &sleepleft);
					if (ret) {
						/* sleep was interrupted */
						sleeptime.tv_sec = sleepleft.tv_sec;
						sleeptime.tv_nsec = sleepleft.tv_nsec;
					}
				} while (ret);
			}
		}
		msg = get_msg_raw(queue_info->inq, 0);
		assert(msg->next == NULL);
		logstr(GLOG_INSANE, "passing message from inq to outq");
		put_msg_raw(queue_info->outq, msg);
	}
}
	
msgqueue_t *
create_queue(void)
{
	msgqueue_t *mq;

	mq = Malloc(sizeof(msgqueue_t));
	memset(mq, 0, sizeof(msgqueue_t));
	pthread_cond_init(&mq->cv, NULL);
	pthread_mutex_init(&mq->mx, NULL);

	return mq;
}


/*
 * get_queue    - returns a new quqeue
 * First it tries to get a free queue from the queue of the free queues (metaqueue)
 * If there are not any, we create a new one. If there are no space for new queues,
 * we call queue_realloc to double the space
 */
int
get_queue(void)
{
        int i;
        msgqueue_t *mq;

        GLOBAL_QUEUE_LOCK;
        if (initialized == false) {
                /* this is the first call */
                queues = calloc(queuespace, sizeof(msgqueue_t *));

                metaqueue = create_queue();
		metaqueue->active = true;

                initialized = true;
        }

        /* first try to get an available queue from the metaqueue */

        mq = try_available();
        if (mq) {
                /* found one, so let's use it */
                i = mq->id;
        } else {
                /* must create a new queue */

                i = numqueues;
                ++numqueues;

                mq = create_queue();
                mq->id = i;
		mq->active = true;

                if (numqueues > queuespace) {
                        /* there is no space left in the array */
                        queue_realloc();
                }

                queues[i] = mq;
        }

        GLOBAL_QUEUE_UNLOCK;

        return i;
}

int
disable_delay(int msqid)
{
	return set_delay_status(msqid, 0);
}

int
enable_delay(int msqid)
{
	return set_delay_status(msqid, 1);
}

int
set_delay_status(int msqid, int state)
{
	msgqueue_t *mq;
	int ret;
	
        mq = queuebyid(msqid);
	if (! mq) {
		errno = EINVAL;
		return -1;
	}

	if (! mq->delaypair) {
		errno = EINVAL;
		return -1;
	}

        ret = pthread_mutex_lock(&mq->mx);
        assert(ret == 0);
	*mq->impose_delay = state;
	ret = pthread_mutex_unlock(&mq->mx);
        assert(ret == 0);

	return 0;
}

/*
 * queue_thaw	- release locks from the queue
 */
int
queue_thaw(int msqid)
{
	msgqueue_t *mq;
	int ret;

	mq = queuebyid(msqid);
	if (! mq) {
		errno = EINVAL;
		return -1;
	}

	logstr(GLOG_ERROR, "thaw queue %d", msqid);

	ret = pthread_mutex_unlock(&mq->mx);
	assert(ret == 0);
	if (mq->delaypair) {
		ret = pthread_mutex_unlock(&mq->delaypair->mx);
		assert(ret == 0);
	}

	return 0;
}

/*
 * queue_freeze	- hold processing of the queue
 */
int
queue_freeze(int msqid)
{
	msgqueue_t *mq;
	int ret;

	mq = queuebyid(msqid);
	if (! mq) {
		errno = EINVAL;
		return -1;
	}

        logstr(GLOG_ERROR, "freeze queue %d", msqid);

	ret = pthread_mutex_lock(&mq->mx);
	assert(ret == 0);
	if (mq->delaypair) {
		ret = pthread_mutex_lock(&mq->delaypair->mx);
		assert(ret == 0);
	}

	return 0;
}

	

int
set_delay(int msqid, const struct timespec *ts)
{
	msgqueue_t *mq;
	int ret;

        mq = queuebyid(msqid);
        if (! mq) {
                errno = EINVAL;
                return -1;
        }

        if (! mq->delaypair) {
                errno = EINVAL;
                return -1;
        }

	if (! mq->delay_ts) {
		errno = EINVAL;
		return -1;
	}

        ret = pthread_mutex_lock(&mq->mx);
        assert(ret == 0);
	memcpy((void *)mq->delay_ts, ts, sizeof(struct timespec));
        ret = pthread_mutex_unlock(&mq->mx);
	assert(ret == 0);

        return 0;
}

int
put_msg_raw(msgqueue_t *mq, msg_t *msg)
{
	int ret;
	msg_t *tail;

	if (mq->active == false) {
		logstr(GLOG_ERROR, "message queue is marked inactive");
		return -1;
	}

	ret = pthread_mutex_lock(&mq->mx);
	assert(ret == 0);

	/* check if there are messages already in the queue */
	if (mq->tail) {
		/* the queue is not empty */
		assert(mq->head);
		tail = mq->tail;
		tail->next = msg;
	} else {
		/* the queue is emtpy */
		assert(mq->head == NULL);
		mq->head = msg;
	}
	mq->tail = msg;
	assert(mq->tail->next == NULL);
	mq->msgcount++;

	pthread_cond_signal(&mq->cv);
	pthread_mutex_unlock(&mq->mx);
	
	return 0;
}

int
put_msg(int msqid, void *omsgp, size_t msgsz, int msgflg)
{
	msgqueue_t *mq;
	msg_t *new;
	void *msgp;
	int ret;
	size_t truesize;

	mq = queuebyid(msqid);
	assert(mq);

	new = Malloc(sizeof(msg_t));
	/* zero out the message structure */
	memset(new, 0, sizeof(msg_t));

	clock_gettime(CLOCK_TYPE, &new->timestamp);

        /*
         * msgsize is the length of the message, we must add
         * the length of the type field (long) also
         */
        truesize = msgsz + sizeof(long);

	msgp = Malloc(truesize);
	memcpy(msgp, omsgp, truesize);

	new->msgp = msgp;
	new->msgsz = truesize;

	ret = put_msg_raw(mq, new);

	return ret;
}

int
instant_msg(int msqid, void *omsgp, size_t msgsz, int msgflg)
{
	msgqueue_t *mq;
	msg_t *new;
	void *msgp;
	int ret;
	size_t truesize;

	mq = queuebyid(msqid);
	assert(mq);

	if (mq->delaypair != NULL)
		mq = mq->delaypair;
	assert(mq);

	new = Malloc(sizeof(msg_t));
	/* zero out the message structure */
	memset(new, 0, sizeof(msg_t));

	clock_gettime(CLOCK_TYPE, &new->timestamp);

        /*
         * msgsize is the length of the message, we must add
         * the length of the type field (long) also
         */
        truesize = msgsz + sizeof(long);

	msgp = Malloc(truesize);
	memcpy(msgp, omsgp, truesize);

	new->msgp = msgp;
	new->msgsz = truesize;

	ret = put_msg_raw(mq, new);

	return ret;
}

/* 
 * release_queue	- add the queue to the metaqueue. Make sure it's empty
 */
int 
release_queue(int msqid)
{
	msg_t *msg;
	int ret;
	msgqueue_t *mq;

	mq = queuebyid(msqid);

	if (mq->delaypair) {
		logstr(GLOG_ERROR, "release_queue: attempt to free a delay queue");
		return -1;
	}

	if (mq->head) {
		logstr(GLOG_INSANE, "release_queue: queue not empty");
		return -1;
	}

        ret = pthread_mutex_lock(&mq->mx);
        assert(ret == 0);
	mq->active = false;
        ret = pthread_mutex_unlock(&mq->mx);
        assert(ret == 0);

	msg = Malloc(sizeof(msg_t));
	/* zero out the message structure */
	memset(msg, 0, sizeof(msg_t));

	msg->msgp = mq;
	ret = put_msg_raw(metaqueue, msg);
	/* with metaqueue there can no be other return values */
	assert(ret == 0);

	return 0;
}

/*
 * try_available	- tries to fetch a message from the metaqueue
 */
msgqueue_t *
try_available(void)
{
	msg_t *msg;
	msgqueue_t *mq = NULL;

	msg = get_msg_raw(metaqueue, -1);

	if (msg) {
		mq = (msgqueue_t *)msg->msgp;
		mq->active = true;
		Free(msg);
	}

	return mq;
}

/*
 * peek_msg_timestamp - returns pointer to timestamp of the first message in the queue
 */
struct timespec *
peek_msg_timestamp(msgqueue_t *mq)
{
	struct timespec *timestamp;
	int ret;

        if (mq->active == false) {
                logstr(GLOG_ERROR, "get_msg_raw: message queue is marked inactive");
                return NULL;
        }

        ret = pthread_mutex_lock(&mq->mx);
        assert(ret == 0);

        /* the queue is now empty, wait for messages */
        while (mq->head == NULL)
                pthread_cond_wait(&mq->cv, &mq->mx);

	assert(mq->head);
	assert(mq->tail);

        timestamp = &mq->head->timestamp;

        pthread_mutex_unlock(&mq->mx);

        return timestamp;
}


/* 
 * get_msg_raw	- retuns the first message from the message queue
 */
msg_t *
get_msg_raw(msgqueue_t *mq, mseconds_t timeout)
{
	msg_t *msg;
	int ret;
	struct timespec to;

	if (mq->active == false) {
		logstr(GLOG_ERROR, "get_msg_raw: message queue is marked inactive");
		return NULL;
	}

	ret = pthread_mutex_lock(&mq->mx);
	assert(ret == 0);
	msg = NULL;

	mstotimespec(timeout, &to);

	to.tv_sec += time(NULL);

	if (timeout >= 0) {
		/* the queue is now empty, wait for messages */
		while (mq->head == NULL)
			if (timeout == 0) {
				ret = pthread_cond_wait(&mq->cv, &mq->mx);
			} else {
				ret = pthread_cond_timedwait(&mq->cv, &mq->mx, &to);
				if (ret == ETIMEDOUT)
					break;
			}
	} else {
		/* if timeout < 0, we do not wait for messages */
		if (mq->head == 0) {
			/* there is no message */
			ret = -1;
		}
	}

	if (ret == 0) {
		/* a message has been found on the queue */
		assert(mq->head);
		assert(mq->tail);
		msg = mq->head;
		mq->head = msg->next;
		msg->next = NULL;

		mq->msgcount--;
		if (mq->head == NULL) {
			/* queue now empty */
			assert(mq->tail == msg);
			assert(mq->msgcount == 0);
			mq->tail = NULL;
		}
	}

	pthread_mutex_unlock(&mq->mx);
	return msg;
}

/*
 * get_msg	- Returns the first message from the message queue
 * mimics the semantics of msgrcv().
 */
size_t
get_msg(int msqid, void *msgp, size_t maxsize, int msgflag)
{
	return get_msg_timed(msqid, msgp, maxsize, msgflag, 0);
}

size_t
get_msg_timed(int msqid, void *msgp, size_t maxsize, int msgflag, mseconds_t timeout)
{
	msgqueue_t *mq;
	msg_t *msg;
	size_t msglen;
	size_t msgsize;

	mq = queuebyid(msqid);
	assert(mq);

	if (mq->delaypair)
		msg = get_msg_raw(mq->delaypair, timeout);
	else
		msg = get_msg_raw(mq, timeout);

	/* msg is NULL if timeout occurred */
	if (msg == NULL) {
		msglen = 0;
	} else {
		/* we need just the msgsize without the type field */
		msgsize = msg->msgsz - sizeof(long);

		msglen = (maxsize < msgsize) ? maxsize : msgsize;
		memcpy(msgp, msg->msgp, msglen + sizeof(long));
		Free(msg->msgp);
		Free(msg);
	}
	
	return msglen;
}

size_t
in_queue_len(int msgid)
{
	msgqueue_t *mq;

	mq = queuebyid(msgid);

	assert(mq);

	return mq->msgcount;
}

size_t
out_queue_len(int msgid)
{
	msgqueue_t *mq;

	mq = queuebyid(msgid);
	assert(mq);

	if (mq->delaypair)
	return mq->delaypair->msgcount;

	return in_queue_len(msgid);
}

int
walk_queue(int msgid, int (* callback)(void *))
{
	msgqueue_t *mq;
        msg_t *msg;
	int ret;

	mq = queuebyid(msgid);
	assert(mq);

	if (mq->active == false) {
                logstr(GLOG_ERROR, "get_msg_raw: message queue is marked inactive");
                return -1;
        }

	if (mq->head) {
		msg = mq->head;
		while (msg) {
			logstr(GLOG_DEBUG, "walk_queue: calling callback function");
			ret = callback(msg->msgp);
			if (ret < 0) {
				logstr(GLOG_ERROR, "walk_queue: callback returned FAILURE");
				return -1;
			}
			msg = msg->next;
		}
	}

	if (mq->delaypair) {
		if (mq->delaypair->head) {
			msg = mq->head;
			while (msg) {
				logstr(GLOG_DEBUG, "walk_queue: calling callback function");
				ret = callback(msg->msgp);
				if (ret < 0) {
					logstr(GLOG_ERROR, "walk_queue: callback returned FAILURE");
					return -1;
				}
				msg = msg->next;
			}
		}
	}
	return 0;
}
