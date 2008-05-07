/* $Id$ */

/*
 * Copyright (c) 2008
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
#include "srvutils.h"
#include "msgqueue.h"

#define LOOPSIZE 100
#define THREADPAIRS 500
#define TIMELIMIT 10000

typedef struct queuepair_s {
	int inq;
	int outq;
} queuepair_t;

typedef struct test_message_s
{
        long mtype;
        int *counter;
} test_message_t;

/* internal functions */
static void *msgqueueping(void *arg); 

static void *
msgqueueping(void *arg)
{
	queuepair_t *qpair;
	size_t size;
	int *ret;
	test_message_t message;
	int i;

	ret = Malloc(sizeof(int));
	*ret = 0;

	qpair = (queuepair_t *)arg;

	for (i=0; i < LOOPSIZE; i++) {
		size = get_msg_timed(qpair->inq, &message, sizeof(int *), 0, TIMELIMIT);
		if (size == 0) {
			printf("timeout\n");
			*ret = 1;
			goto OUT;
		} else {
			(*message.counter)++;
			put_msg(qpair->outq, &message, sizeof(int *), 0);
		}
	}
OUT:
	pthread_exit(ret);
}

int
main(int argc, char **argv)
{
	thread_info_t threads[THREADPAIRS * 2];
	int balls[THREADPAIRS];
	test_message_t message;
	int ret;
	int i;
	int qa, qb;
	int *exitvalue;
	int sum = 0;
	queuepair_t qpair_ping;
	queuepair_t qpair_pong;
	gross_ctx_t myctx = { 0x00 }; /* dummy context */
	ctx = &myctx;

	qa = get_queue();
	qb = get_queue();

	qpair_ping.inq = qa;
	qpair_ping.outq = qb;
	qpair_pong.inq = qb;
	qpair_pong.outq = qa;

	printf("Check: msgqueue\n");

	printf("  Creating %d threads to test the message queues...", THREADPAIRS * 2);
	fflush(stdout);
	/* start the threads */
	for (i=0; i < THREADPAIRS; i++) {
		create_thread(&threads[i*2], 0, &msgqueueping, &qpair_ping);
		create_thread(&threads[i*2+1], 0, &msgqueueping, &qpair_pong);
	}
	printf("  Done.\n");

	printf("  Sending out %d chain letters...", THREADPAIRS);
	fflush(stdout);
	/* serve ping pong balls */
	for (i=0; i < THREADPAIRS; i++) {
		balls[i] = 0;
		message.mtype = 0;
		message.counter = &balls[i];
		put_msg(qa, &message, sizeof(int *), 0);
	}
	printf("  Done.\n");

	printf("  Waiting for the results...");
	fflush(stdout);
	for (i=0; i < THREADPAIRS * 2; i++) {
		ret = pthread_join(*threads[i].thread, (void **)&exitvalue);
		if (ret == 0) {
			if (*exitvalue != 0) {
				printf(" Thread returned %d (!= 0)\n", *exitvalue);
				return 1;
			}
		} else {
			perror("pthread_join:");
			return 2;
		}
		Free(threads[i].thread);
		Free(exitvalue);
	}
	printf("  Done.\n");

	for (i=0; i < THREADPAIRS; i++)
		sum += balls[i];

	if (sum != LOOPSIZE * THREADPAIRS * 2)
		return 3;
	else 
		return 0;
}
