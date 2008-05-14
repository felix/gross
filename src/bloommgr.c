/* $Id$ */

/*
 * Copyright (c) 2006, 2007
 *               Antti Siira <antti@utu.fi>
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

#include "common.h"
#include "srvutils.h"
#include "syncmgr.h"
#include "msgqueue.h"

/* prototypes */
static void *bloommgr(void *arg);

static void *
rotate(void *arg)
{
	logstr(GLOG_INFO, "rotate thread starting");

	if ((time(NULL) - *ctx->last_rotate) <= ctx->config.rotate_interval) {
		logstr(GLOG_DEBUG, "rotation not needed");
		return NULL;
	}

	/*      debug_print_ring_queue(ctx->filter, TRUE); */
	logstr(GLOG_DEBUG, "Now: %d Last: %d Max-diff %d", time(NULL), *(ctx->last_rotate),
	    ctx->config.rotate_interval * ctx->config.num_bufs);
	ACTIVATE_BLOOM_GUARD();
	if (time(NULL) - *(ctx->last_rotate) > ctx->config.rotate_interval * ctx->config.num_bufs) {
		zero_bloom_ring_queue(ctx->filter);
		*(ctx->last_rotate) = time(NULL);
		logstr(GLOG_INFO, "Max timediff exceeded. Zeroing whole bloom ring.");
	} else {
		*(ctx->last_rotate) += ctx->config.rotate_interval;
		ctx->filter = rotate_bloom_ring_queue(ctx->filter);
	}
	RELEASE_BLOOM_GUARD();
	logstr(GLOG_DEBUG, "rotation completed");
	return NULL;
}

static void *
bloommgr(void *arg)
{
	update_message_t message;
	sha_256_t digest;
	int ret;
	size_t size;
	startup_sync_t ss;

	ctx->filter = build_bloom_ring(ctx->config.num_bufs, ctx->config.filter_size);

	logstr(GLOG_INFO, "bloommgr starting...");

	sem_post(ctx->sync_guard);

	/* pseudo-loop */
	for (;;) {
		size = get_msg(ctx->update_q, &message, MSGSZ);
		if (size < 0) {
			gerror("get_msg bloommgr");
			continue;
		}
		switch (message.mtype) {
		case UPDATE:
			logstr(GLOG_DEBUG, "received update command");
			memcpy(&digest, message.mtext, sizeof(sha_256_t));
			ACTIVATE_BLOOM_GUARD();
			insert_digest_bloom_ring_queue(ctx->filter, digest);
			RELEASE_BLOOM_GUARD();
			/* debug_print_digest(digest, TRUE); */
			break;
		case UPDATE_OPER:
			/* logstr(GLOG_DEBUG, "received update oper command"); */
			memcpy(&digest, message.mtext, sizeof(sha_256_t));
			ACTIVATE_BLOOM_GUARD();
			insert_digest_bloom_ring_queue(ctx->filter, digest);
			RELEASE_BLOOM_GUARD();
			break;
		case ABSOLUTE_UPDATE:
			memcpy(&ss, message.mtext, sizeof(ss));
			/* logstr(GLOG_INSANE, "Absolute update, buffer %d, index %d", ss.buffer, ss.index); */
			ACTIVATE_BLOOM_GUARD();
			insert_absolute_bloom_ring_queue(ctx->filter, ss.filter, FILTER_SIZE,
			    ss.index, ss.buffer);
			RELEASE_BLOOM_GUARD();
			break;
		case ROTATE:
			logstr(GLOG_INFO, "received rotate command");
			/* debug_print_ring_queue(ctx->filter, TRUE); */
			create_thread(NULL, DETACH, &rotate, NULL);
			break;
		case SYNC_AGGREGATE:
			sync_aggregate(ctx->filter);
			ret = sem_post(ctx->sync_guard);
			if (ret)
				daemon_fatal("pthread_mutex_unlock");
			break;
		default:
			logstr(GLOG_ERROR, "Unknown message type in update queue");
			break;
		}
	}

	/*
	 * never reached: 
	 * release_bloom_ring_queue(ctx->filter);
	 */
}

void
bloommgr_init()
{
	sem_wait(ctx->sync_guard);
	create_thread(&ctx->process_parts.bloommgr, DETACH, &bloommgr, NULL);
}
