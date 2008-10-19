/* $Id$ */

/*
 * Copyright (c) 2008
 *               Eino Tuominen <eino@utu.fi>
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

#ifndef HELPER_DNS_H
#define HELPER_DNS_H

#include <netdb.h>

typedef enum { HOSTBYNAME, HOSTBYADDR } dns_request_type_t;

typedef  unsigned long  int  ub4;

typedef struct dns_request_s
{
	dns_request_type_t type;
	void *query;
#if ARES_VERSION_MAJOR > 0 && ARES_VERSION_MINOR > 4
	void (*callback)(void *arg, int status, int timeouts, struct hostent *host);
#else
	void (*callback)(void *arg, int status, struct hostent *host);
#endif
	void *cba;
} dns_request_t;

void helper_dns_init();
struct hostent *Gethostbyname(const char *name, mseconds_t timeout);
struct hostent *Gethostbyaddr(const char *addr, mseconds_t timeout);
struct hostent *Gethostbyaddr_str(const char *addr, mseconds_t timeout);
ub4 one_at_a_time(char *key, ub4 len);

#endif /* #ifndef HELPER_DNS_H */
