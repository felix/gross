/*
 * Copyright (c) 2006,2007 Eino Tuominen <eino@utu.fi>
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

#define DEFAULT_CONFIG	"update",		"grey", 	\
			"host",			"127.0.0.1",	\
			"port",			"1111",		\
			"sync_port",		"1112",		\
			"status_port",		"1121",		\
			"rotate_interval", 	"3600",		\
			"filter_bits",		"22",		\
			"number_buffers",	"8",            \
			"stat_interval",	"300",		\
			"sjsms_response_grey",	"$X4.4.3|$NPlease$ try$ again$ later", \
			"sjsms_response_match",	"$Y", 		\
			"sjsms_response_trust",	"$Y",		\
			"log_method",		"syslog",	\
			"log_level",		"info",		\
			"stat_type",		"delay",	\
			"stat_type",		"status",	\
			"grey_mask",		"0",		\
			"grey_delay",		"10",           \
			"check",		"dnsbl",	\
			"syslog_facility",	"mail",		\
			"blocker_port",		"4466",		\
			"query_timelimit",	"5000"

#define MULTIVALUES	"dnsbl",	\
			"check",	\
                        "stat_type",	\
			"protocol", 	\
			"log_method"

#define VALID_NAMES     "dnsbl",			\
			"host",				\
			"port",				\
                        "filter_bits",			\
                        "rotate_interval",		\
                        "number_buffers",		\
                        "update",			\
                        "peer_name",			\
                        "statefile",			\
			"sjsms_response_grey",		\
			"sjsms_response_match",		\
			"sjsms_response_trust",		\
                        "log_method",			\
                        "log_level",			\
			"grey_mask",			\
                        "grey_delay",               	\
			"check",			\
			"protocol",			\
                        "syslog_facility",		\
                        "sync_listen",			\
                        "sync_peer",			\
                        "sync_port",			\
                        "stat_interval",		\
                        "stat_type",			\
			"status",			\
			"blocker_host",			\
			"blocker_port",			\
			"query_timelimit"

#define DEPRECATED_NAMES 	"syncport",		\
				"synchost",		\
				"peerport",		\
				"peerhost",		\
				"statushost",		\
				"statusport"		

typedef struct configlist_s {
	bool is_default;
	const char *name;
	const char *value;
	struct configlist_s *next;  /* linked list */
} configlist_t;

configlist_t *read_config(const char *filename);
const char *gconf(configlist_t *config, const char *name);

#endif /* CONF_H */
