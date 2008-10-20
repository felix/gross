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
#include "helper_dns.h"

/* internal funtions */
static void *dnstest(void *arg);

/* dummy context */
gross_ctx_t *ctx;

#define THREADCOUNT 200

static void *
dnstest(void *arg)
{
	struct hostent *host;
	int i;
	char buf[INET_ADDRSTRLEN];
	const char *ptr;
	int *errors = arg;
       
	for (i=0; i < 1; i++) {
		host = Gethostbyname("ns1.utu.fi", 0);
		ptr = inet_ntop(AF_INET, host->h_addr_list[0], buf, INET_ADDRSTRLEN);
		assert (ptr);
		if (strcmp("130.232.1.1", buf))
			(*errors)++;
		free_hostent(host);
	}
	pthread_exit(NULL);
}

#if 0
int
main(int argc, char **argv)
{
	thread_info_t threads[THREADCOUNT];
	gross_ctx_t myctx = { 0x00 }; /* dummy context */
	int i = 0;
	struct hostent *host;
	char buf[INET_ADDRSTRLEN];
	const char *ptr;
	int ret;

        ctx = &myctx;
	ctx->config.loglevel = GLOG_EMERG;

	helper_dns_init();

	host = Gethostbyaddr_str("130.232.1.1", 0);
	ptr = inet_ntop(AF_INET, host->h_addr_list[0], buf, INET_ADDRSTRLEN);
	assert (ptr);
	printf("got: %s -> %s\n", host->h_name, buf);
	free_hostent(host);
	host = Gethostbyaddr_str("130.232.1.3", 0);
	ptr = inet_ntop(AF_INET, host->h_addr_list[0], buf, INET_ADDRSTRLEN);
	assert (ptr);
	printf("got: %s -> %s\n", host->h_name, buf);
	free_hostent(host);

	return 0;
}

#else
int
main(int argc, char **argv)
{
	thread_info_t threads[THREADCOUNT];
	gross_ctx_t myctx = { 0x00 }; /* dummy context */
	int i = 0;
	int errors = 0;

        ctx = &myctx;
	ctx->config.loglevel = GLOG_EMERG;

	helper_dns_init();

	/* populate cache */
	create_thread(&threads[0], 0, &dnstest, &errors);
	sleep(1);
	/* fire up THREADCOUNT threads to do dns queries */
	for(i=1; i < THREADCOUNT; i++)
		create_thread(&threads[i], 0, &dnstest, &errors);
	for (i=0; i < THREADCOUNT; i++)
		if (0 != pthread_join(*threads[i].thread, NULL))
			perror("pthread_join");

	return(errors);
}
#endif
