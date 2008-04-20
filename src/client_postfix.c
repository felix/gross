/* $Id$ */

/*
 * Copyright (c) 2006, 2008
 *               Eino Tuominen <eino@utu.fi>
 *               Antti Siira <antti@utu.fi>
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
#include "utils.h"

int
client_postfix(int argc, char **argv)
{
	int fd;
	struct sockaddr_in gserv;
	char mbuf[MAXLINELEN * 4];
	char line[MAXLINELEN];
	size_t linelen;
	int opt = 1;
	int counter = 0;
	int match = 0;
	int cmatch = 0;
	int runs = 1;
	char *sender, *recipient, *caddr, *helo;

#if RANDOM
	srand(time(NULL));
#endif

	if (argc != 9 && argc != 7 && argc != 6) {
		fprintf(stderr,
		    "usage: gclient postfix sender recipient ip_address helo [runs] [host port]\n");
		return 1;
	}

	sender = argv[2];
	recipient = argv[3];
	caddr = argv[4];
	helo = argv[5];

	fd = socket(AF_INET, SOCK_STREAM, 0);

	memset(&gserv, 0, sizeof(gserv));
	gserv.sin_family = AF_INET;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if (argc == 9) {
		inet_pton(AF_INET, argv[7], &gserv.sin_addr);
		gserv.sin_port = htons(atoi(argv[8]));
	} else {
		inet_pton(AF_INET, "127.0.0.1", &gserv.sin_addr);
		gserv.sin_port = htons(GROSSPORT);
	}

	if (connect(fd, (struct sockaddr *)&gserv, sizeof(gserv))) {
		gerror("connect");
		return 2;
	}

	if (argc > 6)
		runs = atoi(argv[6]);

	while (counter < runs) {
		counter++;
		snprintf(mbuf, MAXLINELEN * 4,
#if RANDOM
		    "sender=%d\nrecipient=%d\nclient_address=%d\nhelo_name=%d\n\n",
		    random(), random(), random(), random());
#else
		    "sender=%s\nrecipient=%s\nclient_address=%s\nhelo_name=%s\n\n",
		    sender, recipient, caddr, helo);
#endif /* RANDOM */

		writen(fd, mbuf, strlen(mbuf));

		do {
			linelen = readline(fd, line, MAXLINELEN);
			if (linelen < 0) {
				gerror("readline");
				return 2;
			}

			if (strlen(line) > 0)
				printf("%s\n", line);

			if (line[0] && line[8] == 'u') {
				match++;
				cmatch++;
			}

		} while (strlen(line) > 0);


		if (counter % 10000 == 0) {
			printf("%d, %d, %f\n", counter, cmatch, ((double)match) / 10000);
			fflush(stdout);
			match = 0;
		}
	}
	close(fd);

	return 0;
}
