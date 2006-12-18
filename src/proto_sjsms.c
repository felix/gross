/*
 * Copyright (c) 2006 Eino Tuominen <eino@utu.fi>
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

#include <stdarg.h>

#include "proto_sjsms.h"

/* prototypes of internals */
int send_sjsms_msg(int fd, struct sockaddr_in *grserv, sjsms_msg_t *message);

int
fold(gray_req_t *req, const char *sender,
        const char *rcpt, const char *caddr)
{
        int sender_len, rcpt_len, caddr_len;

        sender_len = strlen(sender);
        rcpt_len = strlen(rcpt);
        caddr_len = strlen(caddr);

        req->sender = 0;
        memcpy(req->message + req->sender, sender, sender_len);
        *(req->message + req->sender + sender_len) = '\0';

        req->recipient = req->sender + sender_len + 1;
        memcpy(req->message + req->recipient, rcpt , rcpt_len);
        *(req->message + req->recipient + rcpt_len) = '\0';

        req->client_address = req->recipient + rcpt_len + 1;
        memcpy(req->message + req->client_address, caddr , caddr_len);
        *(req->message + req->client_address + caddr_len) = '\0';

        req->msglen = sender_len + 1 + rcpt_len + 1 + caddr_len + 1;

        return 1;
}

int
senderrormsg(int fd, struct sockaddr_in *gserv, const char *fmt, ...)
{
	sjsms_msg_t message;
	va_list	vap;

	va_start(vap, fmt);
	vsnprintf(message.message, MAXLINELEN, fmt, vap);
	va_end(vap);

	message.msglen = MIN(strlen(message.message), MAXLINELEN);
	message.msgtype = LOGMSG;

	return send_sjsms_msg(fd, gserv, &message);
}
	

int
sendquery(int fd, struct sockaddr_in *gserv, gray_req_t *request)
{
	sjsms_msg_t message;
	void *ret;

	message.msgtype = QUERY;
	message.msglen = MIN(request->msglen + 4 * sizeof(int), MAXLINELEN);
	memcpy(&message.message, request, message.msglen);
	return send_sjsms_msg(fd, gserv, &message);
}

int
send_sjsms_msg(int fd, struct sockaddr_in *gserv, sjsms_msg_t *message)
{
	int slen;

	slen = sizeof(struct sockaddr_in);

	return sendto(fd, message, message->msglen + 2 * sizeof(int),
		0, (struct sockaddr *)gserv, slen);
}

gray_tuple_t *
unfold(gray_req_t *request)
{
        gray_tuple_t *tuple;

        tuple = malloc(sizeof(gray_tuple_t));
	if (! tuple)
		return NULL;

        if (request->sender >= MAXLINELEN ||
                        request->recipient >= MAXLINELEN ||
                        request->client_address >= MAXLINELEN)
                return NULL;
        tuple->sender = strdup(request->message + request->sender);
        tuple->recipient = strdup(request->message + request->recipient);
        tuple->client_address = strdup(request->message + request->client_address);

        return tuple;
}

