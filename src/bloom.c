/* -*- mode:c; coding:utf-8 -*-
 *
 * Copyright (c) 2006,2007
 *                    Antti Siira <antti@utu.fi>
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

#include "bloom.h"
#include "srvutils.h"

intraindex_t BITARRAY_BASE_SIZE = sizeof(bitarray_base_t) * BITS_PER_CHAR;

array_index_t 
array_index(bitindex_t bit_index)
{
	array_index_t index;
	index.array_index = bit_index / BITARRAY_BASE_SIZE;
	index.intra_index = bit_index % BITARRAY_BASE_SIZE;

	return index;
}

void 
debug_print_filter(bloom_filter_t * filter, int with_newline)
{
	int i;

	assert(filter);

	for (i = 0; i < filter->size; i++) {
		debug_print_bits(filter->filter[i], FALSE);
	}

	if (with_newline)
		printf("\n");
}

void 
debug_print_bits(int value, int with_newline)
{
	int i;
	for (i = 31; i >= 0; i--) {
		if ((value >> i) & 0x01)
			printf("1");
		else
			printf("0");
	}

	if (with_newline)
		printf("\n");
}

void 
debug_print_array_index(array_index_t index, int with_newline)
{
	printf("array index=%d intra index=%d", index.array_index, index.intra_index);
	if (with_newline)
		printf("\n");
}

void 
debug_print_bit_up(bitarray_base_t * array, bitindex_t bit_index, int with_newline)
{
	array_index_t index = array_index(bit_index);
	bitarray_base_t bit = get_bit(array, bit_index);

	assert(array);

	printf("bit %d at (", bit_index);
	debug_print_array_index(index, FALSE);
	printf(") is %d", bit);
	if (with_newline)
		printf("\n");
}

bitarray_base_t 
add_mask(intraindex_t intra_index)
{
        assert(intra_index <= 32);
	return 1 << intra_index;
}

bitarray_base_t 
get_bit(bitarray_base_t * array, bitindex_t bit_index)
{
	array_index_t index = array_index(bit_index);

	assert(array);
	assert(index.intra_index <= 32);

	return (array[index.array_index] >> index.intra_index) & 1;
}

void 
insert_bit(bitarray_base_t * array, bitindex_t bit_index)
{
	array_index_t index = array_index(bit_index);

	assert(array);
	assert(index.intra_index <= 32);

	array[index.array_index] |= add_mask(index.intra_index);
}

void 
init_bit_array(bitarray_base_t * array, bitindex_t size)
{
	bitindex_t i;
	assert(array);

	for (i = 0; i < size; i++) {
		array[i] = 0;
	}
}

bitindex_t 
int_to_index(unsigned int value, unsigned int mask)
{
	return (bitindex_t) (value & mask);
}

void 
insert_digest(bloom_filter_t * filter, sha_256_t digest)
{
        assert(filter);

	insert_bit(filter->filter, int_to_index(digest.h0, filter->mask));
	insert_bit(filter->filter, int_to_index(digest.h1, filter->mask));
	insert_bit(filter->filter, int_to_index(digest.h2, filter->mask));
	insert_bit(filter->filter, int_to_index(digest.h3, filter->mask));
	insert_bit(filter->filter, int_to_index(digest.h4, filter->mask));
	insert_bit(filter->filter, int_to_index(digest.h5, filter->mask));
	insert_bit(filter->filter, int_to_index(digest.h6, filter->mask));
	insert_bit(filter->filter, int_to_index(digest.h7, filter->mask));
}

int 
is_in_array(bloom_filter_t * filter, sha_256_t digest)
{
        assert(filter);

	return get_bit(filter->filter, int_to_index(digest.h0, filter->mask)) &&
	get_bit(filter->filter, int_to_index(digest.h1, filter->mask)) &&
	get_bit(filter->filter, int_to_index(digest.h2, filter->mask)) &&
	get_bit(filter->filter, int_to_index(digest.h3, filter->mask)) &&
	get_bit(filter->filter, int_to_index(digest.h4, filter->mask)) &&
	get_bit(filter->filter, int_to_index(digest.h5, filter->mask)) &&
	get_bit(filter->filter, int_to_index(digest.h6, filter->mask)) &&
	get_bit(filter->filter, int_to_index(digest.h7, filter->mask));
}

void 
insert_digest_to_group_member(bloom_filter_group_t * filter_group,
unsigned int member_index, sha_256_t digest)
{
	assert(filter_group);
	assert(member_index < filter_group->group_size);

	insert_digest(filter_group->filter_group[member_index], digest);
}


bloom_filter_t *
create_bloom_filter(bitindex_t num_bits)
{
	bloom_filter_t *result;

	assert(num_bits < sizeof(num_bits) * BITS_PER_CHAR);
	assert(num_bits >= 4);
	assert(num_bits <= 32);
	
	result = (bloom_filter_t *)Malloc(sizeof(bloom_filter_t));

	assert(result);

	result->bitsize = 1 << num_bits;
	result->mask = ((bitindex_t) - 1) >> (BITARRAY_BASE_SIZE - num_bits);
	result->filter = (bitarray_base_t *) Malloc(result->bitsize / BITS_PER_CHAR);
	result->size = result->bitsize / BITARRAY_BASE_SIZE;

	zero_bloom_filter(result);

	return result;
}

void 
release_bloom_filter(bloom_filter_t * filter)
{
	assert(filter->filter);
	free((bitarray_base_t *) (filter->filter));
	filter->filter = NULL;
	Free(filter);
}

void 
zero_bloom_filter(bloom_filter_t * filter)
{
        assert(filter);
	init_bit_array(filter->filter, filter->size);
}

bloom_filter_t* 
copy_bloom_filter(bloom_filter_t* filter, int empty)
{
  bloom_filter_t* tmp = (bloom_filter_t *)Malloc(sizeof(bloom_filter_t));

  assert(tmp);

  tmp->bitsize = filter->bitsize;
  tmp->mask = filter->mask;
  tmp->size = filter->size;
  tmp->filter = (bitarray_base_t *)Malloc(tmp->bitsize / BITS_PER_CHAR);

  assert(tmp->filter);

  if (empty) {
    zero_bloom_filter(tmp);
  } else {
    memcpy(tmp->filter, filter->filter, tmp->bitsize / BITS_PER_CHAR);
  }

  return tmp;
}

bloom_filter_group_t *
create_bloom_filter_group(unsigned int num, bitindex_t num_bits)
{
	bloom_filter_group_t *result;
	unsigned int i;

	assert(num > 0);
 	result = (bloom_filter_group_t *)Malloc(sizeof(bloom_filter_group_t));

	result->group_size = num;
	result->filter_group = (bloom_filter_t **)Malloc(sizeof(bloom_filter_group_t *) * num);

	assert(result->filter_group);

	for (i = 0; i < result->group_size; i++) {
		result->filter_group[i] = create_bloom_filter(num_bits);
		assert(result->filter_group[i]);
	}

	return result;
}

void 
release_bloom_filter_group(bloom_filter_group_t * filter_group)
{
	unsigned int i;

	assert(filter_group);

	for (i = 0; i < filter_group->group_size; i++) {
		release_bloom_filter(filter_group->filter_group[i]);
		filter_group->filter_group[i] = 0x00;
	}

	Free(filter_group);
}

double 
bloom_error_rate(unsigned int n, unsigned int k, unsigned int m)
{
	return 1.0 - exp(-(((double) n) * ((double) k)) / ((double) m)) * ((double) k);
}

unsigned int 
bloom_required_size(double c, unsigned int k, unsigned int n)
{
	return (unsigned int)((-(double) k) * ((double) n) /
log(1.0 - pow(c, (1.0 / ((double) k)))));
}


/* Returns the optimal number of bits required */
bitindex_t optimal_size(unsigned int n, double c)
{
	unsigned int result;
	unsigned int native_size = bloom_required_size(c, NUM_HASH, n);

	for (result = 1; result < BITARRAY_BASE_SIZE; result++) {
		if (bloom_required_size(c, NUM_HASH, 1 << result) >= native_size)
			return result;
	}

	/* Never reached */
	assert(0);
	return 0;
}

/* Adds filter rvalue to lvalue and return the address of lvalue */
bloom_filter_t * add_filter(bloom_filter_t * lvalue, const bloom_filter_t * rvalue)
{
	int i;

	assert(lvalue);
	assert(rvalue);

	assert(lvalue->size == rvalue->size);
	assert(lvalue->mask == rvalue->mask);

	for (i = 0; i < lvalue->size; i++) {
		lvalue->filter[i] |= rvalue->filter[i];
	}

	return lvalue;
}

bloom_ring_queue_t *
create_bloom_ring_queue(unsigned int num, bitindex_t num_bits)
{
	bloom_ring_queue_t *result;

	result = (bloom_ring_queue_t *)Malloc(sizeof(bloom_ring_queue_t));

	result->group = create_bloom_filter_group(num, num_bits);
	result->current_index = 0;
	result->aggregate = create_bloom_filter(num_bits);

	return result;
}

void 
release_bloom_ring_queue(bloom_ring_queue_t * brq)
{
  release_bloom_filter_group(brq->group);
  release_bloom_filter(brq->aggregate);
  Free(brq);
}

void 
insert_digest_bloom_ring_queue(bloom_ring_queue_t * brq, sha_256_t digest)
{
        assert(brq);
	insert_digest(brq->aggregate, digest);
	insert_digest_to_group_member(brq->group, brq->current_index, digest);
}

int 
is_in_ring_queue(bloom_ring_queue_t * brq, sha_256_t digest)
{
        assert(brq);
	return is_in_array(brq->aggregate, digest);
}

unsigned int 
bloom_ring_queue_next_index(bloom_ring_queue_t * brq)
{
        assert(brq);
	if (brq->current_index + 1 >= brq->group->group_size)
		return 0;

	return brq->current_index + 1;
}

bloom_ring_queue_t *
advance_bloom_ring_queue(bloom_ring_queue_t * brq)
{
        assert(brq);
	brq->current_index = bloom_ring_queue_next_index(brq);

	return brq;
}

bloom_ring_queue_t * rotate_bloom_ring_queue(bloom_ring_queue_t * brq)
{
	unsigned int i;
	bloom_filter_t* tmp = copy_bloom_filter(brq->aggregate, TRUE);

	zero_bloom_filter(brq->group->filter_group[bloom_ring_queue_next_index(brq)]);

	for (i = 0; i < brq->group->group_size; i++) {
		tmp = add_filter(tmp, brq->group->filter_group[i]);
	}

	advance_bloom_ring_queue(brq);
	memcpy(brq->aggregate->filter, tmp->filter, tmp->bitsize / BITS_PER_CHAR);

	release_bloom_filter(tmp);

	return brq;
}

void zero_bloom_ring_queue(bloom_ring_queue_t* brq)
{
  unsigned int i;

  assert(brq);
  zero_bloom_filter(brq->aggregate);
  for (i = 0; i < brq->group->group_size; i++) {
    zero_bloom_filter(brq->group->filter_group[i]);
  }
  
  brq->current_index = 0;
}

void 
debug_print_ring_queue(bloom_ring_queue_t * brq, int with_newline)
{
	unsigned int i;

	assert(brq);
	printf("Aggregate: ");
	debug_print_filter(brq->aggregate, TRUE);

	for (i = 0; i < brq->group->group_size; i++) {
		printf("Filter %d: ", i);
		debug_print_filter(brq->group->filter_group[i], TRUE);
	}

	if (with_newline)
		printf("\n");
}

void 
insert_absolute_bloom_ring_queue(bloom_ring_queue_t * brq, bitarray_base_t buffer[],
int size, int index, unsigned int buf_index)
{
	bitindex_t i;
	
	assert(brq);
	assert(buf_index < brq->group->group_size);

	if (size > brq->group->filter_group[buf_index]->size)
		size = brq->group->filter_group[buf_index]->size;

	for (i = 0; i < size; i++) {
		assert(index + i < brq->group->filter_group[buf_index]->size);
		brq->group->filter_group[buf_index]->filter[index * size + i] |= buffer[i];
	}
}

void 
sync_aggregate(bloom_ring_queue_t * brq)
{
	int i;
	unsigned int index = brq->current_index;

	zero_bloom_filter(brq->aggregate);

	for (i = 0; i < brq->group->group_size; i++) {
		brq->aggregate = add_filter(brq->aggregate, brq->group->filter_group[index]);

		index++;
		if (index >= brq->group->group_size)
			index = 0;
	}
}
