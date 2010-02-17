/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   sparc/sparc64 implementation
 * Copyright (C) 1995-2004 Mark Adler
 * Copyright (C) 2009-2010 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/*
 * This is not the original adler32.c from the zlib distribution,
 * but a heavily modified version. If you are looking for the
 * original, please go to zlib.net.
 */

# include "ia64.h"

# define BASE 65521UL    /* largest prime smaller than 65536 */
/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */
# define NMAX 5552

/* use NO_DIVIDE if your processor does not do division in hardware */
# ifdef NO_DIVIDE
#  define MOD(a) \
	do { \
		if (a >= (BASE << 16)) a -= (BASE << 16); \
		if (a >= (BASE << 15)) a -= (BASE << 15); \
		if (a >= (BASE << 14)) a -= (BASE << 14); \
		if (a >= (BASE << 13)) a -= (BASE << 13); \
		if (a >= (BASE << 12)) a -= (BASE << 12); \
		if (a >= (BASE << 11)) a -= (BASE << 11); \
		if (a >= (BASE << 10)) a -= (BASE << 10); \
		if (a >= (BASE << 9)) a -= (BASE << 9); \
		if (a >= (BASE << 8)) a -= (BASE << 8); \
		if (a >= (BASE << 7)) a -= (BASE << 7); \
		if (a >= (BASE << 6)) a -= (BASE << 6); \
		if (a >= (BASE << 5)) a -= (BASE << 5); \
		if (a >= (BASE << 4)) a -= (BASE << 4); \
		if (a >= (BASE << 3)) a -= (BASE << 3); \
		if (a >= (BASE << 2)) a -= (BASE << 2); \
		if (a >= (BASE << 1)) a -= (BASE << 1); \
		if (a >= BASE) a -= BASE; \
	} while (0)
#  define MOD4(a) \
	do { \
		if (a >= (BASE << 4)) a -= (BASE << 4); \
		if (a >= (BASE << 3)) a -= (BASE << 3); \
		if (a >= (BASE << 2)) a -= (BASE << 2); \
		if (a >= (BASE << 1)) a -= (BASE << 1); \
		if (a >= BASE) a -= BASE; \
	} while (0)
# else
#  define MOD(a) a %= BASE
#  define MOD4(a) a %= BASE
# endif

static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	unsigned long long scale_order_lo;
	unsigned long long scale_order_hi;
	unsigned long long vs1, vs2, vs1s;
	unsigned long long in8;
	uint32_t s1, s2;
	unsigned k;

	if(!buf)
		return 1L;

	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;
	if(!HOST_IS_BIGENDIAN) {
		scale_order_lo = 0x0005000600070008ULL;
		scale_order_hi = 0x0001000200030004ULL;
	} else {
		scale_order_lo = 0x0004000300020001ULL;
		scale_order_hi = 0x0008000700060005ULL;
	}

	while(likely(len))
	{
		k = len < NMAX ? (unsigned)len : NMAX;
		len -= k;

		if(likely(k >= SOULL))
		{
			unsigned f, n;
			const uint8_t *o_buf;

			/* align hard down */
			f = (unsigned) ALIGN_DOWN_DIFF(buf, SOULL);
			o_buf = buf;
			buf = (const uint8_t *)ALIGN_DOWN(buf, SOULL);
			n = SOULL - f;

			/* add n times s1 to s2 for start round */
			s2 += s1 * n;

			/* insert scalar start somewhere */
			vs1 = s1;
			vs2 = s2;
			/* get input data */
			in8 = *(const unsigned long long *)buf;
			{
				unsigned long long vs2l, vs2h;

				/* get input data */
				in8 = *(const unsigned long long *)buf;
				if(f)
				{
					if(!HOST_IS_BIGENDIAN) {
						in8 >>= f * BITS_PER_CHAR;
						in8 <<= f * BITS_PER_CHAR;
					} else {
						in8 <<= f * BITS_PER_CHAR;
						in8 >>= f * BITS_PER_CHAR;
					}
				}

				/* add 8 byte horizontal and add to old dword */
				vs1 += psad1(in8, 0);
				/* apply order, widen to 32 bit */
				vs2l = unpack1l(0, in8);
				vs2h = unpack1h(0, in8);
				vs2l = pmpy2r(vs2l, scale_order_lo) + pmpy2l(vs2l, scale_order_lo);
				vs2h = pmpy2r(vs2h, scale_order_hi) + pmpy2l(vs2h, scale_order_hi);

				vs2 += vs2l + vs2h;
			}

			buf += SOULL;
			k -= n;

			vs1s = 0;
			if(likely(k >= SOULL)) do
			{
				unsigned long long vs2l_sum, vs2h_sum;
				unsigned long long vs2l, vs2h;
				unsigned j;

				j = k / SOULL;
				k -= j * SOULL;
				vs2l_sum = vs2h_sum = 0;
				do
				{
					/* add vs1 for this round */
					vs1s += vs1;
					/* get input data */
					in8 = *(const unsigned long long *)buf;
					/* add 8 byte horizontal and add to old dword */
					vs1 += psad1(in8, 0);
					/* apply order, widen to 32 bit */
					vs2l = unpack1l(0, in8);
					vs2h = unpack1h(0, in8);
					vs2l_sum += pmpy2r(vs2l, scale_order_lo) + pmpy2l(vs2l, scale_order_lo);
					vs2h_sum += pmpy2r(vs2h, scale_order_hi) + pmpy2l(vs2h, scale_order_hi);
					buf += SOULL;
				} while(--j);

// TODO: signed overflow?
				/*
				 * we do not use the 32 bit cleanly as packed,
				 * since Intel in it's fscking wisdom again forgot
				 * instructions...
				 * Since we are cleanly calculating till 32 bit,
				 * and ia64 is 64 bit, does it matter when the high
				 * sum turns > 0x8000000?
				 */
				vs2 += vs2l_sum + vs2h_sum;
			} while (k >= SOULL);
			vs2 += vs1s * 8;

			if(likely(k))
			{
				unsigned long long vs2l, vs2h;
				/* get input data */
				in8 = *(const unsigned long long *)buf;

				/* swizzle data in place */
				if(!HOST_IS_BIGENDIAN) {
					in8 <<= k * BITS_PER_CHAR;
					in8 >>= k * BITS_PER_CHAR;
				} else {
					in8 >>= k * BITS_PER_CHAR;
					in8 <<= k * BITS_PER_CHAR;
				}

				/* add k times vs1 for this trailer */
				vs2 += vs1 * k;

				/* add 8 byte horizontal and add to old dword */
				vs1 += psad1(in8, 0);
				/* apply order, widen to 32 bit */
				vs2l = unpack1l(0, in8);
				vs2h = unpack1h(0, in8);
				vs2l = pmpy2r(vs2l, scale_order_lo) + pmpy2l(vs2l, scale_order_lo);
				vs2h = pmpy2r(vs2h, scale_order_hi) + pmpy2l(vs2h, scale_order_hi);
				vs2 += vs2l + vs2h;

				buf += k;
				k -= k;
			}

			/* vs1 is one giant 64 bit sum, but we only calc to 32 bit */
			s1 = vs1;
			/* add both vs2 sums */
			s2 = unpack4h(0, vs2) + unpack4l(0, vs2);
		}

		if(unlikely(k)) do {
			s1 += *buf++;
			s2 += s1;
		} while (--k);
		MOD(s1);
		MOD(s2);
	}

	return s2 << 16 | s1;
}

/* ========================================================================= */
static noinline uint32_t adler32_common(uint32_t adler, const uint8_t *buf, unsigned len)
{
	/* split Adler-32 into component sums */
	uint32_t sum2 = (adler >> 16) & 0xffff;
	adler &= 0xffff;

	/* in case user likes doing a byte at a time, keep it fast */
	if(len == 1)
	{
		adler += buf[0];
		if(adler >= BASE)
			adler -= BASE;
		sum2 += adler;
		if(sum2 >= BASE)
			sum2 -= BASE;
		return adler | (sum2 << 16);
	}

	/* initial Adler-32 value (deferred check for len == 1 speed) */
	if(buf == NULL)
		return 1L;

	/* in case short lengths are provided, keep it somewhat fast */
	while(len--) {
		adler += *buf++;
		sum2 += adler;
	}
	if(adler >= BASE)
		adler -= BASE;
	MOD4(sum2);	/* only added so many BASE's */
	return adler | (sum2 << 16);
}

uint32_t adler32(uint32_t adler, const uint8_t *buf, unsigned len)
{
	if(len < 2 * SOULL)
		return adler32_common(adler, buf, len);
	return adler32_vec(adler, buf, len);
}

static char const rcsid_a32ia64[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
