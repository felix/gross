/*
 * Copyright (c) 2006 Antti Siira <antasi@iki.fi>
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

#ifndef SYNCMGR_H
#define SYNCMGR_H

#include "srvutils.h"

/* Sync protocol:
 * - All integers are 32bit in network order
 *
 * Message:
 ** type   int32_t
 ** length uint32_t
 ** startup | sync | None
 *
 * Startup:
 ** buffer  int32_t
 ** index   uint32_t
 ** filter  bitarray_base_t[FILTER_SIZE]
 *
 * Sync:
 ** digest sha_256_t
 *
 */

typedef struct {
  int32_t type;
  uint32_t length;
} sync_msg_t;

typedef struct {
  int32_t buffer;
  uint32_t index;
  bitarray_base_t filter[FILTER_SIZE];  
} startup_sync_t;

typedef struct {
  sha_256_t digest;
} oper_sync_t;

typedef struct {
  bitindex_t filter_size;
  int32_t num_bufs;
} sync_config_t;

typedef struct {
  uint32_t count;
} aggregate_sync_t;

static void *syncmgr(void *arg);
int min(int x, int y);

int send_startup_sync(peer_t* peer,startup_sync_t* sync);
int send_oper_sync(peer_t* peer, oper_sync_t* sync);
int force_peer_aggregate();
void send_filters(peer_t* peer);

void *recv_syncs(void *arg);
int recv_sync_msg(peer_t* peer);
int recv_startup_sync(peer_t* peer);
int recv_oper_sync(peer_t* peer);

startup_sync_t sston(startup_sync_t ss); /*  Startup sync to network order */
startup_sync_t sstoh(startup_sync_t ss); /*  Startup sync to host order */


#endif
