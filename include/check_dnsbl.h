/*
 * Copyright (c) 2006,2007,2008 Eino Tuominen <eino@utu.fi>
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

#ifndef DNSBLC_H
#define DNSBLC_H

#define MAXQUERYSTRLEN 256
#define ERRORTOLERANCE 5

typedef enum { TYPE_DNSBL, TYPE_RHSBL, TYPE_DNSWL } dns_check_type_t;

typedef struct {
	const char *name;
	bool definitive;
	dns_check_type_t type;
	dnsbl_t *dnsbase;
} dns_check_info_t;

typedef struct {
	dnsbl_t *dnsbl;
	bool *done;
	int *timeout;
	const char *querystr;
	edict_t *edict;
	dns_check_info_t *check_info;
} callback_arg_t;

int add_dnsbl(dnsbl_t **current, const char *name, int weight);
int tolerate_dnsbl(dnsbl_t *dnsbl);
int increment_dnsbl_tolerance_counters(dnsbl_t *dnsbl);
void dnsbl_init(dns_check_info_t *check_info, pool_limits_t *limits);

#endif /* DNSBLC_H */
