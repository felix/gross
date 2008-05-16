/* $Id$ */

/*
 * This file is in the public domain
 */

#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define hashfunc(a, b) (hashlittle(a, b, 0x715517) & HASHMASK)

uint32_t hashlittle( const void *key, size_t length, uint32_t initval);
