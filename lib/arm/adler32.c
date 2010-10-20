/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   arm implementation
 * Copyright (C) 1995-2004 Mark Adler
 * Copyright (C) 2009 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/*
 * This is not the original adler32.c from the zlib distribution,
 * but a heavily modified version. If you are looking for the
 * original, please go to zlib.net.
 */

#if defined(__ARM_NEON__) || defined(__ARM_ARCH_6__)  || \
    defined(__ARM_ARCH_6J__)  || defined(__ARM_ARCH_6Z__) || \
    defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_7A__)

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

# if defined(__ARM_NEON__)
#  include <arm_neon.h>
#  include "my_neon.h"

/* since we do not have the 64bit psadbw sum, we could prop. do a little more */
#  define VNMAX (6*NMAX)

static inline uint32x4_t vector_reduce(uint32x4_t x)
{
	uint32x4_t y;

	y = vshlq_n_u32(x, 16);
	x = vshrq_n_u32(x, 16);
	y = vshrq_n_u32(y, 16);
	y = vsubq_u32(y, x);
	x = vaddq_u32(y, vshlq_n_u32(x, 4));
	return x;
}

static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32x4_t v0_32 = (uint32x4_t){0,0,0,0};
	uint8x16_t    v0 = (uint8x16_t)v0_32;
	uint8x16_t vord, vord_a;
	uint32x4_t vs1, vs2;
	uint32x2_t v_tsum;
	uint8x16_t in16;
	uint32_t s1, s2;
	unsigned k;

	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;

	if(!buf)
		return 1L;

// TODO: byte order?
	if(HOST_IS_BIGENDIAN)
		vord = (uint8x16_t){16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1};
	else
		vord = (uint8x16_t){1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

	if(likely(len >= 2*SOVUCQ))
	{
		unsigned f, n;

		/*
		 * Add stuff to achieve alignment
		 */
		/* align hard down */
		f = (unsigned) ALIGN_DOWN_DIFF(buf, SOVUCQ);
		n = SOVUCQ - f;
		buf = (const unsigned char *)ALIGN_DOWN(buf, SOVUCQ);

		/* add n times s1 to s2 for start round */
		s2 += s1 * n;

		/* set sums 0 */
		vs1 = v0_32;
		vs2 = v0_32;
		/*
		 * the accumulating of s1 for every round grows very fast
		 * (quadratic?), even if we accumulate in 4 dwords, more
		 * rounds means nonlinear growth.
		 * We already split it out of s2, normaly it would be in
		 * s2 times 16... and even grow faster.
		 * Thanks to this split and vector reduction, we can stay
		 * longer in the loops. But we have to prepare for the worst
		 * (qht all 0xff), only do 6 times the work.
		 * (we could prop. stay a little longer since we have 4 sums,
		 * not 2 like on x86).
		 */
		k = len < VNMAX ? (unsigned)len : VNMAX;
		len -= k;
		/* insert scalar start somewhere */
		vs1 = vsetq_lane_u32(s1, vs1, 0);
		vs2 = vsetq_lane_u32(s2, vs2, 0);

		/* get input data */
		in16 = *(const uint8x16_t *)buf;
		/* mask out excess data */
		if(HOST_IS_BIGENDIAN) {
			in16 = neon_simple_alignq(v0, in16, n);
			vord_a = neon_simple_alignq(v0, vord, n);
		} else {
			in16 = neon_simple_alignq(in16, v0, f);
			vord_a = neon_simple_alignq(vord, v0, f);
		}

		/* pairwise add bytes and long, pairwise add word long acc */
		vs1 = vpadalq_u16(vs1, vpaddlq_u8(in16));
		/* apply order, add words, pairwise add word long acc */
		vs2 = vpadalq_u16(vs2,
			vmlal_u8(
				vmull_u8(vget_low_u8(in16), vget_low_u8(vord_a)),
				vget_high_u8(in16), vget_high_u8(vord_a)
			)
		);

		buf += SOVUCQ;
		k -= n;

		if(likely(k >= SOVUCQ)) do
		{
			uint32x4_t vs1_r = v0_32;
			do
			{
				/* add vs1 for this round */
				vs1_r = vaddq_u32(vs1_r, vs1);

				/* get input data */
				in16 = *(const uint8x16_t *)buf;

// TODO: make work in inner loop more tight
				/*
				 * decompose partial sums, so we do less instructions and
				 * build loops around it to do acc and so on only from time
				 * to time.
				 * This is hard with NEON, because the instruction are nice:
				 * we have the stuff in widening and with acc (practicaly
				 * for free...)
				 */
				/* pairwise add bytes and long, pairwise add word long acc */
				vs1 = vpadalq_u16(vs1, vpaddlq_u8(in16));
				/* apply order, add words, pairwise add word long acc */
				vs2 = vpadalq_u16(vs2,
					vmlal_u8(
						vmull_u8(vget_low_u8(in16), vget_low_u8(vord)),
						vget_high_u8(in16), vget_high_u8(vord)
					)
				);

				buf += SOVUCQ;
				k -= SOVUCQ;
			} while (k >= SOVUCQ);
			/* reduce vs1 round sum before multiplying by 16 */
			vs1_r = vector_reduce(vs1_r);
			/* add vs1 for this round (16 times) */
			/* they have shift right and accummulate, where is shift left and acc?? */
			vs2 = vaddq_u32(vs2, vshlq_n_u32(vs1_r, 4));
			/* reduce both vectors to something within 17 bit */
			vs2 = vector_reduce(vs2);
			vs1 = vector_reduce(vs1);
			len += k;
			k = len < VNMAX ? (unsigned) len : VNMAX;
			len -= k;
		} while(likely(k >= SOVUC));

		if(likely(k))
		{
			/*
			 * handle trailer
			 */
			f = SOVUCQ - k;
			/* add k times vs1 for this trailer */
			vs2 = vmlaq_u32(vs2, vs1, vdupq_n_u32(k));

			/* get input data */
			in16 = *(const uint8x16_t *)buf;
			/* masks out bad data */
			if(HOST_IS_BIGENDIAN)
				in16 = neon_simple_alignq(in16, v0, f);
			else
				in16 = neon_simple_alignq(v0, in16, k);

			/* pairwise add bytes and long, pairwise add word long acc */
			vs1 = vpadalq_u16(vs1, vpaddlq_u8(in16));
			/* apply order, add words, pairwise add word long acc */
			vs2 = vpadalq_u16(vs2,
				vmlal_u8(
					vmull_u8(vget_low_u8(in16), vget_low_u8(vord)),
					vget_high_u8(in16), vget_high_u8(vord)
				)
			);

			buf += k;
			k -= k;
		}

		/* add horizontal */
		v_tsum = vpadd_u32(vget_high_u32(vs1), vget_low_u32(vs1));
		v_tsum = vpadd_u32(v_tsum, v_tsum);
		s1 = vget_lane_u32(v_tsum, 0);
		v_tsum = vpadd_u32(vget_high_u32(vs2), vget_low_u32(vs2));
		v_tsum = vpadd_u32(v_tsum, v_tsum);
		s2 = vget_lane_u32(v_tsum, 0);
	}

	if(unlikely(len)) do {
		s1 += *buf++;
		s2 += s1;
	} while (--len);
	MOD(s1);
	MOD(s2);

	return s2 << 16 | s1;
}
static char const rcsid_a32n[] GCC_ATTR_USED_VAR = "$Id: $";

# else
/* ARM v6 or better */
#  define SOU32 (sizeof(uint32_t))
#  define SOVUCQ (4 * SOU32)
static inline uint32_t reduce(uint32_t x)
{
	uint32_t y = x & 0x0000ffff;
	x >>= 16;
	y -= x;
	x <<= 4;
	return x + y;
}

static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1, s2;
	unsigned k;

	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;

	if(!buf)
		return 1L;

	k    = ALIGN_DIFF(buf, SOU32);
	len -= k;
	if(k) do {
		s1 += *buf++;
		s2 += s1;
	} while (--k);

	if(likely(len >= 4*SOU32))
	{
		uint32_t vs1 = s1, vs2 = s2;
		uint32_t order_lo, order_hi;

// TODO: byte order?
		if(HOST_IS_BIGENDIAN) {
			order_lo = 0x00030001;
			order_hi = 0x00040002;
		} else {
			order_lo = 0x00020004;
			order_hi = 0x00010003;
		}
		k = len < NMAX ? (unsigned) len : NMAX;
		len -= k;

		do
		{
			uint32_t vs1_r = 0;
			do
			{
				uint32_t t21, t22, in;
				/* add vs1 for this round */
				vs1_r += vs1;

				/* get input data */
				in = *(const uint32_t *)buf;

				/* add horizontal and acc */
				asm("usada8 %0, %1, %2, %3" : "=r" (vs1) : "r" (in), "r" (0), "r" (vs1));
				/* widen bytes to words, apply order, add and acc */
				asm("uxtb16 %0, %1" : "=r" (t21) : "r" (in));
				asm("uxtb16 %0, %1, ror #8" : "=r" (t22) : "r" (in));
				asm("smlad %0, %1, %2, %3" : "=r" (vs2) : "r" (t21) , "r" (order_lo), "r" (vs2));
				asm("smlad %0, %1, %2, %3" : "=r" (vs2) : "r" (t22) , "r" (order_hi), "r" (vs2));

				buf += SOU32;
				k -= SOU32;
			} while (k >= SOU32);
			/* reduce vs1 round sum before multiplying by 4 */
			vs1_r = reduce(vs1_r);
			/* add vs1 for this round (4 times) */
			vs2 += vs1_r * 4;
			/* reduce both sums to something within 17 bit */
			vs2 = reduce(vs2);
			vs1 = reduce(vs1);
			len += k;
			k = len < NMAX ? (unsigned) len : NMAX;
			len -= k;
		} while(likely(k >= SOU32));
		len += k;
		s1 = vs1;
		s2 = vs2;
	}

	if(unlikely(len)) do {
		s1 += *buf++;
		s2 += s1;
	} while (--len);
// TODO: compiler creates calls to umodsi4
	/* v6 has umull, to create a 64Bit mul result, the compiler could
	 * create a "mull by invers", but doesn't. Try with reduce
	 * + if(s > BASE) s -= BASE; ? */
	MOD(s1);
	MOD(s2);

	return s2 << 16 | s1;
}
static char const rcsid_a32v6[] GCC_ATTR_USED_VAR = "$Id: $";
# endif

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
	if(len < SOVUCQ)
		return adler32_common(adler, buf, len);
	return adler32_vec(adler, buf, len);
}
#else
# include "../generic/adler32.c"
#endif
/* EOF */
