#define HTHASH_C
#include "hthash.h"

#ifdef HAVE_HW_MULT
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
static inline uint32_t __hthash32_u(const uint32_t *key, size_t len, uint32_t seed)
{
	/* Initialise hash */
	uint32_t h = seed;
	size_t i;

	/* Mix 4 bytes at a time into the hash */
	for(i = 0; i < len; i++) {
		uint32_t k = get_unaligned(&key[i]);
		MMHASH_MIX(h, k);
	}

	return h;
}

static inline uint32_t __hthash32(const uint32_t *key, size_t len, uint32_t seed)
{
	/* Initialise hash */
	uint32_t h = seed;
	size_t i;

	/* Mix 4 bytes at a time into the hash */
	for(i = 0; i < len; i++) {
		uint32_t k = key[i];
		MMHASH_MIX(h, k);
	}

	return h;
}

uint32_t hthash32(const uint32_t *key, size_t len, uint32_t seed)
{
	uint32_t h = __hthash32(key, len, seed);
	/*
	 * Do a few final mixes of the hash to ensure the last few
	 * bytes are well-incorporated.
	 */
	h ^= len;
	MMHASH_FINAL(h);
	return h;
}

uint32_t hthash(const void *key, size_t len, uint32_t seed)
{
	const unsigned char *data = key;
	size_t o_len = len;
	uint32_t h = seed;
	unsigned align = ALIGN_DOWN_DIFF(key, SO32);

	if(!align || UNALIGNED_OK)
	{
		uint32_t t = 0;
		if(len >= SO32)
		{
			if(align)
				h = __hthash32_u(key, len / SO32, seed);
			else
				h = __hthash32(key, len / SO32, seed);
			data = &data[len & ~SO32M1];
			len %= SO32;
		}
		switch(len)
		{
		case 3: t |= data[2] << 16; GCC_FALL_THROUGH
		case 2: t |= data[1] <<  8; GCC_FALL_THROUGH
		case 1: t |= data[0];
				MMHASH_MIX(h, t);
		}
	}
	else
	{
		const uint32_t *dt_32;
		uint32_t t = 0, d = 0;
		int32_t sl, sr;

		dt_32 = (const uint32_t *)ALIGN_DOWN(data, SO32);
		t     = *dt_32++;
		len  -= SO32 - align;
		sl    = BITS_PER_CHAR * (SO32 - align);
		sr    = BITS_PER_CHAR * align;
		for(; len >= SO32; t = d, len -= SO32, dt_32++) {
			d = *dt_32;
			if(HOST_IS_BIGENDIAN)
				t = (t << sr) | (d >> sl);
			else
				t = (t >> sr) | (d << sl);
			MMHASH_MIX(h, t);
		}

		d = *dt_32++;
		if(HOST_IS_BIGENDIAN)
			t = (t << sr) | (d >> sl);
		else
			t = (t >> sr) | (d << sl);

		if(len >= align)
		{
			MMHASH_MIX(h, t);
			len  -= align;
			if(len) /* shifting out everything is not supported on all plattforms */
			{
				t = d;
				sl = (SO32 * BITS_PER_CHAR) - sl;
				if(HOST_IS_BIGENDIAN) {
					t <<=  sl;
					t >>= (len * BITS_PER_CHAR) + sl;
				} else {
					t <<=  len * BITS_PER_CHAR;
					t >>= (len * BITS_PER_CHAR) + sl;
				}
			}
		}
		if(len)
			MMHASH_MIX(h, t);
	}

	h ^= o_len;
	MMHASH_FINAL(h);
	return h;
}

#else
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
static inline size_t __hthash32_u(uint32_t w[3], const uint32_t *key, size_t len)
{
	while(len > 3)
	{
		w[0] += get_unaligned(&key[0]);
		w[1] += get_unaligned(&key[1]);
		w[2] += get_unaligned(&key[2]);
		__jhash_mix(w[0], w[1], w[2]);
		key += 3; len -= 3;
	}
	return len;
}

static inline size_t __hthash32(uint32_t w[3], const uint32_t *key, size_t len)
{
	while(len > 3)
	{
		w[0] += key[0];
		w[1] += key[1];
		w[2] += key[2];
		__jhash_mix(w[0], w[1], w[2]);
		key += 3; len -= 3;
	}
	return len;
}

uint32_t hthash(const void *key, size_t len, uint32_t seed)
{
	const unsigned char *data = key;
	uint32_t w[3];

	w[0] = w[1] = w[2] = JHASH_INITVAL + seed + len;
	if(len > SO32 * 3)
	{
		unsigned align = ALIGN_DOWN_DIFF(key, SO32);
		size_t t_len, r_len;

		if(align)
			t_len = __hthash32_u(w, key, len / SO32);
		else
			t_len = __hthash32(w, key, len / SO32);
		r_len = len - (((len / SO32) - t_len) * SO32);
		data = &data[len - r_len];
		len = r_len;
	}

	switch (len)
	{
// TODO: bring into cpu endianess
	case 12: w[2] += (uint32_t)data[11] << 24; GCC_FALL_THROUGH
	case 11: w[2] += (uint32_t)data[10] << 16; GCC_FALL_THROUGH
	case 10: w[2] += (uint32_t)data[ 9] <<  8; GCC_FALL_THROUGH
	case 9 : w[2] += (uint32_t)data[ 8]; GCC_FALL_THROUGH
	case 8 : w[1] += (uint32_t)data[ 7] << 24; GCC_FALL_THROUGH
	case 7 : w[1] += (uint32_t)data[ 6] << 16; GCC_FALL_THROUGH
	case 6 : w[1] += (uint32_t)data[ 5] <<  8; GCC_FALL_THROUGH
	case 5 : w[1] +=           data[ 4]; GCC_FALL_THROUGH
	case 4 : w[0] += (uint32_t)data[ 3] << 24; GCC_FALL_THROUGH
	case 3 : w[0] += (uint32_t)data[ 2] << 16; GCC_FALL_THROUGH
	case 2 : w[0] += (uint32_t)data[ 1] <<  8; GCC_FALL_THROUGH
	case 1 : w[0] +=           data[ 0];
		__jhash_final(w[0], w[1], w[2]); GCC_FALL_THROUGH
	case 0 : break; /* Nothing left to add */
	};

	return w[2];
}

uint32_t hthash32_mod(const uint32_t *key, size_t len, uint32_t seed, uint32_t rat)
{
	uint32_t w[3];

	w[0] = w[1] = w[2] = rat + seed + (len << 2);
	len = __hthash32(w, key, len);

	switch(len) {
	case 3: w[2] += key[2]; GCC_FALL_THROUGH
	case 2: w[1] += key[1]; GCC_FALL_THROUGH
	case 1: w[0] += key[0];
		__jhash_final(w[0], w[1], w[2]); GCC_FALL_THROUGH
	case 0 : break; /* Nothing left to add */
	};

	return w[2];
}

#endif
