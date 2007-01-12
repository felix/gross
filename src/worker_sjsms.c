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
 * handle_connection    - the actual greylist server
 */
int
handle_connection(client_info_t *client_info)
{
	socklen_t len;
	grey_req_t request;
	sjsms_msg_t *msg;
	grey_tuple_t *tuple;
	char response[MAXLINELEN];
	int status;
	tmout_action_t ta1, ta2;
	char *str;
	struct timespec start, end;
	int delay;

	assert(client_info);
	assert(0 <= client_info->msglen);
	assert(client_info->msglen <= MSGSZ);
 	
	/* default response is 'FAIL' */
	strncpy(response, "F", MAXLINELEN);

	/* build the tmout_action_t list */
	ta1.timeout = 1000;             /* 1 second */
	ta1.action = &calm_client;
	ta1.arg = client_info;
	ta1.next = &ta2;

	ta2.timeout = 5000;		/* 5 seconds */
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
		status = test_tuple(tuple, &ta1);

		switch (status) {
			case STATUS_MATCH:
				snprintf(response, MAXLINELEN, "M %s", ctx->config.sjsms.responsematch);
				break;
			case STATUS_GREY:
				snprintf(response, MAXLINELEN, "G %s", ctx->config.sjsms.responsegrey);
				break;
			case STATUS_TRUST:
				snprintf(response, MAXLINELEN, "T %s", ctx->config.sjsms.responsetrust);
				break;
		}

		response[MAXLINELEN-1] = '\0';

		len = sizeof(struct sockaddr_in);
		sendto(client_info->connfd, response, strlen(response),
			0, (struct sockaddr *)client_info->caddr, len);
		clock_gettime(CLOCK_TYPE, &end);

		delay = ms_diff(&end, &start);
		logstr(GLOG_INFO, "processing delay: %d ms", delay);

		switch (status) {
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

		free_request(tuple);
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

	return 1;
}
