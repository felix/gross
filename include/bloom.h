/* -*- mode:c; coding:utf-8 -*-
*************************************************************************
* File:    bloom.h
* Purpose: Probabilistic set membership
* Authors: Antti Siira <antasi@iki.fi>
* Tags:    bloom filter header
*************************************************************************/

#ifndef BLOOM_H
#define BLOOM_H

#include "sha256.h"

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef uint32_t bitindex_t;
typedef uint32_t bitarray_base_t;
typedef bitindex_t bitmask_t;
typedef uint32_t intraindex_t;

typedef struct {
	bitindex_t array_index;
	intraindex_t intra_index;
}      array_index_t;

typedef struct {
	bitarray_base_t *filter;
        bitindex_t bitsize; /* Number of bits */
        bitmask_t mask;
        bitindex_t size; /* number of bitarray_base_t elements */
 }      bloom_filter_t;

typedef struct {
	bloom_filter_t **filter_group;
	unsigned int group_size;
}      bloom_filter_group_t;

typedef struct {
	bloom_filter_group_t *group;
	bloom_filter_t *aggregate;
	unsigned int current_index;
}      bloom_ring_queue_t;

typedef struct {
	char magic[8];
	bloom_ring_queue_t *brq;
	size_t lumpsize;
        time_t last_rotate;
} mmapped_brq_t;

#define BITARRAY_SIZE_BITS ((int32_t)24)
#define BITS_PER_CHAR      ((uint32_t)8)
#define NUM_HASH           ((uint32_t)8)

extern intraindex_t BITARRAY_BASE_SIZE;

array_index_t array_index(bitindex_t bit_index);
void debug_print_filter(bloom_filter_t * filter, int with_newline);
void debug_print_array_index(array_index_t index, int with_newline);
void debug_print_bit_up(bitarray_base_t * array, bitindex_t bit_index, int with_newline);
bitarray_base_t add_mask(intraindex_t intra_index);
bitarray_base_t get_bit(bitarray_base_t * array, bitindex_t bit_index);
void insert_bit(bitarray_base_t * array, bitindex_t bit_index);
void init_bit_array(bitarray_base_t * array, bitindex_t size);
void debug_print_bits(int value, int with_newline);
bitindex_t int_to_index(unsigned int value, unsigned int mask);
void insert_digest(bloom_filter_t * filter, sha_256_t digest);
int is_in_array(bloom_filter_t * filter, sha_256_t digest);
bloom_filter_t *create_bloom_filter(bitindex_t num_bits);
bloom_filter_t* copy_bloom_filter(bloom_filter_t* filter, int empty);
void release_bloom_filter(bloom_filter_t * filter);
bloom_filter_group_t *create_bloom_filter_group(unsigned int num, bitindex_t num_bits);
void release_bloom_filter_group(bloom_filter_group_t * filter_group);
double bloom_error_rate(unsigned int n, unsigned int k, unsigned int m);
unsigned int bloom_required_size(double c, unsigned int k, unsigned int n);
bitindex_t optimal_size(unsigned int n, double c);
bloom_filter_t *add_filter(bloom_filter_t * lvalue, const bloom_filter_t * rvalue);
void insert_digest_to_group_member(bloom_filter_group_t * filter_group, unsigned int member_index, sha_256_t digest);
bloom_ring_queue_t *create_bloom_ring_queue(unsigned int num, bitindex_t num_bits);
void release_bloom_ring_queue(bloom_ring_queue_t * brq);
void insert_digest_bloom_ring_queue(bloom_ring_queue_t * brq, sha_256_t digest);
bloom_ring_queue_t *rotate_bloom_ring_queue(bloom_ring_queue_t * brq);
void zero_bloom_filter(bloom_filter_t * filter);
void zero_bloom_ring_queue(bloom_ring_queue_t* brq);
bloom_ring_queue_t *advance_bloom_rinq_queue(bloom_ring_queue_t * brq);
unsigned int bloom_rinq_queue_next_index(bloom_ring_queue_t * brq);
int is_in_ring_queue(bloom_ring_queue_t * brq, sha_256_t digest);
void debug_print_ring_queue(bloom_ring_queue_t * brq, int with_newline);
void insert_absolute_bloom_ring_queue(bloom_ring_queue_t * brq, bitarray_base_t buffer[], int size, int index, unsigned int buf_index);
void sync_aggregate(bloom_ring_queue_t * brq);

#endif
