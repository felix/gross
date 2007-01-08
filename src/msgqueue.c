/*
 * Copyright (c) 2006 Eino Tuominen <eino@utu.fi>
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

/* prototypes of internals */
void *delay(void *arg);
int put_msg_raw(msgqueue_t *mq, msg_t *msg);
msg_t *get_msg_raw(msgqueue_t *mq, const struct timespec *timeout);
int set_delay_status(int msqid, int state);

/* array of queues */
msgqueue_t **queues;
int numqueues = 0;

pthread_mutex_t global_queue_lk = PTHREAD_MUTEX_INITIALIZER;

int 
queue_init(size_t num)
{
	int ret;
	
	if (numqueues)
		return -1;

	ret = pthread_mutex_lock(&global_queue_lk);
	assert(ret == 0);

	numqueues = num;
	queues = calloc(numqueues, sizeof(msgqueue_t *)); 

	ret = pthread_mutex_unlock(&global_queue_lk);
	assert(ret == 0);

	return 0;
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

	/* for put_msg() */
	putqid = get_queue();
	if (putqid < 0)
		daemon_perror("get_queue");
	/* for get_msg() */
	getqid = get_queue();
	if (getqid < 0)
		daemon_perror("get_queue");

	impose_delay = Malloc(sizeof(int));

	*impose_delay = 1;

	queue_info->inq = queues[putqid];
	assert(queue_info->inq != NULL);
	queue_info->inq->delay_ts = ts;
	queue_info->inq->impose_delay = impose_delay;
	queue_info->outq = queues[getqid];
	assert(queue_info->outq != NULL);
	queue_info->outq->delay_ts = ts;
	queue_info->outq->impose_delay = impose_delay;

	queue_info->inq->delaypair = queue_info->outq;
	queue_info->outq->delaypair = queue_info->inq;

	Pthread_create(NULL, &delay, (void *)queue_info);

	return putqid;
}

void *
delay(void *arg) {
	queue_info_t *queue_info;
	msg_t *msg;
	struct timespec sleeptime, reftime, sleepleft, now;
	int ret;
	
	queue_info = (queue_info_t *)arg;

	for ( ; ; ) {
		msg = get_msg_raw(queue_info->inq, NULL);

		if (*queue_info->inq->impose_delay &&
				queue_info->inq->delay_ts &&
				( queue_info->inq->delay_ts->tv_sec || 
				queue_info->inq->delay_ts->tv_nsec)) {
			clock_gettime(CLOCK_TYPE, &now);
			ts_sum(&reftime, &msg->timestamp, queue_info->inq->delay_ts);
			ret = ts_diff(&sleeptime, &reftime, &now);

			if (ret == 0) {
				/* we enter here only if reftime is in the future */
				do {
					ret = nanosleep(&sleeptime, &sleepleft);
					if (ret) {
						/* sleep was interrupted */
						sleeptime.tv_sec = sleepleft.tv_sec;
						sleeptime.tv_nsec = sleepleft.tv_nsec;
					}
				} while (ret);
			}
		}
		assert(msg->next == NULL);
		put_msg_raw(queue_info->outq, msg);
	}
}
	
int
get_queue(void)
{
	int ret;
	int retval;
	int i;
	
	ret = pthread_mutex_lock(&global_queue_lk);
	assert(ret == 0);

	for (i = 0; i < numqueues ; i++) {
		if (queues[i] == NULL)
			break;
	}

	if (i == numqueues) {
		/* all the queues are already in use */
		errno = ENOSPC;
		retval = -1;
	} else {
		queues[i] = Malloc(sizeof(msgqueue_t));
		memset(queues[i], 0, sizeof(msgqueue_t));
		pthread_cond_init(&queues[i]->cv, NULL);
		pthread_mutex_init(&queues[i]->mx, NULL);
		retval = i;
	}
	pthread_mutex_unlock(&global_queue_lk);
	return retval;
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
	
        mq = queues[msqid];
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

int
set_delay(int msqid, const struct timespec *ts)
{
	msgqueue_t *mq;
	int ret;

        mq = queues[msqid];
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

	mq = queues[msqid];
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

	mq = queues[msqid];
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
 * get_msg_raw	- retuns the first message from the message queue
 */
msg_t *
get_msg_raw(msgqueue_t *mq, const struct timespec *timeout)
{
	msg_t *msg;
	int ret;

	ret = pthread_mutex_lock(&mq->mx);
	assert(ret == 0);
	msg = NULL;

	/* the queue is now empty, wait for messages */
	while (mq->head == NULL)
		if (timeout == NULL)
			ret = pthread_cond_wait(&mq->cv, &mq->mx);
		else
			ret = pthread_cond_timedwait(&mq->cv, &mq->mx, timeout);
			
	if (ret == 0) {
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
get_msg(int msqid, void *msgp, size_t maxsize, long int msgtype, int msgflag)
{
	return get_msg_timed(msqid, msgp, maxsize, msgtype, msgflag, NULL);
}

size_t
get_msg_timed(int msqid, void *msgp, size_t maxsize, long int msgtype, int msgflag, const struct timespec *timeout)
{
	msgqueue_t *mq;
	msg_t *msg;
	size_t msglen;
	size_t msgsize;

	mq = queues[msqid];
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
		free(msg->msgp);
		free(msg);
	}
	
		return msglen;
}

size_t
in_queue_len(int msgid)
{
  msgqueue_t *mq;

  mq = queues[msgid];

  assert(mq);

  return mq->msgcount;
}

size_t
out_queue_len(int msgid)
{
  msgqueue_t *mq;

  mq = queues[msgid];
  assert(mq);

  if (mq->delaypair)
    return mq->delaypair->msgcount;


  return in_queue_len(msgid);
}
