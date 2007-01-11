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

#include "common.h"
#include "srvutils.h"

void
init_stats()
{
  pthread_mutex_init( &(ctx->stats.mx), NULL);

  zero_stats(TRUE);
  time( &(ctx->stats.startup) );
  ctx->stats.all_greylist = 0;
  ctx->stats.all_match = 0;
  ctx->stats.all_trust = 0;
}

stats_t
zero_stats()
{
  stats_t old;

  WITH_STATS_GUARD(/* Take a copy of the old values */
		   old = ctx->stats;
		   time( &(old.end) );

		   /* Zero values */
		   ctx->stats.greylist = 0;
		   ctx->stats.match = 0;
		   ctx->stats.trust = 0;
		   ctx->stats.greylist_avg_delay = 0.0;
		   ctx->stats.match_avg_delay = 0.0;
		   ctx->stats.trust_avg_delay = 0.0;
		   time( &(ctx->stats.begin));
		   ctx->stats.end = 0);
  return old;
}

double
greylist_delay_update(double d)
{
  WITH_STATS_GUARD(
		   if (ctx->stats.greylist == 0) {
		     logstr(GLOG_WARNING, "Greylist average updated before updating counters");
		     ctx->stats.greylist_avg_delay = d;
		   } else {
		     ctx->stats.greylist_avg_delay = (ctx->stats.greylist_avg_delay * (ctx->stats.greylist-1) + d)/(ctx->stats.greylist);
		   }

		   if (ctx->stats.greylist_max_delay<d) ctx->stats.greylist_max_delay = d;
		   );

  return ctx->stats.greylist_avg_delay;
}

double 
match_delay_update(double d)
{
  WITH_STATS_GUARD(
		   if (ctx->stats.match == 0) {
		     logstr(GLOG_WARNING, "Match average updated before updating counters");
		     ctx->stats.match_avg_delay = d;
		   } else {
		     ctx->stats.match_avg_delay = (ctx->stats.match_avg_delay * (ctx->stats.match-1) + d)/(ctx->stats.match);
		   }

		   if (ctx->stats.match_max_delay<d) ctx->stats.match_max_delay = d;
		   );
  return ctx->stats.match_avg_delay;
}

double 
trust_delay_update(double d)
{
  WITH_STATS_GUARD(
		   if (ctx->stats.trust == 0) {
		     ctx->stats.trust_avg_delay = d;
		     logstr(GLOG_WARNING, "Trust average updated before updating counters");
		   } else {
		     ctx->stats.trust_avg_delay = (ctx->stats.trust_avg_delay * (ctx->stats.trust-1) + d)/(ctx->stats.trust);
		   }

		   if (ctx->stats.trust_max_delay<d) ctx->stats.trust_max_delay = d;
		   );

  return ctx->stats.trust_avg_delay;
}

stats_t
log_stats()
{
  stats_t stats;
  stats = zero_stats();

  statstr(STATS_STATUS, "grossd status summary (begin, end, trust, match, greylist): %lu, %lu, %llu, %llu, %llu", 
	  stats.begin, stats.end, stats.trust, stats.match, stats.greylist);

  statstr(STATS_DELAY, "grossd processing average delay (begin, end, trust[ms], match[ms], greylist[ms]): %lu, %lu, %.3lf, %.3lf, %.3lf", 
	  stats.begin, stats.end, stats.trust_avg_delay, stats.match_avg_delay, stats.greylist_avg_delay);

  statstr(STATS_DELAY, "grossd processing max delay (begin, end, trust[ms], match[ms], greylist[ms]): %lu, %lu, %.3lf, %.3lf, %.3lf", 
	  stats.begin, stats.end, stats.trust_max_delay, stats.match_max_delay, stats.greylist_max_delay);

  statstr(STATS_STATUS_BEGIN, "grossd summary since startup (startup, now, trust, match, greylist): %lu, %lu, %llu, %llu, %llu", 
	  stats.startup, stats.end, stats.all_trust, stats.all_match, stats.all_greylist);

  return stats;
}
			  
	 
	 
