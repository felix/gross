/*
 * Copyright (c) 2006 Eino Tuominen <eino@utu.fi>
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

#ifndef CONF_H
#define CONF_H

#define VALID_NAMES     "dnsbl",             \
			"peerhost",	     \
			"peerport",          \
			"host",              \
			"port",              \
			"syncport",          \
			"synchost",          \
                        "filter_bits",       \
                        "rotate_interval",   \
                        "number_buffers",    \
                        "update",            \
                        "peer_name",         \
                        "statefile",         \
                        "status_host",       \
                        "status_port"

typedef struct configlist_s {
	const char *name;
	const char *value;
	struct configlist_s *next;  /* linked list */
} configlist_t;

int add_config_item(configlist_t **current, const char *name, const char *value);
configlist_t *read_config(const char *filename);
const char *dconf(configlist_t *config, const char *name, const char *def);

#endif /* CONF_H */
