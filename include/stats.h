/* -*- mode:c; coding:utf-8 -*-
 *
 * Copyright (c) 2007 Antti Siira <antti@utu.fi>
 *                    Eino Tuominen <eino@utu.fi>
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

#ifndef STATS_H
#define STATS_H

struct dnsbl_stat {
  char *dnsbl_name;
  uint64_t matches_startup;
  struct dnsbl_stat *next;
};

typedef struct dnsbl_stat dnsbl_stat_t;

typedef struct {
  time_t startup;
  time_t begin;
  time_t end;
  pthread_mutex_t mx;
  uint64_t block;
  uint64_t greylist;
  uint64_t match;
  uint64_t trust;
  uint64_t all_block;
  uint64_t all_greylist;
  uint64_t all_match;
  uint64_t all_trust;
  double block_avg_delay;
  double greylist_avg_delay;
  double match_avg_delay;
  double trust_avg_delay;
  double block_max_delay;
  double greylist_max_delay;
  double match_max_delay;
  double trust_max_delay;
  dnsbl_stat_t *dnsbl_match;
} stats_t;

void init_stats();
stats_t zero_stats();
stats_t log_stats();

double block_delay_update(double d);
double greylist_delay_update(double d);
double match_delay_update(double d);
double trust_delay_update(double d);
uint64_t stat_dnsbl_match(const char *name);
int stat_add_dnsbl(const char *name);
char *dnsbl_stats(char *buf, int32_t size);


#define WITH_STATS_GUARD(X) { pthread_mutex_lock( &(ctx->stats.mx) ); X; pthread_mutex_unlock( &(ctx->stats.mx) ); }
#define INCF_STATS(member) { WITH_STATS_GUARD( ++(ctx->stats.member) ;) }

#endif /* STATS_H */
