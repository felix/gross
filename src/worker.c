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
#include "dnsblc.h"
#endif

#include "msgqueue.h"
#include "worker.h"
#include "utils.h"

/* these are implemented in worker_*.c */
int sjsms_connection(edict_t *edict);
int postfix_connection(edict_t *edict);

/*
 * destructor for client_info_t
 */
void
free_client_info(client_info_t *arg)
{
        free(arg->caddr);
	free(arg->ipstr);
	free(arg->message);
        free(arg);
}

char *
ipstr(struct sockaddr_in *saddr)
{	
	char ipstr[INET_ADDRSTRLEN];

	if (inet_ntop(AF_INET, &saddr->sin_addr,
		ipstr, INET_ADDRSTRLEN) == NULL) {
		strncpy(ipstr, "UNKNOWN\0", INET_ADDRSTRLEN);
	}
	return strdup(ipstr);
}

/*
 * The main worker thread for udp protocol. It first initializes
 * worker thread pool. Then, it listens for requests and
 * and feeds them to the thread pool.
 */
static void *
sjsms_server(void *arg)
{
	int grossfd, ret, msglen;
	socklen_t clen;
	client_info_t *client_info;
	char mesg[MAXLINELEN];
	edict_t *edict;
	thread_pool_t *sjsms_pool;

	grossfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (grossfd < 0) {
		/* ERROR */
		perror("socket");
		return NULL;
	}

	ret = bind(grossfd, (struct sockaddr *)&(ctx->config.gross_host),
			sizeof(struct sockaddr_in));
	if (ret < 0) {
		daemon_perror("bind");
	}

	/* initialize the thread pool */
	logstr(GLOG_INFO, "initializing sjsms worker thread pool");
	sjsms_pool = create_thread_pool("sjsms", &sjsms_connection);
	if (sjsms_pool == NULL)
		daemon_perror("create_thread_pool");

	/* server loop */
	for ( ; ; ) {
		/* client_info struct is free()d by the worker thread */
		client_info = Malloc(sizeof(client_info_t));
		client_info->caddr = Malloc(sizeof(struct sockaddr_in));

		clen = sizeof(struct sockaddr_in);
		msglen = recvfrom(grossfd, mesg, MAXLINELEN, 0,
					(struct sockaddr *)client_info->caddr, &clen);

		if (msglen < 0) {
			if (errno == EINTR)
				continue;
			daemon_perror("recvfrom");
			free_client_info(client_info);
			return NULL;
		} else {
			client_info->message = Malloc(msglen);
			client_info->connfd = grossfd;
			client_info->msglen = msglen;
			client_info->ipstr = ipstr(client_info->caddr);

			memcpy(client_info->message, mesg, msglen);

			/* Write the edict */
			edict = edict_get(true);
			edict->job = (void *)client_info;
			submit_job(sjsms_pool, edict);
			edict_unlink(edict);
		}
	}
	/* never reached */
}

/*
 * The main worker thread for tcp_protocol. Listens for connections
 * and starts a new thread to handle each connection.
 */
static void *
postfix_server(void *arg)
{
        int ret;
        int grossfd;
        int opt;
        client_info_t *client_info;
        socklen_t clen;
	thread_pool_t *postfix_pool;
	edict_t *edict;

        /* create socket for incoming requests */
        grossfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (grossfd < 0) {
                /* ERROR */
                perror("socket");
                return NULL;
        }
        opt = 1;
        ret = setsockopt(grossfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (ret < 0) {
                perror("setsockopt (SO_REUSEADDR)");
                return NULL;
        }

        ret = bind(grossfd, (struct sockaddr *)&(ctx->config.gross_host), sizeof(struct sockaddr_in));
        if (ret < 0) {
                daemon_perror("bind");
        }

        ret = listen(grossfd, MAXCONNQ);
        if (ret < 0) {
                perror("listen");
                return NULL;
        }

	/* initialize the thread pool */
	logstr(GLOG_INFO, "initializing postfix thread pool");
	postfix_pool = create_thread_pool("postfix", &postfix_connection);
	if (postfix_pool == NULL)
		daemon_perror("create_thread_pool");

        /* server loop */
        for ( ; ; ) {
                /* client_info struct is free()d by the worker thread */
                client_info = Malloc(sizeof(client_info_t));
                client_info->caddr = Malloc(sizeof(struct sockaddr_in));

                clen = sizeof(struct sockaddr_in);

		logstr(GLOG_INSANE, "waiting for connections");
                client_info->connfd = accept(grossfd, (struct sockaddr *)client_info->caddr, &clen);
                if (client_info->connfd < 0) {
                        if (errno != EINTR) {
                                daemon_perror("accept()");
                        }
                } else {
			logstr(GLOG_INSANE, "new connection");
			/* a client is connected, handle the
			 * connection over to a worker thread
			 */
			client_info->ipstr = ipstr(client_info->caddr);
			/* Write the edict */
			edict = edict_get(true);
			edict->job = (void *)client_info;
			submit_job(postfix_pool, edict);
			edict_unlink(edict);
                }
        }
}

/* 
 * destructor for grey_tuple_t
 */
void
free_request(grey_tuple_t *arg)
{
	free(arg->sender);
	free(arg->recipient);
	free(arg->client_address);
	free(arg);
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
	bool suspicious;
	bool got_response = false;
	struct timespec start, now;
	mseconds_t timeused;
	tmout_action_t *tap;
	int i;
	struct in_addr inaddr;
	unsigned int ip, net, mask;
	char chkipstr[INET_ADDRSTRLEN] = { '\0' };
	const char *ptr;
	bool free_ta = false;

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
			ta->timeout = 5000;             /* 5 seconds */
			ta->action = NULL;
			ta->next = NULL;
		}

		/* Write the edict */
		edict = edict_get(false);
		edict->job = (void *)request->client_address;
		tap = ta;
		while (tap) {
			edict->timelimit += tap->timeout;
			tap = tap->next;
		}

		/* here should be loop over all checks */
		i = 0;
		while (ctx->checklist[i]) {
			submit_job(ctx->checklist[i], edict);
			i++;
		}

		clock_gettime(CLOCK_TYPE, &start);

		while (ta) {
			do {
				clock_gettime(CLOCK_TYPE, &now);
				timeused = ms_diff(&now, &start);
				if (timeused > ta->timeout)
					break;

				ret = get_msg_timed(edict->resultmq, &message, sizeof(message.result), 0, ta->timeout);
				if (ret > 0) {
					/* We've got a response */
					result = (chkresult_t *)message.result;
					suspicious = result->suspicious;
					free(result);
					logstr(GLOG_INSANE, "suspicious = %d", suspicious);
					if (true == suspicious) {
						logstr(GLOG_INFO, "greylist: %s", realtuple);
						acctstr(ACCT_GREY, "%s", realtuple);
						retvalue = STATUS_GREY;
					} else {
						logstr(GLOG_INFO, "trust: %s", realtuple);
						acctstr(ACCT_TRUST, "%s", realtuple);
						retvalue = STATUS_TRUST;
					}
					got_response = true;

				} 

			} while (! got_response);

			if (got_response)
				break;

			if (timeused > ta->timeout) {
				if (ta->action)
					ta->action(ta->arg, timeused);
				if (! ta->next) {
					/* final timeout, we trust */
					retvalue = STATUS_TRUST;
				}
			}

			ta = ta->next;
		}
		edict_unlink(edict);
#endif /* DNSBL */
	}

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
	if (ctx->config.protocols & PROTO_POSTFIX) {
		logstr(GLOG_INFO, "starting postfix policy server");
		Pthread_create(&ctx->process_parts.postfix_server, &postfix_server, NULL);
	} 
	if (ctx->config.protocols & PROTO_SJSMS) {
		logstr(GLOG_INFO, "starting sjsms policy server");
		Pthread_create(&ctx->process_parts.sjsms_server, &sjsms_server, NULL);
	}
}
