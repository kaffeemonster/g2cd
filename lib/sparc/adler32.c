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

/*
 * This code is mainly an educational thing and to beat the dust out
 * of the VIS unit on those SUN server, where no code will ever use it
 * (any graphic processing stuff on your server? Maybe "Blinken Lights").
 * On a normal Ultrasparc it is slightly slower. On an Ulrasparc3 it is
 * 50% faster.
 * The VIS unit is nice, but misses some instructions...
 * And it could be wider...
 */

#if defined(HAVE_VIS) && (defined(HAVE_REAL_V9) || defined(__sparcv9) || defined(__sparc_v9__))
# include "sparc_vis.h"

#define BASE 65521UL    /* largest prime smaller than 65536 */
/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */
#define NMAX 5552

/* use NO_DIVIDE if your processor does not do division in hardware */
#ifdef NO_DIVIDE
# define MOD(a) \
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
# define MOD4(a) \
	do { \
		if (a >= (BASE << 4)) a -= (BASE << 4); \
		if (a >= (BASE << 3)) a -= (BASE << 3); \
		if (a >= (BASE << 2)) a -= (BASE << 2); \
		if (a >= (BASE << 1)) a -= (BASE << 1); \
		if (a >= BASE) a -= BASE; \
	} while (0)
#else
# define MOD(a) a %= BASE
# define MOD4(a) a %= BASE
#endif

static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	unsigned long long scale_order_lo = 0x0400030002000100ULL;
	unsigned long long scale_order_hi = 0x0800070006000500ULL;
	unsigned long long v1_16 = 0x0001000100010001ULL;
	unsigned long long    v0 = 0;
	unsigned long long vs1, vs2;
	unsigned long long in8;
	uint32_t s1, s2;
	unsigned k;

	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;

	if(!buf)
		return 1L;

	while(likely(len))
	{
		k = len < NMAX ? (unsigned)len : NMAX;
		len -= k;

		if(likely(k >= SOVV))
		{
			unsigned f, n;
			const uint8_t *o_buf;

			/* align hard down */
			f = (unsigned) ALIGN_DOWN_DIFF(buf, SOVV);
			n = SOVV - f;

			/* add n times s1 to s2 for start round */
			s2 += s1 * n;

			/* insert scalar start somewhere */
			vs1 = s1;
			vs2 = s2;

			/* get input data */
			o_buf = buf;
			buf = alignaddr(buf, 0);
			in8 = *(const unsigned long long *)buf;
			{
				unsigned long long vs2l, vs2h, vs2t;

				/* swizzle data in place */
				in8 = faligndata(in8, v0);
				if(f) {
					alignaddrl(o_buf, 0);
					in8 = faligndata(v0, in8);
				}

				/* take sum of all bytes */
				vs1 = pdist(in8, v0, vs1);
				/* apply order, widen to 16 bit */
				vs2l = fmul8x16_lo(in8, scale_order_lo);
				vs2h = fmul8x16_hi(in8, scale_order_hi);
				vs2t = fpadd16(vs2l, vs2h);
				/* add horizontal */
				vs2l = fmuld8ulx16_lo(vs2t, v1_16);
				vs2h = fmuld8sux16_lo(vs2t, v1_16);
				vs2l = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2l, vs2);
				vs2l = fmuld8ulx16_hi(vs2t, v1_16);
				vs2h = fmuld8sux16_hi(vs2t, v1_16);
				vs2l = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2l, vs2);
			}

			buf += SOVV;
			k -= n;

			if(likely(k >= SOVV)) do
			{
				unsigned long long t, vs2l, vs2h, vs2t;

				/* add vs1 for this round (8 times) */
				t   = fpadd32(vs1, vs1); /* *2 */
				t   = fpadd32(t, t); /* *4 */
				t   = fpadd32(t, t); /* *8 */
				vs2 = fpadd32(vs2, t);

				/* get input data */
				in8 = *(const unsigned long long *)buf;

				/* add 8 byte horizontal and add to old dword */
				vs1 = pdist(in8, v0, vs1);

				/* apply order, widen to 16 bit */
				vs2l = fmul8x16_lo(in8, scale_order_lo);
				vs2h = fmul8x16_hi(in8, scale_order_hi);
				vs2t = fpadd16(vs2l, vs2h);
				/* add horizontal */
				vs2l = fmuld8ulx16_lo(vs2t, v1_16);
				vs2h = fmuld8sux16_lo(vs2t, v1_16);
				vs2  = fpadd32(fpadd32(vs2l, vs2h), vs2);
				vs2l = fmuld8ulx16_hi(vs2t, v1_16);
				vs2h = fmuld8sux16_hi(vs2t, v1_16);
				vs2  = fpadd32(fpadd32(vs2l, vs2h), vs2);

				buf += SOVV;
				k -= SOVV;
			} while (k >= SOVV);

			if(likely(k))
			{
				unsigned long long t, r, vs2l, vs2h, vs2t;
				size_t k_m;

				/* get input data */
				in8 = *(const unsigned long long *)buf;

				alignaddr(buf, k);
				/* swizzle data in place */
				in8 = faligndata(v0, in8);

				/* add k times vs1 for this trailer */
				/* since VIS muls suck, use the russian peasant method, k is small */
				t = 0;
				r = vs1;
				k_m = k;
				do
				{
					if(k_m & 1)
						t = fpadd32(t, r); /* add to result if odd */
					r = fpadd32(r, r); /* *2 */
					k_m >>= 1; /* /2 */
				} while(k_m);
				vs2 = fpadd32(vs2, t);

				/* add 8 byte horizontal and add to old dword */
				vs1 = pdist(in8, v0, vs1);

				/* apply order, widen to 16 bit */
				vs2l = fmul8x16_lo(in8, scale_order_lo);
				vs2h = fmul8x16_hi(in8, scale_order_hi);
				vs2t = fpadd16(vs2l, vs2h);
				/* add horizontal */
				vs2l = fmuld8ulx16_lo(vs2t, v1_16);
				vs2h = fmuld8sux16_lo(vs2t, v1_16);
				vs2l = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2l, vs2);
				vs2l = fmuld8ulx16_hi(vs2t, v1_16);
				vs2h = fmuld8sux16_hi(vs2t, v1_16);
				vs2l = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2l, vs2);

				buf += k;
				k -= k;
			}

			/* vs1 is one giant 64 bit sum, but we only calc to 32 bit */
			s1 = vs1;
			/* add both vs2 sums */
			s2 = (vs2 & 0xFFFFFFFFUL) + (vs2 >> 32);
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
	if(len < 2 * SOVV)
		return adler32_common(adler, buf, len);
	return adler32_vec(adler, buf, len);
}

static char const rcsid_a32g[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/adler32.c"
#endif
/* EOF */
