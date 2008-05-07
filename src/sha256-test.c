/* $Id$ */

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
#include <string.h>
#include "sha256.h"

#define MAX_MESSAGE_LEN 1024

typedef struct {
  char *message;
  char *reference_digest;
} test_vector;

test_vector test_vectors[] = { {"The quick brown fox jumps over the lazy dog","d7a8fbb3 07d78094 69ca9abc b0082e4f 8d5651e4 6d3cdb76 2d02d0bf 37c9e592"},
			       {"The quick brown fox jumps over the lazy cog","e4c4d8f3 bf76b692 de791a17 3e053211 50f7a345 b46484fe 427f6acc 7ecc81be"},
			       {"","e3b0c442 98fc1c14 9afbf4c8 996fb924 27ae41e4 649b934c a495991b 7852b855"},
			       {NULL,NULL} };

int
main(int argc, char **argv)
{
	sha_256_t digest;
	char message[MAX_MESSAGE_LEN] = { 0x00 };
	char reference_digest[MAX_MESSAGE_LEN] = { 0x00 };
	char digest_hex[MAX_MESSAGE_LEN] = { 0x00 };
	int error_count = 0;
	test_vector *test;

	printf("Check: sha256\n");
	
	for (test=test_vectors ; test->message && test->reference_digest ; test++) {
		strncpy(message, test->message, MAX_MESSAGE_LEN);
		strncpy(reference_digest, test->reference_digest, MAX_MESSAGE_LEN);

		printf("  Checking '%s'...  ", message);
		fflush(stdout);

		digest = sha256((sha_byte_t *)message, strlen(message));
		snprintf(digest_hex, MAX_MESSAGE_LEN, "%08x %08x %08x %08x %08x %08x %08x %08x", digest.h0,
		    digest.h1, digest.h2, digest.h3, digest.h4, digest.h5, digest.h6, digest.h7);

		if (strncmp(digest_hex, reference_digest, MAX_MESSAGE_LEN) != 0) {
			if (argc>2) {
				printf("\nERROR: For string %s digest '%s' and reference digest '%s' differ.\n",
				       message, digest_hex, reference_digest);
			} else {
				printf("Fail.\n");
			}

			error_count++;
		} else {
			if (argc>2) { 
				printf("\nMessage: '%s'\n\t          digest: '%s'\n\treference digest: '%s'.",
				       message, digest_hex, reference_digest);
			} else {
				printf("Done.\n");
			}
		}
	}

	return error_count > 0;
}
