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
#include "srvutils.h"
#include "sha256-test.h"

#define MAX_MESSAGE_LEN 1024

void
verbose_result(bool ok, char *message, char *digest_hex, char *reference_digest)
{
	char tmp[40] = { 0x00 };
	strncpy(tmp, message, 36);

	if (strnlen(message) > 36) {
		strncat(tmp + 36, "...", 3);
	}

	if (ok) {
		printf("\nMessage: '%s'\n\t          digest: '%s'\n\treference digest: '%s'.",
		    tmp, digest_hex, reference_digest);
	} else {
		printf("\nERROR: For string %s digest '%s' and reference digest '%s' differ.\n",
		    tmp, digest_hex, reference_digest);
	}
}

int
main(int argc, char **argv)
{
	sha_256_t digest;
	char message[MAX_MESSAGE_LEN] = { 0x00 };
	char reference_digest[MAX_MESSAGE_LEN] = { 0x00 };
	char digest_hex[MAX_MESSAGE_LEN] = { 0x00 };
	int error_count = 0;
	test_vector *test;
	char *long_message;

	printf("Check: sha256\n");

	/* Known test vectors */
	for (test = test_vectors; test->message && test->reference_digest; test++) {
		strncpy(message, test->message, MAX_MESSAGE_LEN);
		strncpy(reference_digest, test->reference_digest, MAX_MESSAGE_LEN);

		string_sha256_hexdigest(digest_hex, message);
		if (strncmp(digest_hex, reference_digest, MAX_MESSAGE_LEN) != 0) {
			if (argc > 1) {
				verbose_result(FALSE, message, digest_hex, reference_digest);
			}

			error_count++;
		} else {
			if (argc > 1) {
				verbose_result(TRUE, message, digest_hex, reference_digest);
			}
		}
	}

	/* Known long message */
	long_message = Malloc(sizeof(char) * 1000001);
	memset(long_message, 'a', 1000000);
	long_message[1000000] = 0x00;
	strncpy(reference_digest, "cdc76e5c 9914fb92 81a1c7e2 84d73e67 f1809a48 a497200e 046d39cc c7112cd0",
	    MAX_MESSAGE_LEN);

	string_sha256_hexdigest(digest_hex, long_message);

	if (strncmp(digest_hex, reference_digest, MAX_MESSAGE_LEN) != 0) {
		if (argc > 1) {
			verbose_result(FALSE, long_message, digest_hex, reference_digest);
		}

		error_count++;
	} else {
		if (argc > 1) {
			verbose_result(TRUE, long_message, digest_hex, reference_digest);
		}
	}
	Free(long_message);

	return error_count > 0;
}
