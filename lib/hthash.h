#ifndef LIB_HTHASH_H
# define LIB_HTHASH_H

/*
 * hthash.h
 * fast but distributed hash for hashtables
 *
 *
 * This is the MurmurHash2
 *
 * Copyright (c) by Autin Appleby
 *
 * Released to the public domain
 *
 */

/*
 * from a brief look at it:
 * This hash needs a fast multiplication, which gives good mixing
 * with a low instruction count. Fast multiplication is normaly true
 * today on x86 hardware >= P4/Athlon, but not so beefy/old hardware
 * one may want to use another hashing sheme, like jhash
 */
# include "other.h"

# ifndef UNALIGNED_OK
#  define UNALIGNED_OK 0
# endif

/*
 * 'MRATIO' and 'MSHIFT' are mixing constants generated offline.
 * They're not really 'magic', they just happen to work well.
 */
# define MSHIFT 24
# define MRATIO 0x5BD1E995
# define MMHASH_MIX(h, k)	{ k *= MRATIO; k ^= k >> MSHIFT; k *= MRATIO; h *= MRATIO; h ^= k; }
# define MMHASH_FINAL(h)	{ h ^= h >> 13; h *= MRATIO; h ^= h >> 15; }

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
	unsigned align = (uintptr_t)key & 3;

	if(UNALIGNED_OK || !align || len < 4)
	{
		h = __hthash32(key, len / 4, seed);
		data = &data[len & ~((size_t)3)];
		/* handle remainder */
		switch(len % 4)
		{
		case 3: h ^= data[2] << 16;
		case 2: h ^= data[1] << 8;
		case 1: h ^= data[0];
		        h *= MRATIO;
		}
	}
	else
	{
		uint32_t t = 0, d = 0;
		int32_t sl, sr;
		h = seed ^ len;

		switch(align)
		{
		case 1: t |= data[2] << 16;
		case 2: t |= data[1] << 8;
		case 3: t |= data[0];
		}
		t <<= (8 * align);

		data += 4 - align;
		len -= 4 - align;
		sl = 8 * (4 - align);
		sr = 8 * align;

		for(; len >= 4; t = d, len -= 4, data += 4) {
			d = *(const uint32_t *)data;
			t = (t >> sr) | (d << sl);
			MMHASH_MIX(h, t);
		}

		d = 0;
		if(len >= align)
		{
			switch(len)
			{
			case 3: d |= data[2] << 16;
			case 2: d |= data[1] << 8;
			case 1: d |= data[0];
			}
			t = (t >> sr) | (d << sl);

			MMHASH_MIX(h, t);

			data += align;
			len -= align;
			/* handle remainder */
			switch(len)
			{
			case 3: h ^= data[2] << 16;
			case 2: h ^= data[1] << 8;
			case 1: h ^= data[0];
			        h *= MRATIO;
			}
		}
		else
		{
			switch(len)
			{
			case 3: d |= data[2] << 16;
			case 2: d |= data[1] << 8;
			case 1: d |= data[0];
			case 0: h ^= (t >> sr) | (d << sl);
			        h *= MRATIO;
			}
		}
	}

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

# undef MMHASH_MIX
# undef MMHASH_FINAL
# define MMHASH_MIX(h, k, mul)	{ k *= mul; k ^= k >> MSHIFT; k *= mul; h *= mul; h ^= k; }
# define MMHASH_FINAL(h, mul)	{ h ^= h >> 13; h *= mul; h ^= h >> 15; }
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

# undef MSHIFT
# undef MRATIO
# undef MMHASH_MIX
# undef MMHASH_FINAL
#endif
