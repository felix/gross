/* $Id$ */

/*
 * Copyright (c) 2006, 2007
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

#include "common.h"
#include "srvutils.h"
#include "bloom.h"

#define PRINTSTATUS do { \
	if (error_count > tmperr) \
		printf("  Failed\n"); \
	else \
		printf("  OK.\n"); \
	} while (0)
int
main(int argc, char *argv[])
{
	int i, j, k;
	double c = 0.001;
	int error_count = 0;
	int tmperr = 0;
	char test[512] = { 0x00 };
	char buf[MAXLINELEN];
	gross_ctx_t myctx = { 0x00 };

	bloom_filter_t *bf;
	bloom_filter_t *bf2;

	bloom_filter_group_t *bfg;

	bloom_ring_queue_t *brq;

	ctx = &myctx;
        memset(ctx, 0, sizeof(gross_ctx_t));
	
	printf("Check: bloom\n");

	printf("  Checking optimal size calculations...");
	fflush(stdout);
	tmperr = error_count;
	if (optimal_size(1000, c) != 10) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 1000\n");
	}
	if (optimal_size(2000, c) != 11) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 2000\n");
	}
	if (optimal_size(3000, c) != 12) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 3000\n");
	}
	if (optimal_size(4000, c) != 12) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 4000\n");
	}
	if (optimal_size(5000, c) != 13) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 5000\n");
	}
	if (optimal_size(8000, c) != 13) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 8000\n");
	}
	if (optimal_size(9000, c) != 14) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 9000\n");
	}
	if (optimal_size(16000, c) != 14) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 16000\n");
	}
	if (optimal_size(17000, c) != 15) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 17000\n");
	}
	if (optimal_size(32000, c) != 15) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 32000\n");
	}
	if (optimal_size(33000, c) != 16) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 33000\n");
	}
	if (optimal_size(65000, c) != 16) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 65000\n");
	}
	if (optimal_size(66000, c) != 17) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 66000\n");
	}
	if (optimal_size(131000, c) != 17) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 131000\n");
	}
	if (optimal_size(132000, c) != 18) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 132000\n");
	}
	if (optimal_size(262000, c) != 18) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 262000\n");
	}
	if (optimal_size(263000, c) != 19) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 263000\n");
	}
	if (optimal_size(524000, c) != 19) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 524000\n");
	}
	if (optimal_size(525000, c) != 20) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 525000\n");
	}
	if (optimal_size(1048000, c) != 20) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 1048000\n");
	}
	if (optimal_size(1049000, c) != 21) {
		error_count++;
		if (argc > 2)
			printf("  Error: size 1049000\n");
	}
	PRINTSTATUS;
	
	printf("  Testing with a 7-bit filter...");
	fflush(stdout);
	tmperr = error_count;
	if (argc > 1 && strcmp(argv[1], "visualize") == 0) {
		printf("\n");
		/* 7-bit filter */
		bf = create_bloom_filter(7);
		for (i = 0; i < 83; i++) {
			sprintf(test, "%d", i);
			insert_digest(bf, sha256_string(test));
			debug_print_filter(bf, TRUE);
		}
		release_bloom_filter(bf);
	}

	/* Test insertion with a 7-bit filter */
	bf = create_bloom_filter(7);
	for (i = 0; i < 16; i++) {
		sprintf(test, "%d", i);
		insert_digest(bf, sha256_string(test));
		if (!is_in_array(bf, sha256_string(test))) {
			error_count++;
			if (argc > 2)
				printf("\nError: %s not in array", test);
		}
	}

	/* Test false positives */
	for (i = 17; i < 32; i++) {
		sprintf(test, "%d", i);
		if (is_in_array(bf, sha256_string(test))) {
			error_count++;
			if (argc > 2)
				printf("\nError: %s is in array", test);
		}
	}
	release_bloom_filter(bf);
	PRINTSTATUS;

	printf("  Testing merging of filters...");
	fflush(stdout);
	tmperr = error_count;
	/* Test filter merge */

	bf = create_bloom_filter(7);
	bf2 = create_bloom_filter(7);

	if (is_in_array(bf, sha256_string("omena"))) {
		error_count++;
		if (argc > 2)
			printf("\nError: 'omena' in array bf");
	}			/* initially empty */
	insert_digest(bf, sha256_string("omena"));

	if (argc > 1 && strcmp(argv[1], "visualize") == 0) {
		printf("\nbf after 'omena': ");
		debug_print_filter(bf, FALSE);
	}

	if (is_in_array(bf2, sha256_string("omena"))) {
		error_count++;
		if (argc > 2)
			printf("\nError: 'omena' in array bf2");
	}			/* so is the other one */
	insert_digest(bf2, sha256_string("luumu"));
	if (argc > 1 && strcmp(argv[1], "visualize") == 0) {
		printf("\nbf2 after 'luumu': ");
		debug_print_filter(bf2, FALSE);
	}

	bf2 = add_filter(bf2, bf);
	if (!is_in_array(bf2, sha256_string("omena"))) {
		error_count++;
		if (argc > 2)
			printf("\nError: 'omena' not in array bf2");
	}			/* bf2 should now contain omena */
	if (is_in_array(bf2, sha256_string("appelsiini"))) {
		error_count++;
		if (argc > 2)
			printf("\nError: 'appelsiini' in array bf2");
	}			/* ... but not appelsiini */
	if (argc > 1 && strcmp(argv[1], "visualize") == 0) {
		printf("\nbf2 after merge: ");
		debug_print_filter(bf2, FALSE);
	}

	release_bloom_filter(bf);
	release_bloom_filter(bf2);
	PRINTSTATUS;

	printf("  Testing filter groups...");
	fflush(stdout);
	tmperr = error_count;
	bfg = create_bloom_filter_group(8, 8);	/* 8 filters of 8bit length */

	j = 64;
	for (i = 0; i < j; i++) {
		sprintf(test, "%d", i);
		insert_digest_to_group_member(bfg, (i % (bfg->group_size - 1)) + 1, sha256_string(test));	/* add digests to group members 1..group_size leaving member 0 untouched */
	}

	for (i = 1; i < bfg->group_size; i++) {
		bfg->filter_group[0] = add_filter(bfg->filter_group[0], bfg->filter_group[i]);
	}

	for (i = 0; i < j; i++) {
		sprintf(test, "%d", i);
		if (!is_in_array(bfg->filter_group[0], sha256_string(test))) {
			error_count++;
			if (argc > 2)
				printf("\nError: %s not in array bfg[0]", test);
		}
	}

	if (argc > 1 && strcmp(argv[1], "visualize") == 0) {
		printf("\nbfg[0]: ");
		debug_print_filter(bfg->filter_group[0], FALSE);
	}

	release_bloom_filter_group(bfg);
	PRINTSTATUS;

	printf("  Testing rings...");
	fflush(stdout);
	tmperr = error_count;

	/* Ring queue test */
	/* Ring of 8 10-bit filters */
	brq = build_bloom_ring(8, 21);
	k = 128;

	/* Init */
	for (i = 0; i < k; i++) {
		if (i % (k / 8) == 0)
			rotate_bloom_ring_queue(brq);

		sprintf(test, "%d", i);
		insert_digest_bloom_ring_queue(brq, sha256_string(test));
	}

	/* Test for all inclusion */
	for (i = 0; i < k; i++) {
		sprintf(test, "%d", i);
		if (!is_in_ring_queue(brq, sha256_string(test))) {
			error_count++;
			if (argc > 2)
				printf("\nError: %s not in brq", test);
		}
	}

	/* Test with removal */
	for (i = 0; i < (k / 8); i++) {
		rotate_bloom_ring_queue(brq);
		for (j = i; j < (i + 1) * (k / 8); j++) {
			sprintf(test, "%d", i);
			if (is_in_ring_queue(brq, sha256_string(test))) {
				error_count++;
				if (argc > 2)
					printf("\nError: %s in brq", test);
			}
		}

		for (j = (i + 1) * (k / 8); j < (k / 8); j++) {
			sprintf(test, "%d", i);
			if (!is_in_ring_queue(brq, sha256_string(test))) {
				error_count++;
				if (argc > 2)
					printf("\nError: %s not in brq after removal", test);
			}
		}

	}
	PRINTSTATUS;

	snprintf(buf, MAXLINELEN - 1, "/tmp/test.state.%d", getpid());
	ctx->config.statefile = strdup(buf);
	ctx->config.num_bufs = 8;
	ctx->config.filter_size = 21;
	printf("  Testing statefile %s...", buf);
	fflush(stdout);
	tmperr = error_count;
	create_statefile();

	/* Ring queue test */
	/* Ring of 8 10-bit filters */
	brq = build_bloom_ring(8, 21);
	k = 128;

	/* Init */
	for (i = 0; i < k; i++) {
		if (i % (k / 8) == 0)
			rotate_bloom_ring_queue(brq);

		sprintf(test, "%d", i);
		insert_digest_bloom_ring_queue(brq, sha256_string(test));
	}

	/* Test for all inclusion */
	for (i = 0; i < k; i++) {
		sprintf(test, "%d", i);
		if (!is_in_ring_queue(brq, sha256_string(test))) {
			error_count++;
			if (argc > 2)
				printf("\nError: %s not in brq", test);
		}
	}

	/* Test with removal */
	for (i = 0; i < (k / 8); i++) {
		rotate_bloom_ring_queue(brq);
		for (j = i; j < (i + 1) * (k / 8); j++) {
			sprintf(test, "%d", i);
			if (is_in_ring_queue(brq, sha256_string(test))) {
				error_count++;
				if (argc > 2)
					printf("\nError: %s in brq", test);
			}
		}

		for (j = (i + 1) * (k / 8); j < (k / 8); j++) {
			sprintf(test, "%d", i);
			if (!is_in_ring_queue(brq, sha256_string(test))) {
				error_count++;
				if (argc > 2)
					printf("\nError: %s not in brq after removal", test);
			}
		}

	}
	release_bloom_ring_queue(brq);
	if (unlink(ctx->config.statefile))
		perror("unlink");
	Free(ctx->config.statefile);
	ctx->config.statefile = NULL;
	ctx->config.num_bufs = 8;
	ctx->config.filter_size = 21;
	PRINTSTATUS;

	printf("  Stress test...");
	fflush(stdout);
	tmperr = error_count;
	/* Stress test */
	brq = build_bloom_ring(10, 22);
	for (i = 0; i < 1000000; i++) {
		if ((i % 100000) == 0)
			rotate_bloom_ring_queue(brq);
		sprintf(test, "%d", i);
		insert_digest_bloom_ring_queue(brq, sha256_string(test));
	}

	for (i = 0; i < 1000000; i++) {
		sprintf(test, "%d", i);
		if (!is_in_ring_queue(brq, sha256_string(test))) {
			error_count++;
			if (argc > 2)
				printf("\nError: %s not in brq after removal", test);
		}
	}
	release_bloom_ring_queue(brq);

	PRINTSTATUS;
	
	if (error_count)
		printf("  Total error count: %d\n", error_count);

	return error_count > 0;
}

/* 
 * dummy function to avoid linking msgqueue.c
 */
int
put_msg(int msqid, void *omsgp, size_t msgsz, int msgflg)
{
	return 0;
}
