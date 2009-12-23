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
// TODO: provide non mul hash
# include "other.h"
# include "my_bitopsm.h"

/*
 * 'MRATIO' and 'MSHIFT' are mixing constants generated offline.
 * They're not really 'magic', they just happen to work well.
 */
# define MSHIFT 24
# define MRATIO 0x5BD1E995
# define MMHASH_MIX(h, k)	{ k *= MRATIO; k ^= k >> MSHIFT; k *= MRATIO; h *= MRATIO; h ^= k; }
# define MMHASH_FINAL(h)	{ h ^= h >> 13; h *= MRATIO; h ^= h >> 15; }

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
