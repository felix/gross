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

#include "common.h"
#include "syncmgr.h"
#include "utils.h"
#include "msgqueue.h"

/* prototypes of internals */
int recv_config_sync(peer_t* peer);
static void *syncmgr(void *arg);
int send_update_msg_as_oper_sync(void *arg);


int 
min(int x, int y) 
{
  if (x<y)
    return x;

  return y;
}

startup_sync_t
sston(startup_sync_t ss)
{
  int i;
  startup_sync_t ret_value;
  

  ret_value.buffer = htonl(ss.buffer);
  ret_value.index = htonl(ss.index);

  for (i=0 ; i<FILTER_SIZE ; i++) {
    ret_value.filter[i] = htonl(ss.filter[i]);
  }

  return ret_value;
}

startup_sync_t
sstoh(startup_sync_t ss)
{
  int i;
  startup_sync_t ret_value;
  

  ret_value.buffer = ntohl(ss.buffer);
  ret_value.index = ntohl(ss.index);

  for (i=0 ; i<FILTER_SIZE ; i++) {
    ret_value.filter[i] = ntohl(ss.filter[i]);
  }

  return ret_value;
}

sha_256_t
dton(sha_256_t digest)
{
  sha_256_t ret_value;

  ret_value.h0 = htonl(digest.h0);
  ret_value.h1 = htonl(digest.h1);
  ret_value.h2 = htonl(digest.h2);
  ret_value.h3 = htonl(digest.h3);
  ret_value.h4 = htonl(digest.h4);
  ret_value.h5 = htonl(digest.h5);
  ret_value.h6 = htonl(digest.h6);
  ret_value.h7 = htonl(digest.h7);

  return ret_value;
}

sha_256_t
dtoh(sha_256_t digest)
{
  sha_256_t ret_value;

  ret_value.h0 = ntohl(digest.h0);
  ret_value.h1 = ntohl(digest.h1);
  ret_value.h2 = ntohl(digest.h2);
  ret_value.h3 = ntohl(digest.h3);
  ret_value.h4 = ntohl(digest.h4);
  ret_value.h5 = ntohl(digest.h5);
  ret_value.h6 = ntohl(digest.h6);
  ret_value.h7 = ntohl(digest.h7);

  return ret_value;
}

sync_config_t
scton(sync_config_t* sync)
{
  sync_config_t tmp;
  tmp.filter_size = htonl(sync->filter_size);
  tmp.num_bufs = htonl(sync->num_bufs);

  return tmp;
}

sync_config_t
sctoh(sync_config_t* sync)
{
  sync_config_t tmp;
  tmp.filter_size = ntohl(sync->filter_size);
  tmp.num_bufs = ntohl(sync->num_bufs);

  return tmp;
}

int 
send_update_to_peer(peer_t* peer, void* ptr, int size)
{
  int ret;

  pthread_mutex_lock( &(peer->peer_in_mutex) );
  ret = writen(peer->connected, ptr, size);
  pthread_mutex_unlock( &(peer->peer_in_mutex) );
  
  return ret;
}


int
send_sync_config(peer_t* peer, sync_config_t* sync)
{
  sync_config_t tmp = scton(sync);

  return send_update_to_peer(peer, &tmp, sizeof(tmp));
}


int
send_startup_sync(peer_t* peer, startup_sync_t* sync)
{
  sync_msg_t prologue;
  char buf[sizeof(sync_msg_t) + sizeof(startup_sync_t)] = { 0x00 };
  startup_sync_t tmp = sston(*sync);

  prologue.type = htonl(STARTUP_SYNC);
  prologue.length = htonl(sizeof(startup_sync_t));

  memcpy(buf, &prologue, sizeof(sync_msg_t));
  memcpy(buf+sizeof(sync_msg_t), &tmp, sizeof(startup_sync_t));
  return send_update_to_peer(peer, buf, sizeof(sync_msg_t) + sizeof(startup_sync_t));
}

int
send_oper_sync(peer_t* peer, oper_sync_t* sync)
{
  sync_msg_t prologue;
  char buf[sizeof(sync_msg_t) + sizeof(oper_sync_t)] = { 0x00 };

  prologue.type = htonl(OPER_SYNC);
  prologue.length = htonl(sizeof(oper_sync_t));

  sync->digest = dton(sync->digest);
  memcpy(buf, &prologue, sizeof(sync_msg_t));
  memcpy(buf+sizeof(sync_msg_t), sync, sizeof(oper_sync_t));
  return send_update_to_peer(peer, buf, sizeof(sync_msg_t) + sizeof(oper_sync_t));
}

int
send_update_msg_as_oper_sync(void *arg)
{
	update_message_t *update;
	sync_msg_t prologue;
	char buf[sizeof(sync_msg_t) + sizeof(oper_sync_t)] = { 0x00 };
	sha_256_t digest;
	oper_sync_t os;

	update = (update_message_t *)arg;
	assert(update);

	if (update->mtype == UPDATE) {
		memcpy(&digest, update->mtext, sizeof(sha_256_t)); 

		prologue.type = htonl(OPER_SYNC);
		prologue.length = htonl(sizeof(oper_sync_t));

		os.digest = dton(digest);
		memcpy(buf, &prologue, sizeof(sync_msg_t));
		memcpy(buf+sizeof(sync_msg_t), &os, sizeof(oper_sync_t));
		return send_update_to_peer(&ctx->config.peer, buf,
			sizeof(sync_msg_t) + sizeof(oper_sync_t));
	}

	return 0;
}

int
force_peer_aggregate(peer_t* peer)
{
  sync_msg_t prologue;
  prologue.type = htonl(AGGREGATE_SYNC);
  prologue.length = 0;

  return send_update_to_peer(peer, &prologue, sizeof(sync_msg_t));
}

void *
recv_syncs(void *arg)
{
  int ret;
  peer_t* peer = &(ctx->config.peer);

  if ( !peer ) {
    daemon_shutdown(1, "Null peer pointer.");
  }

  /* logstr(GLOG_INFO, "Startup syncer started: %s", peer->peer_name); */

  /* Ensure config first. Does not return on failure */

  recv_config_sync(peer);

  while( TRUE ) {
    ret = recv_sync_msg(peer);
    /* logstr(GLOG_DEBUG, "Recv returned %d", ret); */
    if ( 0x00 == ret ) {
      return NULL;
    }
  }
}

int
recv_sync_msg(peer_t* peer)
{
  sync_msg_t msg = { -1 };
  int ret = readn(peer->connected, &msg, sizeof(msg));
  update_message_t update;

  if ( ERROR == ret ) {
      /* error */
      peer->connected = 0;
      logstr(GLOG_ERROR, "read returned error");
      return 0;
    } else if ( EMPTY == ret ) {
      /* connection closed */
      peer->connected = 0;
      logstr(GLOG_INFO, "connection closed by client");
      return 0;
    }

  switch ( ntohl(msg.type) ) {
  case STARTUP_SYNC:
    /* logstr(GLOG_DEBUG, "Recv startup sync"); */
    return recv_startup_sync(peer);
    break;
  case OPER_SYNC:
    logstr(GLOG_DEBUG, "Recv oper sync");
    return recv_oper_sync(peer);
    break;
  case AGGREGATE_SYNC:
    logstr(GLOG_INFO, "Startup sync received. Syncing aggregate");
    update.mtype = SYNC_AGGREGATE;
    ret = instant_msg(ctx->update_q, &update, 0, 0);
    /* sleep for a while to allow the message pass the queue */
    sleep(1);
    return ! ret;
    break;
  default:
    logstr(GLOG_ERROR, "Unknown sync message type.");
    WITH_SYNC_GUARD(peer->connected = 0;
		    close(peer->peerfd_out););
    return -1;
    break;
  }
  
  /* never reached */

}

int
recv_startup_sync(peer_t* peer)
{
  startup_sync_t msg = { -1 };
  int ret = readn(peer->connected, &msg, sizeof(msg));
  update_message_t update;

  if ( ret != sizeof(msg) ) logstr(GLOG_ERROR, "read too few bytes");

  if ( ERROR == ret ) {
      /* error */
      peer->connected = 0;
      logstr(GLOG_ERROR, "read returned error");
      return 0;
    } else if ( EMPTY == ret ) {
      /* connection closed */
      peer->connected = 0;
      logstr(GLOG_INFO, "connection closed by client");
      return 0;
    }

  msg = sstoh(msg);

  update.mtype = ABSOLUTE_UPDATE;
  memcpy(update.mtext, &msg, sizeof(msg));
  return ! instant_msg(ctx->update_q, &update, sizeof(msg), 0);
}
  
int
recv_oper_sync(peer_t* peer)
{
  oper_sync_t msg;
  int ret = readn(peer->connected, &msg, sizeof(msg));
  update_message_t update;

  if ( ERROR == ret ) {
      /* error */
      peer->connected = 0;
      logstr(GLOG_ERROR, "read returned error");
      return 0;
    } else if ( EMPTY == ret ) {
      /* connection closed */
      peer->connected = 0;
      logstr(GLOG_INFO, "connection closed by client");
      return 0;
    }

  update.mtype = UPDATE_OPER;
  msg.digest = dtoh(msg.digest);
  memcpy(update.mtext, &(msg.digest), sizeof(msg.digest));
  return ! put_msg(ctx->update_q, &update, sizeof(msg.digest), 0);
}

int
recv_config_sync(peer_t* peer)
{
  sync_config_t msg;
  int ret = readn(peer->connected, &msg, sizeof(msg));

  if ( ERROR == ret ) {
      /* error */
      peer->connected = 0;
      daemon_shutdown(1, "recv_config_sync: read returned error");
    } else if ( EMPTY == ret ) {
      /* connection closed */
      peer->connected = 0;
      daemon_shutdown(1, "recv_config_sync: connection closed by client");
    } 

  msg = sctoh(&msg);
  if ( (msg.filter_size != ctx->config.filter_size) || (msg.num_bufs != ctx->config.num_bufs) ) {
    daemon_shutdown(1, "Configs differ!\nMy:   filter_size %d number_buffers %d\nPeer: filter_size %d number_buffers %d\n",
		    ctx->config.filter_size, ctx->config.num_bufs, msg.filter_size, msg.num_bufs);
  }

  return 1; /* Ok */
}    

void 
start_syncer(void *arg)
{
	Pthread_create(NULL, &recv_syncs, NULL);
}


void
send_filters(peer_t* peer)
{
  int ret = -1;
  int i,j;
  int index;
  startup_sync_t msg;
  char *err; 
  int size = min(FILTER_SIZE,ctx->filter->group->filter_group[0]->size);

  for ( i=0 ; i<ctx->filter->group->group_size ; i++ ) {
    bzero(msg.filter, sizeof(bitarray_base_t)*FILTER_SIZE);
    index = 0;
    for ( j=0 ; j<ctx->filter->group->filter_group[i]->size ; j++ ) {
      msg.filter[j - index * FILTER_SIZE] = ctx->filter->group->filter_group[i]->filter[j];
      if ( (j % size) == (size-1) ) {
	msg.buffer = i;
	msg.index = index;

	ret = send_startup_sync(peer, &msg);
	if (ret < 0) {
	  err = strerror(errno);

	  logstr(GLOG_ERROR, "Send filters: %s", err);
	}
	index++;
	bzero(msg.filter, sizeof(bitarray_base_t)*FILTER_SIZE);
      }
    }
    logstr(GLOG_DEBUG, "Sent buffer: %d", i);
  }

  logstr(GLOG_DEBUG, "Forcing peer aggregate sync");
  force_peer_aggregate(peer);
}

int
synchronize(peer_t* peer, int syncfd) {
  int opt = 0;
  socklen_t clen = sizeof(struct sockaddr_in);
  char ipstr[INET_ADDRSTRLEN];
  int ret;
  sync_config_t conf;
  update_message_t rotatecmd;

  peer->peerfd_out = socket(AF_INET, SOCK_STREAM, 0);
  
  ret = setsockopt(peer->peerfd_out, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if ( ret )
	daemon_perror("setsockopt");
  
  /* Try connect to peer */
  if ( connect(peer->peerfd_out, (struct sockaddr*)&(peer->peer_addr), clen) != 0 ) {
    /* Miserable failure */
    if ( NULL == inet_ntop(AF_INET, &(peer->peer_addr.sin_addr), ipstr, INET_ADDRSTRLEN) )
	daemon_perror("inet_ntop");

    logstr(GLOG_INFO, "Connect to peer failed, host %s, port %d", ipstr, ntohs(peer->peer_addr.sin_port));

    ret = sem_post(ctx->sync_guard);

    if ( ret )
	daemon_perror("pthread_mutex_unlock");

  } else {
    /* Glorious success */
    logstr(GLOG_DEBUG, "Peer fd %d", peer->peerfd_out);
    peer->connected = peer->peerfd_out;
    rotatecmd.mtype = ROTATE;
    instant_msg(ctx->update_q, &rotatecmd, 0, 0);

    start_syncer(NULL);
  }
  
  /* Start listening to incoming sync-requests */
  if ( NULL == inet_ntop(AF_INET, &(ctx->config.sync_host.sin_addr), ipstr, INET_ADDRSTRLEN) )
	daemon_perror("inet_ntop");

  while(TRUE) {
    logstr(GLOG_INFO, "Waiting sync connection on host %s port %d", ipstr, ntohs(ctx->config.sync_host.sin_port));
    peer->peerfd_in = accept(syncfd, (struct sockaddr *)&(ctx->config.sync_host), &clen);
    peer->connected = peer->peerfd_in;
    logstr(GLOG_INFO, "Got sync connection");
    
    if (peer->peerfd_in < 0) {
      if (errno != EINTR) {
	perror("Peer sync in accept()");
	return 0;
      }
    }

    conf.filter_size = ctx->config.filter_size;
    conf.num_bufs = ctx->config.num_bufs;

    logstr(GLOG_INFO, "Examining peer config");
    send_sync_config(peer, &conf);

    WITH_SYNC_GUARD(
	queue_freeze(ctx->update_q);
	send_filters(peer);
	walk_queue(ctx->update_q, &send_update_msg_as_oper_sync);
	queue_thaw(ctx->update_q);
    );

    logstr(GLOG_INFO, "Sent filters. Waiting for oper syncs");
    do {
      ret = recv_sync_msg(peer);
      /* logstr(GLOG_DEBUG, "Recv returned %d", ret); */
    } while ( 0x00 != ret );
  }

  /* never reached 
   * logstr(GLOG_INFO, "Peer %s connected", peer->peer_name);
   */
}

static void *
syncmgr(void *arg)
{
  int ret = -1;
  int syncfd = -1;
  int opt = -1;

  syncfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (syncfd < 0) {
    logstr(GLOG_CRIT, "Cannot listen to sync port. Got errno: %d", errno);
    pthread_exit(NULL);
  }

  opt=1;
  ret = setsockopt(syncfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  if (ret < 0) {
    logstr(GLOG_CRIT, "Socket option setting failed");
    pthread_exit(NULL);
  }

  ret = bind(syncfd, (struct sockaddr *)&(ctx->config.sync_host), sizeof(struct sockaddr_in));
  if (ret < 0) {
    logstr(GLOG_CRIT, "Bind failed in syncmgr, port %d", ntohs(ctx->config.sync_host.sin_port));
    pthread_exit(NULL);
  }
  
  ret = listen(syncfd, ctx->config.max_connq);
  if (ret < 0) {
    logstr(GLOG_CRIT, "Listen failed in syncmgr");
    pthread_exit(NULL);
  }

  synchronize(&(ctx->config.peer), syncfd);

  return NULL;
}

void
syncmgr_init() {
	sem_wait(ctx->sync_guard);
        Pthread_create(&ctx->process_parts.syncmgr, &syncmgr, NULL);
}
