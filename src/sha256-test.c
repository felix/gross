/* $Id */

/*
 * Copyright (c) 2006
 *               Antti Siira <antti@utu.fi>
 *               Eino Tuominen <eino@utu.fi>
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
#include "../include/sha256.h"
#include <string.h>

#define MAX_MESSAGE_LEN 1024

int
getline(FILE * from, char *to, int max)
{
	if (fgets(to, max, from) == NULL) {
		return 0;
	} else {
		return (strlen((const char *)to));
	}
}

int
main(int argc, char **argv)
{
	FILE *data;
	sha_256_t digest;
	char message[MAX_MESSAGE_LEN] = { 0x00 };
	char reference_digest[MAX_MESSAGE_LEN] = { 0x00 };
	char digest_hex[MAX_MESSAGE_LEN] = { 0x00 };
	int error_count = 0;

	printf("sha256-test: ");
	if (argc < 2) {
		printf("\nERROR: No test data file given");
		return 1;
	}

	data = fopen(argv[1], "r");
	if (!data) {
		printf("\nERROR: Unable to open %s", argv[1]);
		return 1;
	}

	while (getline(data, message, MAX_MESSAGE_LEN) && getline(data, reference_digest, MAX_MESSAGE_LEN)) {
		message[strlen(message) - 1] = 0;	// remove newline
		reference_digest[strlen(reference_digest) - 1] = 0;	// remove newline

		digest = sha256((sha_byte_t *)message, strlen(message));
		snprintf(digest_hex, MAX_MESSAGE_LEN, "%08x %08x %08x %08x %08x %08x %08x %08x", digest.h0,
		    digest.h1, digest.h2, digest.h3, digest.h4, digest.h5, digest.h6, digest.h7);

		if (strncmp(digest_hex, reference_digest, MAX_MESSAGE_LEN) != 0) {
			if (argc > 2) {	// Detailed error messages requested
				printf("\nERROR: For string %s digest '%s' and reference digest '%s' differ.",
				    message, digest_hex, reference_digest);
			}
			error_count++;
		}
	}

	if (!error_count) {
		printf("OK\n");
	} else {
		printf("Error count: %d\n", error_count);
	}

	return error_count > 0;
}
