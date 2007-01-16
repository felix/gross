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
int parse_postfix(client_info_t *info, grey_tuple_t *grey_tuple);
char *try_match(char *matcher, char *matchee);

/*
 * postfix_connection	- the actual server for policy delegation
 */
int
postfix_connection(edict_t *edict)
{
	grey_tuple_t *request;
	char *response;
	int ret;
	int status;
	struct timespec start, end;
	int delay;
	client_info_t *client_info;

	client_info = edict->job;
	assert(client_info);

	logstr(GLOG_INFO, "postfix client connected from %s", client_info->ipstr);

	while(1) {
		request = Malloc(sizeof(grey_tuple_t));
		ret = parse_postfix(client_info, request);
		if (ret == PARSE_OK) {
			/* We are go */
			clock_gettime(CLOCK_TYPE, &start);
			status = test_tuple(request, NULL);

			switch (status) {
				case STATUS_MATCH:
					response = "action=dunno";
					break;
				case STATUS_GREY:
					response = "action=defer_if_permit Greylisted";
					break;
				case STATUS_TRUST:
					response = "action=dunno";
					break;
			}

			ret = respond(client_info->connfd, response);
			if ( -1 == ret ) {
				logstr(GLOG_ERROR, "respond() failed in handle_connection");
				perror("handle_connection");
			}

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

		
			free_request(request);
		} else if (ret == PARSE_ERROR) {
			logstr(GLOG_ERROR, "couldn't parse request, closing connection");
			free_request(request);
			break;
		} else if (ret == PARSE_SYS_ERROR) {
			perror("parse_postfix");
			free_request(request);
			break;
		} else if (ret == PARSE_CLOSED) {
			free_request(request);
			break;
		}
	}

        close(client_info->connfd);
	free_client_info(client_info);
	logstr(GLOG_DEBUG, "postfix_connection returning");

	return ret;
}

/*
 * parse_postfix	- build the request tuple (sender, recipient, ipaddr)
 */
int
parse_postfix(client_info_t *client_info, grey_tuple_t *grey_tuple)
{
	char line[MAXLINELEN];
/* 	ssize_t linelen; */
	char *match;
	int input = 0;
	int ret;

	/* zero out the struct - see free_request() */
	memset(grey_tuple, 0, sizeof(grey_tuple_t));

	do {
		ret = readline(client_info->connfd, &line, MAXLINELEN);
		if (ret == ERROR) {
			/* error */
			logstr(GLOG_ERROR, "readline returned error");
			return PARSE_SYS_ERROR;
		} else if (ret == EMPTY) {
			/* connection closed */
			logstr(GLOG_INFO, "connection closed by client");
			return PARSE_CLOSED;
		} else if (ret == DATA && strlen(line) == 0 && input == 0) {
			logstr(GLOG_INFO, "connection close requested by client");
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
		
	} while (strlen(line) > 0);
	
	if ( grey_tuple->sender &&
             grey_tuple->recipient &&
	     grey_tuple->client_address ) {
		return PARSE_OK;
	} else {
		return PARSE_ERROR;
	}
}

char *
try_match(char *matcher, char *matchee) 
{
	if (strncmp(matcher, matchee, strlen(matcher)) == 0)
		/* found a match, return part after the match */
		return strdup(matchee + strlen(matcher));
	else
		return NULL;
}
