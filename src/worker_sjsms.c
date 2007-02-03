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
#include "proto_sjsms.h"
#include "srvutils.h"
#include "worker.h"
#include "utils.h"

#define REASONTEMPLATE "%reason%"

int
mappingstr(const char *from, char *to, size_t len)
{
	const char *from_ptr;
	char *to_ptr;

	from_ptr = from;
	to_ptr = to;
	
	while (from_ptr && from_ptr - from < len - 3) {
		switch(*from_ptr) {
		case ' ':
			*to_ptr++ = '$';
			break;
		}
		*to_ptr++ = *from_ptr++;
	}
	if (from_ptr - from > len - 2)
		return -1;
	else 
		return 0;
}

grey_tuple_t *
unfold(grey_req_t *request)
{
        grey_tuple_t *tuple;
        uint16_t sender, recipient, client_address;

	tuple = request_new();

        sender = ntohs(request->sender);
        recipient = ntohs(request->recipient);
        client_address = ntohs(request->client_address);

        if (sender >= MAXLINELEN ||
                        recipient >= MAXLINELEN ||
                        client_address >= MAXLINELEN) {
                errno = ENOMSG;
                return NULL;
        }
        tuple->sender = strdup(request->message + sender);
        tuple->recipient = strdup(request->message + recipient);
        tuple->client_address = strdup(request->message + client_address);

	return tuple;
}

/*
 *timeout action for sending the 1 second "PROGRESS" packet
 */
void calm_client(void *arg, mseconds_t timeused) {
	client_info_t *client_info;
	char response = 'P';
	socklen_t len;

	len = sizeof(struct sockaddr_in);
	client_info = (client_info_t *)arg;
	sendto(client_info->connfd, &response, 1,
		0, (struct sockaddr *)client_info->caddr, len);

	logstr(GLOG_DEBUG, "timeout: used %d ms. PROGRESS sent", timeused);
}

/*
 * sjsms_connection    - the actual greylist server
 */
int
sjsms_connection(thread_ctx_t *thread_ctx, edict_t *edict)
{
	socklen_t len;
	grey_req_t request;
	sjsms_msg_t *msg;
	grey_tuple_t *tuple;
	char response[MAXLINELEN];
	char reason[MAXLINELEN];
	char *blocktemplate;
	char *reasonsubstitute;
	char *rest;
	final_status_t status = { '\0' };
	int ret;
	tmout_action_t ta1, ta2;
	char *str;
	struct timespec start, end;
	int delay;
	client_info_t *client_info;

	client_info = edict->job;
	assert(client_info);
	assert(0 <= client_info->msglen);
	assert(client_info->msglen <= MSGSZ);

	logstr(GLOG_DEBUG, "query from %s", client_info->ipstr);
 	
	/* default response is 'FAIL' */
	strncpy(response, "F", MAXLINELEN);

	/* build the tmout_action_t list */
	ta1.timeout = 1000;             /* 1 second */
	ta1.action = &calm_client;
	ta1.arg = client_info;
	ta1.next = &ta2;

	ta2.timeout = ctx->config.query_timelimit;
	ta2.action = NULL;
	ta2.next = NULL;

	/* clean the input */
	msg = (sjsms_msg_t *)Malloc(client_info->msglen);
	memcpy(msg, client_info->message, client_info->msglen);
	
	sjsms_to_host_order(msg);

	switch (msg->msgtype) {
	case QUERY:
		recvquery(msg, &request);
		clock_gettime(CLOCK_TYPE, &start);

		tuple = unfold(&request);

		/* FIX: shouldn't crash the whole server */
		if (! tuple)
			daemon_perror("unfold");

		/* We are go */
		ret = test_tuple(&status, tuple, &ta1);

		if (ret < 0) {
			snprintf(response, MAXLINELEN, "F");
		} else {
			switch (status.status) {
			case STATUS_MATCH:
				snprintf(response, MAXLINELEN, "M %s", ctx->config.sjsms.responsematch);
				break;
			case STATUS_GREY:
				snprintf(response, MAXLINELEN, "G %s", ctx->config.sjsms.responsegrey);
				break;
			case STATUS_BLOCK:
				mappingstr(status.reason, reason, MAXLINELEN);
				/* ignore the reason if template does not use it */
				blocktemplate = strdup(ctx->config.sjsms.responseblock);
				reasonsubstitute = strstr(blocktemplate, REASONTEMPLATE);
				if (NULL == reasonsubstitute) {
					snprintf(response, MAXLINELEN, "B %s", blocktemplate);
				} else {
					/* null terminate the first part */
					*reasonsubstitute = '\0';
					rest = reasonsubstitute + strlen(REASONTEMPLATE);
					snprintf(response, MAXLINELEN, "B %s%s%s",
						blocktemplate, reason, rest);
				}
				free(blocktemplate);
				break;
			case STATUS_TRUST:
				snprintf(response, MAXLINELEN, "T %s", ctx->config.sjsms.responsetrust);
				break;
			}
		}

		response[MAXLINELEN-1] = '\0';

		len = sizeof(struct sockaddr_in);
		sendto(client_info->connfd, response, strlen(response),
			0, (struct sockaddr *)client_info->caddr, len);
		clock_gettime(CLOCK_TYPE, &end);

		delay = ms_diff(&end, &start);
		logstr(GLOG_DEBUG, "processing delay: %d ms", delay);

		switch (status.status) {
		case STATUS_MATCH:
		  match_delay_update((double)delay);
		  break;
		case STATUS_GREY:
		  greylist_delay_update((double)delay);
		  break;
		case STATUS_TRUST:
		  trust_delay_update((double)delay);
		  break;
		}

		request_unlink(tuple);
		break;
	case LOGMSG:
		str = (char *)Malloc(msg->msglen);
		memcpy(str, msg->message, MIN(msg->msglen, MAXLINELEN));
		str[msg->msglen-1] = '\0';
		logstr(GLOG_ERROR, "Client %s said: %s", client_info->ipstr, str);
		free(str);
		break;
	default:
		logstr(GLOG_ERROR, "Unknown message from client %s",
			client_info->ipstr);
		break;
	}
	free(msg);

	free_client_info(client_info);
	logstr(GLOG_DEBUG, "sjsms_connection returning");

        if (status.reason)
                free(status.reason);

	return 1;
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
		memset(client_info, 0, sizeof(client_info_t));
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

void
sjsms_server_init()
{
	logstr(GLOG_INFO, "starting sjsms policy server");
	Pthread_create(&ctx->process_parts.sjsms_server, &sjsms_server, NULL);
}
