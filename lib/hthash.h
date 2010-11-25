#ifndef LIB_HTHASH_H
# define LIB_HTHASH_H

/*
 * hthash.h
 * fast but distributed hash for hashtables
 */
# include "other.h"
# include "my_bitopsm.h"

# ifdef HAVE_HW_MULT
/*
 * This is the MurmurHash2
 *
 * Copyright (c) by Autin Appleby
 *
 * Released to the public domain
 *
 */

/*
 * This is a modified version of the MurmurHash2, so all erros are mine
 * Jan
 *
 * from a brief look at it:
 * This hash needs a fast multiplication, which gives good mixing
 * with a low instruction count. Fast multiplication is normaly true
 * today on x86 hardware >= P4/Athlon, but not so beefy/old hardware
 * one may want to use another hashing sheme, like jhash
 */
/*
 * 'MRATIO' and 'MSHIFT' are mixing constants generated offline.
 * They're not really 'magic', they just happen to work well.
 */
#  define MSHIFT 24
#  define MRATIO 0x5BD1E995
#  define MMHASH_MIX(h, k)	{ k *= MRATIO; k ^= k >> MSHIFT; k *= MRATIO; h *= MRATIO; h ^= k; }
#  define MMHASH_FINAL(h)	{ h ^= h >> 13; h *= MRATIO; h ^= h >> 15; }

static inline uint32_t __hthash32_u(const uint32_t *key, size_t len, uint32_t seed)
{
	/* Initialise hash */
	uint32_t h = seed ^ len * 4;
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
	uint32_t h = seed ^ len * 4;
	size_t i;

	/* Mix 4 bytes at a time into the hash */
	for(i = 0; i < len; i++) {
		uint32_t k = key[i];
		MMHASH_MIX(h, k);
	}

	return h;
}

static inline uint32_t hthash32(const uint32_t *key, size_t len, uint32_t seed)
{
	uint32_t h = __hthash32(key, len, seed);
	/*
	 * Do a few final mixes of the hash to ensure the last few
	 * bytes are well-incorporated.
	 */
	MMHASH_FINAL(h)
	return h;
}

static inline uint32_t hthash(const void *key, size_t len, uint32_t seed)
{
	const unsigned char *data = key;
	uint32_t h;
	unsigned align = ALIGN_DOWN_DIFF(key, SO32);

	if(!align || UNALIGNED_OK)
	{
		if(len >= SO32)
		{
			if(align)
				h = __hthash32_u(key, len / SO32, seed);
			else
				h = __hthash32(key, len / SO32, seed);
			data = &data[len & ~SO32M1];
			len %= SO32;
		}
		if(len)
		{
			uint32_t t = get_unaligned((const uint32_t *)data);
			if(HOST_IS_BIGENDIAN)
				t >>= len * BITS_PER_CHAR;
			else {
				t <<= len * BITS_PER_CHAR;
				t >>= len * BITS_PER_CHAR;
			}
			h ^= t;
			h *= MRATIO;
		}
	}
	else
	{
		const uint32_t *dt_32;
		uint32_t t = 0, d = 0;
		int32_t sl, sr;
		h = seed ^ len;

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
		{
			h ^= t;
			h *= MRATIO;
		}
	}

	MMHASH_FINAL(h);
	return h;
}

static inline uint32_t hthash_6words(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t seed)
{
	uint32_t h = seed ^ 24;

	MMHASH_MIX(h, a);
	MMHASH_MIX(h, b);
	MMHASH_MIX(h, c);
	MMHASH_MIX(h, d);
	MMHASH_MIX(h, e);
	MMHASH_MIX(h, f);

	MMHASH_FINAL(h);
	return h;
}

static inline uint32_t hthash_5words(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t seed)
{
	uint32_t h = seed ^ 20;

	MMHASH_MIX(h, a);
	MMHASH_MIX(h, b);
	MMHASH_MIX(h, c);
	MMHASH_MIX(h, d);
	MMHASH_MIX(h, e);

	MMHASH_FINAL(h);
	return h;
}

static inline uint32_t hthash_4words(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t seed)
{
	uint32_t h = seed ^ 16;

	MMHASH_MIX(h, a);
	MMHASH_MIX(h, b);
	MMHASH_MIX(h, c);
	MMHASH_MIX(h, d);

	MMHASH_FINAL(h);
	return h;
}

static inline uint32_t hthash_3words(uint32_t a, uint32_t b, uint32_t c, uint32_t seed)
{
	uint32_t h = seed ^ 12;

	MMHASH_MIX(h, a);
	MMHASH_MIX(h, b);
	MMHASH_MIX(h, c);

	MMHASH_FINAL(h);
	return h;
}

static inline uint32_t hthash_2words(uint32_t a, uint32_t b, uint32_t seed)
{
	uint32_t h = seed ^ 8;

	MMHASH_MIX(h, a);
	MMHASH_MIX(h, b);

	MMHASH_FINAL(h);
	return h;
}

static inline uint32_t hthash_1words(uint32_t a, uint32_t seed)
{
	uint32_t h = seed ^ 4;

	MMHASH_MIX(h, a);

	MMHASH_FINAL(h);
	return h;
}

#  undef MMHASH_MIX
#  undef MMHASH_FINAL
#  define MMHASH_MIX(h, k, mul)	{ k *= mul; k ^= k >> MSHIFT; k *= mul; h *= mul; h ^= k; }
#  define MMHASH_FINAL(h, mul)	{ h ^= h >> 13; h *= mul; h ^= h >> 15; }
static inline uint32_t hthash32_mod(const uint32_t *key, size_t len, uint32_t seed, uint32_t mul)
{
	/* Initialise hash */
	uint32_t h = seed ^ len * 4;
	size_t i;

	/* Mix 4 bytes at a time into the hash */
	for(i = 0; i < len; i++) {
		uint32_t k = key[i];
		MMHASH_MIX(h, k, mul);
	}

	/*
	 * Do a few final mixes of the hash to ensure the last few
	 * bytes are well-incorporated.
	 */
	MMHASH_FINAL(h, mul)
	return h;
}

#  undef MSHIFT
#  undef MRATIO
#  undef MMHASH_MIX
#  undef MMHASH_FINAL

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

#  define rol32(x, k) (((x)<<(k)) | ((x)>>(32-(k))))

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

static inline uint32_t hthash(const void *key, size_t len, uint32_t seed)
{
	const unsigned char *data = key;
	size_t o_len = len;
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
	case 12: w[2] += (uint32_t)data[11] << 24;
	case 11: w[2] += (uint32_t)data[10] << 16;
	case 10: w[2] += (uint32_t)data[ 9] <<  8;
	case 9 : w[2] += (uint32_t)data[ 8];
	case 8 : w[1] += (uint32_t)data[ 7] << 24;
	case 7 : w[1] += (uint32_t)data[ 6] << 16;
	case 6 : w[1] += (uint32_t)data[ 5] <<  8;
	case 5 : w[1] +=           data[ 4];
	case 4 : w[0] += (uint32_t)data[ 3] << 24;
	case 3 : w[0] += (uint32_t)data[ 2] << 16;
	case 2 : w[0] += (uint32_t)data[ 1] <<  8;
	case 1 : w[0] +=           data[ 0];
		__jhash_final(w[0], w[1], w[2]);
	case 0 : break; /* Nothing left to add */
	};

	return w[2];
}

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

static inline uint32_t hthash32_mod(const uint32_t *key, size_t len, uint32_t seed, uint32_t rat)
{
	uint32_t w[3];

	w[0] = w[1] = w[2] = rat + seed + (len << 2);
	len = __hthash32(w, key, len);

	switch(len) {
	case 3: w[2] += key[2];
	case 2: w[1] += key[1];
	case 1: w[0] += key[0];
		__jhash_final(w[0], w[1], w[2]);
	case 0 : break; /* Nothing left to add */
	};

	return w[2];
}

static inline uint32_t hthash32(const uint32_t *key, size_t len, uint32_t seed)
{
	return hthash32_mod(key, len, seed, JHASH_INITVAL);
}

#  undef __jhash_mix
#  undef __jhash_final
#  undef rol32
#  undef JHASH_INITVAL
# endif
#endif
