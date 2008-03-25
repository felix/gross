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
#include "worker.h"
#include "srvutils.h"
#include "utils.h"

enum parse_status_t { PARSE_OK, PARSE_CLOSED, PARSE_ERROR, PARSE_SYS_ERROR };

/* prototypes of internals */
int postfix_connection(thread_pool_t *, thread_ctx_t *, edict_t *edict);
int parse_postfix(client_info_t *info, grey_tuple_t *grey_tuple);

/*
 * postfix_connection	- the actual server for policy delegation
 */
int
postfix_connection(thread_pool_t *info, thread_ctx_t *thread_ctx, edict_t *edict)
{
	grey_tuple_t *request;
	char response[MAXLINELEN] = { '\0' };
	int ret;
	struct timespec start, end;
	int delay;
	client_info_t *client_info;
	final_status_t status = { '\0' };

	client_info = edict->job;
	assert(client_info);

	logstr(GLOG_DEBUG, "postfix client connected from %s", client_info->ipstr);

	while(1) {
		request = request_new();
		ret = parse_postfix(client_info, request);
		if (ret == PARSE_OK) {
			/* We are go */
			clock_gettime(CLOCK_TYPE, &start);
			ret = test_tuple(&status, request, NULL);

			if (ret < 0) {
				/* error */
				snprintf(response, MAXLINELEN, "action=dunno");
			} else {
				switch (status.status) {
				case STATUS_TRUST:
				case STATUS_MATCH:
					snprintf(response, MAXLINELEN, "action=dunno");
					break;
				case STATUS_BLOCK:
					snprintf(response, MAXLINELEN, "action=reject %s",
						status.reason ? status.reason : "Rejected");
					break;
				case STATUS_GREY:
					snprintf(response, MAXLINELEN, "action=defer_if_permit %s",
						status.reason ? status.reason : "Please try again later");
					break;
				default:
					snprintf(response, MAXLINELEN, "action=dunno");
				}
			}

			/* Make sure it's terminated */
			response[MAXLINELEN-1] = '\0';
			ret = respond(client_info->connfd, response);
			if ( -1 == ret ) {
				logstr(GLOG_ERROR, "respond() failed in handle_connection");
			}

			clock_gettime(CLOCK_TYPE, &end);
			delay = ms_diff(&end, &start);
			logstr(GLOG_DEBUG, "processing delay: %d ms", delay);
			
			switch (status.status) {
			case STATUS_BLOCK:
			  block_delay_update((double)delay);
			  break;
			case STATUS_MATCH:
			  match_delay_update((double)delay);
			  break;
			case STATUS_GREY:
			  greylist_delay_update((double)delay);
			  break;
			case STATUS_TRUST:
			  trust_delay_update((double)delay);
			  break;
			default:
			  /* FIX: count errors */
			  ;
			}

		
			request_unlink(request);
			/* check if the client requested a single query mode */
			if (client_info->single_query)
				break;
		} else if (ret == PARSE_ERROR) {
			logstr(GLOG_ERROR, "couldn't parse request, closing connection");
			request_unlink(request);
			break;
		} else if (ret == PARSE_SYS_ERROR) {
			request_unlink(request);
			break;
		} else if (ret == PARSE_CLOSED) {
			request_unlink(request);
			break;
		}
	}

        close(client_info->connfd);
	free_client_info(client_info);
	logstr(GLOG_DEBUG, "postfix_connection returning");

	if (status.reason)
		Free(status.reason);

	return ret;
}

/*
 * parse_postfix	- build the request tuple (sender, recipient, ipaddr)
 */
int
parse_postfix(client_info_t *client_info, grey_tuple_t *grey_tuple)
{
	char line[MAXLINELEN];
	char *match;
	int input = 0;
	int ret;

	do {
		ret = readline(client_info->connfd, &line, MAXLINELEN);
		if (ret == ERROR) {
			/* error */
			logstr(GLOG_ERROR, "readline returned error");
			return PARSE_SYS_ERROR;
		} else if (ret == EMPTY) {
			/* connection closed */
			logstr(GLOG_DEBUG, "connection closed by client");
			return PARSE_CLOSED;
		} else if (ret == DATA && strlen(line) == 0 && input == 0) {
			logstr(GLOG_DEBUG, "connection close requested by client");
			return PARSE_CLOSED;
		}

		input = 1;

		/* matching switch */
		match = try_match("sender=", line);
		if (match) {
			grey_tuple->sender = match;
			logstr(GLOG_DEBUG, "sender=%s", match);
			continue;
		}
		match = try_match("recipient=", line);
		if (match) {
			grey_tuple->recipient = match;
			logstr(GLOG_DEBUG, "recipient=%s", match);
			continue;
		}
		match = try_match("client_address=", line);
		if (match) {
			grey_tuple->client_address = match;
			logstr(GLOG_DEBUG, "client_address=%s", match);
			continue;
		}
		match = try_match("helo_name=", line);
		if (match) {
			grey_tuple->helo_name = match;
			logstr(GLOG_DEBUG, "helo_name=%s", match);
			continue;
		}
		match = try_match("grossd_mode=", line);
		if (match) {
			client_info->single_query = true;
			logstr(GLOG_DEBUG, "Client requested a single connection mode");
			continue;
		}
		
	} while (strlen(line) > 0);
	
	ret = check_request(grey_tuple);
	if (ret < 0)
		return PARSE_ERROR;
	else
		return PARSE_OK;
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
        if (grossfd < 0) 
		daemon_fatal("postfix_server: socket");
        opt = 1;
        ret = setsockopt(grossfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (ret < 0) 
                daemon_fatal("setsockopt (SO_REUSEADDR)");

        ret = bind(grossfd, (struct sockaddr *)&(ctx->config.gross_host), sizeof(struct sockaddr_in));
        if (ret < 0) 
                daemon_fatal("bind");

        ret = listen(grossfd, MAXCONNQ);
        if (ret < 0)
                daemon_fatal("listen");

        /* initialize the thread pool */
        logstr(GLOG_INFO, "initializing postfix thread pool");
        postfix_pool = create_thread_pool("postfix", &postfix_connection, NULL, NULL);
        if (postfix_pool == NULL)
                daemon_fatal("create_thread_pool");

        /* server loop */
        for ( ; ; ) {
                /* client_info struct is free()d by the worker thread */
                client_info = Malloc(sizeof(client_info_t));
		memset(client_info, 0, sizeof(client_info_t));
                client_info->caddr = Malloc(sizeof(struct sockaddr_in));

                clen = sizeof(struct sockaddr_in);

                logstr(GLOG_INSANE, "waiting for connections");
                client_info->connfd = accept(grossfd, (struct sockaddr *)client_info->caddr, &clen);
                if (client_info->connfd < 0) {
                        if (errno != EINTR)
                                daemon_fatal("accept()");
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

void
postfix_server_init()
{
	logstr(GLOG_INFO, "starting postfix policy server");
	Pthread_create(&ctx->process_parts.postfix_server, &postfix_server, NULL);
}

