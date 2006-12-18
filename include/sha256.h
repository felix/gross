/* -*- mode:c; coding:utf-8 -*-
*************************************************************************
* File:    sha256.h
* Purpose: Compute SHA-256 checksum for byte vector
* Authors: Antti Siira <antasi@iki.fi>
* Tags: checksum sha 256 header
* Extra: http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
*************************************************************************/

#ifndef SHA256_H
#define SHA256_H

#define TRUE  1
#define FALSE 0

/* Types from inttypes.h */
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t  sha_byte_t;
typedef uint32_t sha_uint_t;
typedef uint64_t sha_ulong_t;

/* Struct for SHA-256 checksums */
typedef struct {
  sha_uint_t h0;
  sha_uint_t h1;
  sha_uint_t h2;
  sha_uint_t h3;
  sha_uint_t h4;
  sha_uint_t h5;
  sha_uint_t h6;
  sha_uint_t h7;
} sha_256_t;


int little_endian();
void swap_bytes(sha_byte_t* a, sha_byte_t* b);
void convert_int64_little_endian (sha_ulong_t *num);
void convert_int64_big_endian (sha_ulong_t *num);
void convert_int32_little_endian (sha_uint_t *num);
void convert_int32_big_endian (sha_uint_t *num);
sha_uint_t rotate_right(sha_uint_t num, int amount);
void debug_print_digest(sha_256_t digest, int with_newline);
sha_256_t sha256_string(char* message);
sha_256_t sha256(sha_byte_t* message, sha_ulong_t size);

#endif
