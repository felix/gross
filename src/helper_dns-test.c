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

/* dummy context */
gross_ctx_t *ctx;

int
main(int argc, char **argv)
{
	gross_ctx_t myctx = { 0x00 }; /* dummy context */
	struct hostent *host;
	char *foo;

        ctx = &myctx;
	ctx->config.loglevel = GLOG_EMERG;

	helper_dns_init();

	host = Gethostbyname("www.utu.fi", 0);
	printf("got: %s\n", host->h_name);
	
	return(0);
}
