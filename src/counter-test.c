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
#include "counter.h"

#define LOOPSIZE 100
#define THREADCOUNT 1000
#define JOINTCOUNTERS 10

/* internal functions */
static void *countertest(void *arg); 

/* dummy context */
gross_ctx_t *ctx;
unsigned *seed;

static void *
countertest(void *arg)
{
	int c1, c2;
	int *jc;
	int counters[LOOPSIZE];
	int i;
	int *ep, errors;

	jc = (int *)arg;

	/* create two counters */
	c1 = counter_create("c1", "first counter");
	c2 = counter_create("c2", "second counter");

	for (i=0; i < LOOPSIZE; i++) {
		counter_increment(c1);
		counter_increment(c2);
		counter_increment(jc[rand_r(seed) % JOINTCOUNTERS]);
	}

	if (counter_read(c1) != LOOPSIZE)
		errors++;
	if (counter_read(c2) != LOOPSIZE)
		errors++;

	counter_restart(c1);
	for (i=0; i < LOOPSIZE; i++)
		counter_decrement(c2);

	if (counter_read(c1) != 0)
		errors++;
	if (counter_read(c2) != 0)
		errors++;

	for (i=0; i < 10; i++) {
		counter_increment(c1);
		counter_increment(c2);
	}

	counter_release(c1);
	counter_release(c2);
	c1 = counter_create("c1", "first counter");
	c2 = counter_create("c2", "second counter");

	if (counter_read(c1) != 0)
		errors++;
	if (counter_read(c2) != 0)
		errors++;

	for (i=0; i < LOOPSIZE; i++)
		counters[i] = counter_create(NULL, NULL);
	for (i=0; i < LOOPSIZE; i++)
		counter_release(counters[i]);
	for (i=0; i < LOOPSIZE; i++)
		counters[i] = counter_create(NULL, NULL);
	for (i=0; i < LOOPSIZE; i++)
		counter_release(counters[i]);

	ep = Malloc(sizeof(int));
	*ep = errors;
	pthread_exit(ep);
}

int
main(int argc, char **argv)
{
	thread_info_t threads[THREADCOUNT];
	int jc[JOINTCOUNTERS];
	int ret;
	int *ep;
	int i;
	int sum = 0;
	gross_ctx_t myctx = { 0x00 }; /* dummy context */
	unsigned myseed = time(NULL);

	ctx = &myctx;
	seed = &myseed;

	/* create a counter that all the threads will be incrementing */
	for (i=0; i < JOINTCOUNTERS; i++)
		jc[i] = counter_create("jc", "joint counter");

	for (i=0; i < THREADCOUNT; i++)
		create_thread(&threads[i], 0, &countertest, jc);

	for (i=0; i < THREADCOUNT; i++) {
		ret = pthread_join(*threads[i].thread, (void **)&ep);
		if (ret == 0) {
			if (*ep != 0)
				return 1;
		} else {
			return 2;
		}
	}

	for (i=0; i < JOINTCOUNTERS; i++)
		sum += counter_read(jc[i]);

	if (sum != THREADCOUNT * LOOPSIZE) 
		return 3;
	else
		return 0;
}
