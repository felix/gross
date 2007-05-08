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

#include <string.h>

#include "common.h"
#include "proto_sjsms.h"

#define MTASTRLEN 252
#define SBUFLEN 256

/*
 * the old protocol, we use these only if server retuns
 * 'short' answers
 */
#define STATUS_TRUST       "$Y"
#define STATUS_MATCH       "$Y"
#define STATUS_GREYLIST    "$X4.4.3|$NPlease$ try$ again$ later"
#define STATUS_BLOCK	   "$N"

#define MAP_SUCCESS     -1
#define MAP_FAIL        0

#define MAP_SEPARATOR ','

#define GROSSCHECK_ERROR { 						\
	senderrormsg(fd, gserv, "ERROR: request was: %s", requestcopy); \
	Free(requestcopy); 						\
	close(fd);							\
	return MAP_FAIL; 						\
}

/* #define ARGDEBUG */

int
grosscheck(char *arg, long *arglen, char *res, long *reslen)
{
	/* arg and res are maximum of MTASTRLEN bytes long. */
	char buffer[SBUFLEN] = { 0x00 };
	char sender[SBUFLEN] = { 0x00 };
	char recipient[SBUFLEN] = { 0x00 };
	char caddr[SBUFLEN] = { 0x00 };
	char helo[SBUFLEN] = { 0x00 };
	char recbuf[MAXLINELEN] = { 0x00 };
	char *requestcopy = NULL; /* null terminated copy of the request */
	int n = -1;
	int ret = -1;
	int numservers = 0;
	char *rstr = 0x00;
	char *begin;
	char *end;
	grey_req_t request;
	bool success = false;
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

	if (fd < 0)
		return MAP_FAIL;

	/* initialize */
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
		fprintf(foo, "query: %s\n", requestcopy);
	}
#endif

#define GETNEXT	{				\
	begin = end + 1;			\
	end = strchr(begin, MAP_SEPARATOR); 	\
	if ( NULL == end) GROSSCHECK_ERROR; 	\
	*end = '\0'; 				\
}

	/* primary server ip */
	end = buffer - 1; /* an ugly kludge, I know */
	GETNEXT;
        ret = inet_pton(AF_INET, begin, &gserv1.sin_addr);
	if ( ret < 1 ) GROSSCHECK_ERROR;

	/* secondary server ip */
	GETNEXT;
	ret = inet_pton(AF_INET, begin, &gserv2.sin_addr);
	if ( ret < 1 ) 
		numservers = 1;
	else
		numservers = 2;
	assert((numservers == 1) || (numservers == 2));

	GETNEXT;
	gserv1.sin_port = gserv2.sin_port = htons(atoi(begin));

	GETNEXT;
	strncpy(caddr, begin, SBUFLEN-1);

	GETNEXT;
	strncpy(recipient, begin, SBUFLEN-1);

	/* sender */
	GETNEXT;
	/* no empty sender */
	if ( *begin == '\0') {
		strncpy(sender, "<>", SBUFLEN-1);
	} else {
		strncpy(sender, begin, SBUFLEN-1);
	}

	/* helo */
	begin = end + 1;
	end = strchr(begin, MAP_SEPARATOR);
	/* no empty helo string */
	if ( *begin == '\0' ) {
		strncpy(helo, "NO-HELO", SBUFLEN-1);
	} else {
		strncpy(helo, begin, SBUFLEN-1);
	}
	
	/* end of arguments */
	if ( NULL != end) GROSSCHECK_ERROR; 

	gserv = &gserv1;

	/* Make sure they are null terminated */
	sender[SBUFLEN-1] = '\0';
	recipient[SBUFLEN-1] = '\0';
	caddr[SBUFLEN-1] = '\0';
	helo[SBUFLEN-1] = '\0';
	fold(&request, sender, recipient, caddr, helo);

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
			if ('\0' == recbuf[1])	
				rstr = STATUS_GREYLIST;
			else
				rstr = &recbuf[2];
			success = true;
			break;
		case 'T':
			if ('\0' == recbuf[1])	
				rstr = STATUS_TRUST;
			else
				rstr = &recbuf[2];
			success = true;
			break;
		case 'M':
			if ('\0' == recbuf[1])	
				rstr = STATUS_MATCH;
			else
				rstr = &recbuf[2];
			success = true;
			break;
		case 'B':
			if ('\0' == recbuf[1])
				rstr = STATUS_BLOCK;
			else
				rstr = &recbuf[2];
			success = true;
			break;
		default:
			success = false;
			break;
	}

	if (success) {
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

	}
	Free(requestcopy);
	close(fd);
	return success ? MAP_SUCCESS : MAP_FAIL;
}

#ifdef GROSSC_MAIN
int
main(int argc, char **argv)
{
	char bar[256];
	long foolen, barlen;
	char *arg = "127.0.0.1,127.0.0.1,1111,127.0.0.2,foo@foo,bar@bar";
	foolen = strlen(arg);

	assert(foolen < 252);

	if (grosscheck(arg, &foolen, bar, &barlen) ==  MAP_SUCCESS) {
		printf("%ld: %s\n", barlen, bar);
	} else {
		printf("Unknown or No reponse \n");
	}
	
	return 0;
}
#endif
