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
#define QUEUES 8
#define BALLS (16 * QUEUES)
#define QUEUEPAIRS (8 * QUEUES)
#define THREADS (8 * QUEUEPAIRS)
#define TIMELIMIT 10000

typedef struct queuepair_s {
	int inq;
	int outq;
} queuepair_t;

/* internal functions */
static void *msgqueueping(void *arg); 

static void *
msgqueueping(void *arg)
{
	queuepair_t *qpair;
	size_t size;
	int *ret;
	int i;
	int tmp;
	int *counter;

	ret = Malloc(sizeof(int));
	*ret = 0;

	qpair = (queuepair_t *)arg;

	for (i=0; i < LOOPSIZE; i++) {
		size = get_msg_timed(qpair->inq, &counter, sizeof(int *), TIMELIMIT);
		if (size == 0) {
			printf("  timeout\n");
			goto OUT;
		} else {
			/* avoid ++ to lure out concurrency problems */
			tmp = *counter + 1;
			usleep(1000);
			*counter = tmp;
			put_msg(qpair->outq, &counter, sizeof(int *));
		}
	}
OUT:
	pthread_exit(ret);
}

int
main(int argc, char **argv)
{
	thread_info_t threads[THREADS];
	int balls[BALLS];
	int queues[QUEUES];
	queuepair_t qpairs[QUEUEPAIRS];
	int *counter; 
	int ret;
	int i;
	int *exitvalue;
	int sum = 0;
	gross_ctx_t myctx = { 0x00 }; /* dummy context */
	ctx = &myctx;

	printf("Check: msgqueue\n");

	printf("  Creating %d message queues...", QUEUES);
	fflush(stdout);
	for (i=0; i < QUEUES; i++)
		queues[i] = get_queue();
	printf("  Done.\n");
 
	printf("  Making %d circular queue pairs...", QUEUEPAIRS);
	for (i=0; i < QUEUEPAIRS; i++) {
		qpairs[i].inq = queues[i % QUEUES];
		qpairs[i].outq = queues[(i + 1) % QUEUES];
	}
	printf("  Done.\n");

	printf("  Creating %d threads to test the message queues...", THREADS);
	fflush(stdout);
	/* start the threads */
	for (i=0; i < THREADS; i++) 
		create_thread(&threads[i], 0, &msgqueueping, &qpairs[i % QUEUEPAIRS]);
	printf("  Done.\n");

	printf("  Sending out %d chain letters...", BALLS);
	fflush(stdout);
	/* serve ping pong balls */
	for (i=0; i < BALLS; i++) {
		balls[i] = 0;
		counter = &balls[i];
		put_msg(queues[i % QUEUES], &counter, sizeof(int *));
	}
	printf("  Done.\n");

	printf("  Waiting for the results...");
	fflush(stdout);
	for (i=0; i < THREADS; i++) {
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

	for (i=0; i < BALLS; i++)
		sum += balls[i];

	if (sum != LOOPSIZE * THREADS)
		return 3;
	else 
		return 0;
}
