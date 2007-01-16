/*
 * Copyright (c) 2006,2007
 *                    Antti Siira <antti@utu.fi>
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

#include <pthread.h>
#include <signal.h>

#include "common.h"
#include "stats.h"
#include "srvutils.h"
#include "msgqueue.h"
#include "utils.h"

/* prototypes */
static void *srvstatus(void *arg);

#define SRV_OK   0x00
#define SRV_WARN 0x01
#define SRV_ERR  0x02

#define MINUTE   60

#define QUEUE_WARN ((unsigned int)30)
#define QUEUE_ERR  ((unsigned int)50)

int test_thread(pthread_t* thread)
{
  int ret = pthread_kill(*thread, 0);

  if ( ret != 0 ) return -1;

  return 1;
}


void get_srvstatus(char* buf, int len) 
{
  int state = SRV_OK;
  unsigned int update_len = out_queue_len(ctx->update_q);
  unsigned int log_len = out_queue_len(ctx->log_q);

  if ( test_thread(ctx->process_parts.bloommgr.thread) == -1) {
    state |= SRV_ERR;
    snprintf(buf, len - strlen(buf), "%d: bloommgr-thread is dead.", state);
  } else if ( ctx->process_parts.syncmgr.thread && test_thread(ctx->process_parts.syncmgr.thread) == -1) {
    state |= SRV_ERR;
    snprintf(buf, len - strlen(buf), "%d: syncmgr-thread is dead.", state);
  } else if ( ctx->process_parts.sjsms_server.thread && test_thread(ctx->process_parts.sjsms_server.thread) == -1) {
    state |= SRV_ERR;
    snprintf(buf, len - strlen(buf), "%d: sjsms_server-thread is dead.", state);
  } else if ( ctx->process_parts.postfix_server.thread && test_thread(ctx->process_parts.postfix_server.thread) == -1) {
    state |= SRV_ERR;
    snprintf(buf, len - strlen(buf), "%d: postfix_server-thread is dead.", state);
  } else if ( time(NULL) - *ctx->last_rotate > ctx->config.rotate_interval + MINUTE) {
    state |= SRV_ERR;
    snprintf(buf, len - strlen(buf), "%d: Rotate stuck.", state);
  } else if (update_len > QUEUE_ERR) {
    state |= SRV_ERR;
    snprintf(buf, len - strlen(buf), "%d: Update queue length %d.", state, update_len);
  } else if (log_len > QUEUE_ERR) {
    state |= SRV_ERR;
    snprintf(buf, len - strlen(buf), "%d: Log queue length %d.", state, log_len);
  } else if (update_len > QUEUE_WARN) {
    state |= SRV_WARN;
    snprintf(buf, len - strlen(buf), "%d: Update queue length %d.", state, update_len);
  } else if (log_len > QUEUE_WARN) {
    state |= SRV_WARN;
    snprintf(buf, len - strlen(buf), "%d: Log queue length %d.", state, log_len);
  } else if ( (ctx->config.peer.connected < 1) && (( ctx->config.flags & FLG_NOREPLICATE ) == 0 ) ) {
    state |= SRV_WARN;
    snprintf(buf, len - strlen(buf), "%d: Peer unreachable.", state);
  } else {
    state |= SRV_OK;
    snprintf(buf, len - strlen(buf), "%d: Grossd OK. Update queue: %d Log queue: %d", 
	     state, update_len, log_len);
    WITH_STATS_GUARD(snprintf(buf+strlen(buf), len - strlen(buf), " Trust: %llu Match: %llu Greylist: %llu Queries/sec: %lf", 
			     ctx->stats.all_trust, ctx->stats.all_match, ctx->stats.all_greylist, (double)(ctx->stats.all_trust + ctx->stats.all_match + ctx->stats.all_greylist)/(double)(time(NULL) - ctx->stats.startup)); );
  }

}

static void *
srvstatus(void *arg)
{
  int ret = -1;
  int statfd = -1;
  int opt = -1;
  int tmpfd = -1;
  char statbuf[MSGSZ] = { 0x00 };
  socklen_t clen = sizeof(struct sockaddr_in);

  statfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (statfd < 0) {
    logstr(GLOG_CRIT, "Srvstatus socket failed.");
    pthread_exit(NULL);
  }

  opt=1;
  ret = setsockopt(statfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  if (ret < 0) {
    logstr(GLOG_CRIT, "Socket option setting failed");
    pthread_exit(NULL);
  }

  ret = bind(statfd, (struct sockaddr *)&(ctx->config.status_host), sizeof(struct sockaddr_in));
  if (ret < 0) {
    logstr(GLOG_CRIT, "Bind failed in statmgr, port %d", ntohs(ctx->config.status_host.sin_port));
    pthread_exit(NULL);
  }

  ret = listen(statfd, ctx->config.max_connq);
  if (ret < 0) {
    logstr(GLOG_CRIT, "Listen failed in statmgr");
    pthread_exit(NULL);
  }

  while(TRUE) {
    memset(statbuf, 0, MSGSZ);
    tmpfd = accept(statfd, (struct sockaddr *)&(ctx->config.status_host), &clen);

    if (tmpfd < 0) {
      perror("Syncfd accept");
      continue;
    }

    get_srvstatus(statbuf, MSGSZ-2);
    statbuf[MSGSZ-1] = '\0';
    statbuf[strlen(statbuf)] = '\n';

    logstr(GLOG_DEBUG, "Telling service status.");
    writen(tmpfd, statbuf, strlen(statbuf));
    close(tmpfd);
  }
}

void
srvstatus_init()
{
	Pthread_create(NULL, &srvstatus, NULL);
}
