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

#include "common.h"
#include "proto_sjsms.h"

int
client_sjsms(int argc, char **argv) 
{
	int fd;
	struct sockaddr_in gserv;
	char recbuf[MAXLINELEN + 1];
	int runs = 1;
	int counter = 0, n;
	const char *request;
	char *sender, *recipient, *caddr;
	
	if (argc != 8 && argc != 6 && argc != 5) {
		fprintf(stderr, "usage: gclient sjsms sender recipient ip_address [runs] [host port]\n");
		return 1;
	}

	sender = argv[2];
	recipient = argv[3];
	caddr = argv[4];

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	memset(&gserv, 0, sizeof(gserv));
	gserv.sin_family = AF_INET;

	if (argc == 8) {
		inet_pton(AF_INET, argv[6], &gserv.sin_addr);
		gserv.sin_port = htons(atoi(argv[7]));
	} else {
		inet_pton(AF_INET, "127.0.0.1", &gserv.sin_addr);
		gserv.sin_port = htons(GROSSPORT);
	}

	if (argc > 5)
		runs = atoi(argv[5]);

	while (counter < runs) {
		counter++;

#ifdef LOGDEBUG
		senderrormsg(fd, &gserv, "yhteyskokeilu");
#endif

		request = buildquerystr(sender, recipient, caddr, NULL);

		sendquerystr(fd, &gserv, request);
	  
		do {
			n = recvfrom(fd, recbuf, MAXLINELEN, 0, NULL, NULL);
			recbuf[n] = '\0';
			printf("got: %s\n", recbuf);
		} while (*recbuf == 'P');

		printf("%s\n", recbuf);
	}

	return 0;
}
