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

#define MTASTRLEN 252
#define SBUFLEN 256

#define MAP_TRUST       "$Y"
#define MAP_MATCH       "$Y"
#define MAP_GREYLIST    "$X4.4.3|$NPlease$ try$ again$ later"
#define MAP_UNKNOWN     "$Y"    /* accept if server not available */
#define MAP_ERROR       "$Y" 

#define MAP_SUCCESS     -1
#define MAP_FAIL        0

#define MAP_SEPARATOR ","

#define GROSSCHECK_ERROR() \
                   { strncpy(res, MAP_ERROR, MTASTRLEN); \
                     *reslen = strlen(res); \
                     if (*reslen > MTASTRLEN) { \
                         *reslen = MTASTRLEN; \
                     } \
	             senderrormsg(fd, gserv, "ERROR: request was: %s", requestcopy); \
		     free(requestcopy); \
                     return MAP_SUCCESS; }

/* #define ARGDEBUG */

int
grosscheck(char *arg, long *arglen, char *res, long *reslen)
{
	/* arg and res are maximum of MTASTRLEN bytes long. */
	char buffer[SBUFLEN] = { 0x00 };
	char sender[SBUFLEN] = { 0x00 };
	char recipient[SBUFLEN] = { 0x00 };
	char caddr[SBUFLEN] = { 0x00 };
	char recbuf[MAXLINELEN] = { 0x00 };
	char *requestcopy = NULL; /* null terminated copy of the request */
	int n = -1;
	int ret = -1;
	int numservers = 0;
	char *token = 0x00;
	char *rstr = 0x00;
	grey_req_t request;
#ifdef ARGDEBUG
	FILE *foo;
#endif
	int fd;
        struct sockaddr_in gserv1, gserv2, *gserv;
	struct timeval tv;
	fd_set readers;	

	assert(arglen);
	assert(*arglen >= 0);
	assert(arg);
	assert(res);
	assert(reslen);

	memset(&request, 0, sizeof(request));
        fd = socket(AF_INET, SOCK_DGRAM, 0);

	if (fd < 0) {
	  strncpy(res, MAP_UNKNOWN, MTASTRLEN);
	  *reslen = strlen(res);
	  if (*reslen > MTASTRLEN) {
	    *reslen = MTASTRLEN;
	  }

	  return MAP_SUCCESS;
	}

        memset(&gserv1, 0, sizeof(struct sockaddr_in));
        gserv1.sin_family = AF_INET;
        memset(&gserv2, 0, sizeof(struct sockaddr_in));
        gserv2.sin_family = AF_INET;

	/* arg should contain a string <ip>,<sender>,<recipient> */
	strncpy(buffer, arg, *arglen);
	buffer[*arglen] = '\0';
	requestcopy = strdup(buffer);

#ifdef ARGDEBUG
	foo = fopen("/tmp/argout", "a");
	if (foo) {
		fprintf(foo, "query: %s\n", buffer);
	}
#endif

	/* primary server ip */
	token = strtok(buffer, MAP_SEPARATOR);
	if ( NULL == token ) GROSSCHECK_ERROR();
        ret = inet_pton(AF_INET, token, &gserv1.sin_addr);
	if ( ret < 1 ) GROSSCHECK_ERROR();

	/* secondary server ip */
	token = strtok(NULL, MAP_SEPARATOR);
	if ( NULL == token ) GROSSCHECK_ERROR();
	ret = inet_pton(AF_INET, token, &gserv2.sin_addr);
	if ( ret < 1 ) 
		numservers = 1;
	else
		numservers = 2;
	assert((numservers == 1) || (numservers == 2));

	token = strtok(NULL, MAP_SEPARATOR);
	if ( NULL == token ) GROSSCHECK_ERROR();
	gserv1.sin_port = gserv2.sin_port = htons(atoi(token));

	token = strtok(NULL, MAP_SEPARATOR);
	if ( NULL == token ) GROSSCHECK_ERROR();
	strncpy(caddr, token, SBUFLEN-1);

	token = strtok(NULL, MAP_SEPARATOR);
	if ( NULL == token ) GROSSCHECK_ERROR();
	strncpy(recipient, token, SBUFLEN-1);

	token = strtok(NULL, MAP_SEPARATOR);
	if ( NULL == token ) {
		strncpy(sender, "<>", SBUFLEN-1);
		sender[SBUFLEN-1] = '\0';
	} else {
		strncpy(sender, token, SBUFLEN-1);
	}

	gserv = &gserv1;

	fold(&request, sender, recipient, caddr);

QUERY:
	sendquery(fd, gserv, &request);

	/* initial timeout value */
	tv.tv_sec = 2;	/* 1 second of server timeout + some extra */
	tv.tv_usec = 0;

	do {
		FD_ZERO(&readers);
		FD_SET(fd, &readers);
		memset(recbuf, 0, MAXLINELEN);
		select(fd + 1, &readers, NULL, NULL, &tv);
		if (FD_ISSET(fd, &readers)) {
			n = recvfrom(fd, recbuf, MAXLINELEN, 0, NULL, NULL);
			if (n < 0)
				break;
		} else {
			/* error, try another server if configured */
			numservers--;
			if (numservers == 0) {
				break;
			} else  {
				gserv = &gserv2;
				goto QUERY;
			}
		}
		/* update timeout value in case we got 'P' */
		if (recbuf[0] == 'P')
			tv.tv_sec = 10;
	} while (recbuf[0] == 'P');

	switch (recbuf[0]) {
		case 'G':
			rstr = MAP_GREYLIST;
			break;
		case 'T':
			rstr = MAP_TRUST;
			break;
		case 'M':
			rstr = MAP_MATCH;
			break;
		default:
			rstr = MAP_UNKNOWN;
			break;
	}

	strncpy(res, rstr, MTASTRLEN);
	*reslen = strlen(rstr);
	if (*reslen > MTASTRLEN)
		*reslen = MTASTRLEN;

#ifdef ARGDEBUG
        if (foo) {
                fprintf(foo, "res: %s\n", res);
		fclose(foo);
        }
#endif

	close(fd);

	free(requestcopy);

	return MAP_SUCCESS;
}

#ifdef GROSSC_MAIN
int
main(int argc, char **argv)
{
	char bar[256];
	long foolen, barlen;
	char *arg = "130.232.3.69,127.0.0.1,1111,127.0.0.2,foo@foo,bar@bar";
	foolen = strlen(arg);

	assert(foolen < 252);

	if (grosscheck(arg, &foolen, bar, &barlen) ==  MAP_SUCCESS) {
		printf("%ld: %s\n", barlen, bar);
	} else {
		printf("error\n");
	}
	
	return 0;
}
#endif
