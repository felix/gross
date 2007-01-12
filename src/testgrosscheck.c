/*
 * Copyright (c) 2007
 *                    Eino Tuominen <eino@utu.fi>
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

#include <stdio.h>
#include <dlfcn.h>
#include <link.h>
#include <stdlib.h>
#include <strings.h>

#define MAP_SUCCESS     -1

int
main(int argc, char **argv)
{
	char res[256];
	void *gc;

	long arglen, reslen;
	int (*grosscheck)(char *arg, long *arglen, char *res, long *reslen);
	char *error;

	char *arg = "127.0.0.1,127.0.0.1,1111,127.0.0.2,foo@foo,bar@bar\0";
	arglen = strlen(arg);

	gc = dlopen("grosscheck.so", RTLD_LAZY);
	if (! gc) {
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}

	grosscheck = dlsym(gc, "grosscheck");
	error = dlerror();
	if (NULL != error) {
		fprintf(stderr, "%s\n", error);
		exit(1);
	}
	
	if ((*grosscheck)(arg, &arglen, res, &reslen) ==  MAP_SUCCESS) {
		printf("%ld: %s\n", reslen, res);
	} else {
		printf("error\n");
	}
	
	return 0;
}
