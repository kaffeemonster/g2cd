#ifndef LIB_HTHASH_H
# define LIB_HTHASH_H

/*
 * hthash.h
 * fast but distributed hash for hashtables
 */
# include "other.h"
# include "my_bitopsm.h"

# define rol32(x, k) (((x)<<(k)) | ((x)>>(32-(k))))
# ifdef HAVE_HW_MULT
/*
 * This is the MurmurHash3 hash, written by Austin Appleby
 *
 * Placed in the public domain. The author hereby disclaims
 * copyright to this source code.
 */

/*
 * This is a modified version of the MurmurHash3, so all erros are mine
 * Jan
 *
 * from a brief look at it:
 * This hash needs a fast multiplication, which gives good mixing
 * with a low instruction count. Fast multiplication is normaly true
 * today on x86 hardware >= P4/Athlon, but not so beefy/old hardware
 * one may want to use another hashing sheme, like jhash
 */
#  define MMHASH_MIX(h, k)	do { k *= 0xcc9e2d51; k = rol32(k, 15); k *= 0x1b873593; h ^= k; h = rol32(h, 13); h = h*5+0xe6546b64;} while(0)
#  define MMHASH_FINAL(h)	do { h ^= h >> 16; h *= 0x85ebca6b; h ^= h >> 13; h *= 0xc2b2ae35; h ^= h >> 16;} while(0)

static inline uint32_t hthash_6words(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t seed)
{
	uint32_t h = seed;

	MMHASH_MIX(h, a);
	MMHASH_MIX(h, b);
	MMHASH_MIX(h, c);
	MMHASH_MIX(h, d);
	MMHASH_MIX(h, e);
	MMHASH_MIX(h, f);
	h ^= 24;
	MMHASH_FINAL(h);
	return h;
}

static inline uint32_t hthash_5words(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t seed)
{
	uint32_t h = seed;

	MMHASH_MIX(h, a);
	MMHASH_MIX(h, b);
	MMHASH_MIX(h, c);
	MMHASH_MIX(h, d);
	MMHASH_MIX(h, e);
	h ^= 20;
	MMHASH_FINAL(h);
	return h;
}

static inline uint32_t hthash_4words(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t seed)
{
	uint32_t h = seed;

	MMHASH_MIX(h, a);
	MMHASH_MIX(h, b);
	MMHASH_MIX(h, c);
	MMHASH_MIX(h, d);
	h ^= 16;
	MMHASH_FINAL(h);
	return h;
}

static inline uint32_t hthash_3words(uint32_t a, uint32_t b, uint32_t c, uint32_t seed)
{
	uint32_t h = seed;

	MMHASH_MIX(h, a);
	MMHASH_MIX(h, b);
	MMHASH_MIX(h, c);
	h ^= 12;
	MMHASH_FINAL(h);
	return h;
}

static inline uint32_t hthash_2words(uint32_t a, uint32_t b, uint32_t seed)
{
	uint32_t h = seed;

	MMHASH_MIX(h, a);
	MMHASH_MIX(h, b);
	h ^= 8;
	MMHASH_FINAL(h);
	return h;
}

static inline uint32_t hthash_1words(uint32_t a, uint32_t seed)
{
	uint32_t h = seed;

	MMHASH_MIX(h, a);
	h ^= 4;
	MMHASH_FINAL(h);
	return h;
}

uint32_t hthash(const void *key, size_t len, uint32_t seed);
uint32_t hthash32(const uint32_t *key, size_t len, uint32_t seed);

#  define MMHASH_MIXM(h, k, mul)	do { k *= 0xcc9e2d51; k = rol32(k, 15); k *= mul; h ^= k; h = rol32(h, 13); h = h*5+0xe6546b64;} while(0)
static inline uint32_t hthash32_mod(const uint32_t *key, size_t len, uint32_t seed, uint32_t mul)
{
	/* Initialise hash */
	uint32_t h = seed;
	size_t i;

	/* Mix 4 bytes at a time into the hash */
	for(i = 0; i < len; i++) {
		uint32_t k = key[i];
		MMHASH_MIXM(h, k, mul);
	}
	h ^= len * 4;
	/*
	 * Do a few final mixes of the hash to ensure the last few
	 * bytes are well-incorporated.
	 */
	MMHASH_FINAL(h);
	return h;
}

#  ifndef HTHASH_C
#   undef MMHASH_MIX
#   undef MMHASH_FINAL
#   undef rol32
#  endif
#  undef MMHASH_MIXM
# else

/* Jenkins hash support.
 *
 * Copyright (C) 2006. Bob Jenkins (bob_jenkins@burtleburtle.net)
 *
 * http://burtleburtle.net/bob/hash/
 *
 * These are the credits from Bob's sources:
 *
 * lookup3.c, by Bob Jenkins, May 2006, Public Domain.
 *
 * These are functions for producing 32-bit hashes for hash table lookup.
 * hashword(), hashlittle(), hashlittle2(), hashbig(), mix(), and final()
 * are externally useful functions.  Routines to test the hash are included
 * if SELF_TEST is defined.  You can use this free for any purpose.  It's in
 * the public domain.  It has no warranty.
 *
 * I munched it some more to fit into g2cd, so bugs are my fault - Jan
 */

/* __jhash_mix -- mix 3 32-bit values reversibly. */
#  define __jhash_mix(a, b, c) \
{ \
 a -= c;  a ^= rol32(c, 4);  c += b; \
 b -= a;  b ^= rol32(a, 6);  a += c; \
 c -= b;  c ^= rol32(b, 8);  b += a; \
 a -= c;  a ^= rol32(c, 16); c += b; \
 b -= a;  b ^= rol32(a, 19); a += c; \
 c -= b;  c ^= rol32(b, 4);  b += a; \
}

/* __jhash_final - final mixing of 3 32-bit values (a,b,c) into c */
#define __jhash_final(a, b, c) \
{ \
 c ^= b; c -= rol32(b, 14); \
 a ^= c; a -= rol32(c, 11); \
 b ^= a; b -= rol32(a, 25); \
 c ^= b; c -= rol32(b, 16); \
 a ^= c; a -= rol32(c, 4);  \
 b ^= a; b -= rol32(a, 14); \
 c ^= b; c -= rol32(b, 24); \
}

/* Some initval: an arbitrary value */
#  define JHASH_INITVAL	0x9e3779b9

static inline uint32_t hthash_6words(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t seed)
{
	uint32_t x = JHASH_INITVAL + seed + (6 << 2);
	a += x;
	b += x;
	c += x;
	__jhash_mix(a, b, c);

	a += d;
	b += e;
	c += f;
	__jhash_final(a, b, c);
	return c;
}

static inline uint32_t hthash_5words(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t seed)
{
	uint32_t x = JHASH_INITVAL + seed + (5 << 2);
	a += x;
	b += x;
	c += x;
	__jhash_mix(a, b, c);

	a += d;
	b += e;
	__jhash_final(a, b, c);
	return c;
}

static inline uint32_t hthash_4words(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t seed)
{
	uint32_t x = JHASH_INITVAL + seed + (4 << 2);
	a += x;
	b += x;
	c += x;
	__jhash_mix(a, b, c);

	a += d;
	__jhash_final(a, b, c);
	return c;
}

static inline uint32_t hthash_3words(uint32_t a, uint32_t b, uint32_t c, uint32_t seed)
{
	uint32_t x = JHASH_INITVAL + seed + (3 << 2);
	a += x;
	b += x;
	c += x;
	__jhash_final(a, b, c);
	return c;
}

static inline uint32_t hthash_2words(uint32_t a, uint32_t b, uint32_t seed)
{
	uint32_t c = JHASH_INITVAL + seed + (2 << 2);
	a += c;
	b += c;
	__jhash_final(a, b, c);
	return c;
}

static inline uint32_t hthash_1words(uint32_t a, uint32_t seed)
{
	uint32_t c = JHASH_INITVAL + seed + (1 << 2), b;
	a += c;
	b = c;
	__jhash_final(a, b, c);
	return c;
}

uint32_t hthash32_mod(const uint32_t *key, size_t len, uint32_t seed, uint32_t rat);
uint32_t hthash(const void *key, size_t len, uint32_t seed);
static inline uint32_t hthash32(const uint32_t *key, size_t len, uint32_t seed)
{
	return hthash32_mod(key, len, seed, JHASH_INITVAL);
}
#  ifndef HTHASH_C
#   undef __jhash_mix
#   undef __jhash_final
#   undef JHASH_INITVAL
#   undef rol32
#  endif
# endif
#endif
