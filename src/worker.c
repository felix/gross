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
#include "syncmgr.h"

#ifdef DNSBL
#include "check_dnsbl.h"
#endif

#include "msgqueue.h"
#include "worker.h"
#include "utils.h"

/* these are implemented in worker_*.c */
void postfix_server_init();
void sjsms_server_init();

/*
 * destructor for client_info_t
 */
void
free_client_info(client_info_t *arg)
{
	if (arg->caddr)
		free(arg->caddr);
	if (arg->ipstr)
		free(arg->ipstr);
	if (arg->message)
		free(arg->message);
        free(arg);
}

/*
 * destructor for grey_tuple_t
 */
void
request_unlink(grey_tuple_t *request)
{
        int ret;

        ret = pthread_mutex_lock(&request->reference.mx);
        assert(0 == ret);
        assert(request->reference.count > 0);

        if (--request->reference.count == 0) {
                /* last reference */
                if (request->sender)
                        free(request->sender);
                if (request->recipient)
                        free(request->recipient);
                if (request->client_address)
                        free(request->client_address);
                pthread_mutex_unlock(&request->reference.mx);
                free(request);
        } else {
                pthread_mutex_unlock(&request->reference.mx);
        }
}

grey_tuple_t *
request_new()
{
        grey_tuple_t *request;

        request = Malloc(sizeof(grey_tuple_t));
        bzero(request, sizeof(grey_tuple_t));

        pthread_mutex_init(&request->reference.mx, NULL);
        request->reference.count = 1;

        return request;
}

int
test_tuple(grey_tuple_t *request, tmout_action_t *ta) {
	char maskedtuple[MSGSZ];
	char realtuple[MSGSZ];
	sha_256_t digest;
	update_message_t update;
	int ret;
	int retvalue = STATUS_UNKNOWN;
	oper_sync_t os;
	edict_t *edict;
	poolresult_message_t message;
	chkresult_t *result;
	bool suspicious = false;
	bool got_response = false;
	struct timespec start, now;
	mseconds_t timeused;
	tmout_action_t *tap;
	int i;
	int checks_running;
	struct in_addr inaddr;
	unsigned int ip, net, mask;
	char chkipstr[INET_ADDRSTRLEN] = { '\0' };
	const char *ptr;
	bool free_ta = false;
	grey_tuple_t *requestcopy;

	/* record the processing start time */
	clock_gettime(CLOCK_TYPE, &start);

	/*
	 * apply checkmask to the ip 
	 */ 
	if (strlen(request->client_address) > INET_ADDRSTRLEN) {
		logstr(GLOG_NOTICE, "invalid ipaddress: %s", request->client_address);
		return STATUS_FAIL;
	}

	ret = inet_pton(AF_INET, request->client_address, &inaddr);
	switch(ret) {
	case -1:
		logstr(GLOG_ERROR, "test_tuple: inet_pton: %s", strerror(errno));
		return STATUS_FAIL;
		break;
	case 0:
		logstr(GLOG_ERROR, "not a valid ip address: %s", request->client_address);
		return STATUS_FAIL;
		break;
	}

	/* case default */
	ip = inaddr.s_addr;

	/* this is 0xffffffff ^ (2 ** (32 - mask - 1) - 1) */
	mask = 0xffffffff ^ ((1 << (32 - ctx->config.grey_mask)) - 1); 

	/* ip is in network order */
	net = ip & htonl(mask);

	ptr = inet_ntop(AF_INET, &net, chkipstr, INET_ADDRSTRLEN);
	if (! ptr) {
		logstr(GLOG_ERROR, "test_tuple: inet_ntop: %s", strerror(errno));
		return STATUS_FAIL;
	}
	
	/* greylist */
	snprintf(maskedtuple, MSGSZ, "%s %s %s",
			chkipstr,
			request->sender,
			request->recipient);
	digest = sha256_string(maskedtuple);

	/* for logging */
	snprintf(realtuple, MSGSZ, "%s %s %s",
			request->client_address,
			request->sender,
			request->recipient);

	logstr(GLOG_INSANE, "checking ip=%s mask=0x%x, net=%s",
		request->client_address, mask, chkipstr);

	/* check status */
	if ( is_in_ring_queue(ctx->filter, digest) ) {
		logstr(GLOG_INFO, "match: %s", realtuple);
		acctstr(ACCT_MATCH, "%s", realtuple);
		retvalue = STATUS_MATCH;
	} else {
#ifndef DNSBL
		logstr(GLOG_INFO, "greylist: %s", realtuple);
		acctstr(ACCT_GREY, "%s", realtuple);
		retvalue = STATUS_GREY;
#else
		/* build default entry, if timeout not given */
		if (! ta) {
			free_ta = true;
			ta = Malloc(sizeof(tmout_action_t));
			ta->timeout = ctx->config.query_timelimit;
			ta->action = NULL;
			ta->next = NULL;
		}

		/* Write the edict */
		edict = edict_get(false);
		edict->job = (void *)request;
		tap = ta;
		while (tap) {
			edict->timelimit += tap->timeout;
			tap = tap->next;
		}

		/* here should be loop over all checks */
		i = 0;
		while (ctx->checklist[i]) {
			request->reference.count++;	
			submit_job(ctx->checklist[i]->pool, edict);
			i++;
		}
		checks_running = i;

		while (ta && suspicious == false && checks_running > 0) {
			clock_gettime(CLOCK_TYPE, &now);
			timeused = ms_diff(&now, &start);
			/* make sure timeleft != 0 as it would cause get_msg_timed to block */
			if (timeused < ta->timeout) {
				ret = get_msg_timed(edict->resultmq, &message,
					sizeof(message.result), 0, ta->timeout - timeused);
				if (ret > 0) {
					/* We've got a response */
					checks_running--;
					result = (chkresult_t *)message.result;
					suspicious = result->suspicious;
					free(result);
					logstr(GLOG_INSANE, "Received a check result, suspicious = %d",
						suspicious);
				} 
			} else if (ta->action) {
				ta->action(ta->arg, timeused);
				ta = ta->next;
			} else {
				ta = ta->next;
			}
		}
		if (true == suspicious) {
			logstr(GLOG_INFO, "greylist: %s", realtuple);
			acctstr(ACCT_GREY, "%s", realtuple);
			retvalue = STATUS_GREY;
		} else {
			logstr(GLOG_INFO, "trust: %s", realtuple);
			acctstr(ACCT_TRUST, "%s", realtuple);
			retvalue = STATUS_TRUST;
		}
		edict_unlink(edict);
#endif /* DNSBL */
	}

	/* we cannot free(ta) if we got it as parameter */
	if (free_ta) free(ta);

	if (((retvalue == STATUS_GREY) || (retvalue == STATUS_MATCH)) 
		|| (ctx->config.flags & FLG_UPDATE_ALWAYS)) {
		/* update the filter */
		update.mtype = UPDATE;
		memcpy(update.mtext, &digest, sizeof(sha_256_t));
		ret = put_msg(ctx->update_q, &update, sizeof(sha_256_t), 0);
		if (ret < 0) {
			perror("update put_msg");
		}	

		/* update peer */
		if ( connected( &(ctx->config.peer) ) ) {
			os.digest = digest;
			logstr(GLOG_INSANE, "Sending oper sync");
			send_oper_sync( &(ctx->config.peer) , &os);
		}
	}

	/* check if DRYRUN is enabled */
	if (ctx->config.flags & FLG_DRYRUN)
		retvalue = STATUS_TRUST;

	/* Update counters */
	switch (retvalue) {
	case STATUS_MATCH:
	  INCF_STATS(match);
	  INCF_STATS(all_match);
	  break;
	case STATUS_GREY:
	  INCF_STATS(greylist);
	  INCF_STATS(all_greylist);
	  break;
	case STATUS_TRUST:
	  INCF_STATS(trust);
	  INCF_STATS(all_trust);
	  break;
	}

	return retvalue;
}


void
worker_init()
{
	if (ctx->config.protocols == 0)
		logstr(GLOG_NOTICE, "No protocols configured");
	if (ctx->config.protocols & PROTO_POSTFIX)
		postfix_server_init();
	if (ctx->config.protocols & PROTO_SJSMS)
		sjsms_server_init();
#ifdef MILTER
	if (ctx->config.protocols & PROTO_MILTER)
		milter_init();
#endif
}
