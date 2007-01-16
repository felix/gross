/*
 * Copyright (c) 2006,2007
 *                    Eino Tuominen <eino@utu.fi>
 *                    Antti Siira <antti@utu.fi>
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
#include "utils.h"

int 
blocker(edict_t *edict)
{
	chkresult_t *result;

	result = (chkresult_t *)Malloc(sizeof(chkresult_t));
	result->suspicious = 0;
	send_result(edict, result);
	
	logstr(GLOG_DEBUG, "blocker returning");
	return 1;
}

void
blocker_init()
{
	thread_pool_t *pool;

	/* initialize the thread pool */
        logstr(GLOG_INFO, "initializing Sophos blocker thread pool");
	pool = create_thread_pool("blocker", &blocker);
        if (pool == NULL)
                daemon_perror("create_thread_pool");

	register_check(pool);
}
