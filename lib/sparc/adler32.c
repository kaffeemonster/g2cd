/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   sparc/sparc64 implementation
 * Copyright (C) 1995-2004 Mark Adler
 * Copyright (C) 2009-2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/*
 * This is not the original adler32.c from the zlib distribution,
 * but a heavily modified version. If you are looking for the
 * original, please go to zlib.net.
 */

#if 0 && defined(HAVE_VIS) && (defined(HAVE_REAL_V9) || defined(__sparcv9) || defined(__sparc_v9__))
/*
 * I should have messured against the full adler32 code...
 *
 * This code is only for educational and documentry purpose.
 * It is always slower than the generic code, CPU 1 : FGU 0.
 *
 * UltraSPARC:
 * VIS:   10000 * 262144 bytes  t: 122300 ms
 * CPU:   10000 * 262144 bytes  t: 85500 ms
 * UltraSPARC III:
 * VIS2:  10000 * 262144 bytes  t: 41600 ms
 * CPU:   10000 * 262144 bytes  t: 35800 ms
 *
 * If you like, with this code, you can beat the dust out of the VIS
 * unit on your SUN server, where no code will ever use it (any
 * graphic processing stuff on your server? Maybe "Blinken Lights").
 *
 * And when unrolling makes your code slower (wtf?) even if you do not
 * bust your register set...
 * The VIS unit is nice, but misses some instructions, needs shorter
 * pipelines... And it could be wider... And not in the FPU, Niagara will
 * hate this code...
 */
# define HAVE_ADLER32_VEC
static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len);
# define MIN_WORK 16
#endif

#include "../generic/adler32.c"

#if 0 && defined(HAVE_VIS) && (defined(HAVE_REAL_V9) || defined(__sparcv9) || defined(__sparc_v9__))
# include "sparc_vis.h"

static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	unsigned long long scale_order_lo = 0x0400030002000100ULL;
	unsigned long long scale_order_hi = 0x0800070006000500ULL;
	unsigned long long v1_16 = 0x0001000100010001ULL;
	unsigned long long    v0 = 0;
	unsigned long long vs1, vs2, vs1s;
	unsigned long long in8;
	uint32_t s1, s2;
	unsigned k;

	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;

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
# ifdef HAVE_VIS2
			/* set swizzle mask */
			write_bmask1(0x88018845);
# endif
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
# ifdef HAVE_VIS2
				vs2l = fpand(vs2t, 0x0000FFFF0000FFFFULL);
				vs2h = bshuffle(vs2t, v0);
				vs2t = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2t, vs2);
# else
				vs2l = fmuld8ulx16_lo(vs2t, v1_16);
				vs2h = fmuld8sux16_lo(vs2t, v1_16);
				vs2l = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2l, vs2);
				vs2l = fmuld8ulx16_hi(vs2t, v1_16);
				vs2h = fmuld8sux16_hi(vs2t, v1_16);
				vs2l = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2l, vs2);
# endif
			}

			buf += SOVV;
			k -= n;

			vs1s = 0;
			if(likely(k >= SOVV)) do
			{
				unsigned long long vs2l_sum, vs2h_sum;
				unsigned long long vs2l, vs2h, vs2t;
				unsigned j;

# ifdef HAVE_VIS2
				j = k / SOVV > 32 ? 32 : k / SOVV;
# else
				j = k / SOVV > 16 ? 16 : k / SOVV;
# endif
				k -= j * SOVV;
				vs2l_sum = vs2h_sum = 0;
				do
				{
					/* add vs1 for this round */
					vs1s = fpadd32(vs1, vs1s);
					/* get input data */
					in8 = *(const unsigned long long *)buf;
					/* add 8 byte horizontal and add to old dword */
					vs1 = pdist(in8, v0, vs1);
					/* apply order, widen to 16 bit */
					vs2l = fmul8x16_lo(in8, scale_order_lo);
					vs2h = fmul8x16_hi(in8, scale_order_hi);
					vs2l_sum = fpadd16(vs2l, vs2l_sum);
					vs2h_sum = fpadd16(vs2h, vs2h_sum);
					buf += SOVV;
				} while(--j);

# ifdef HAVE_VIS2
				vs2l = fpand(vs2l_sum, 0x0000FFFF0000FFFFULL);
				vs2h = fpand(vs2h_sum, 0x0000FFFF0000FFFFULL);
				vs2t = fpadd32(vs2l, vs2h);
				vs2l = bshuffle(vs2l_sum, v0);
				vs2h = bshuffle(vs2h_sum, v0);
				vs2  = fpadd32(vs2t, vs2);
				vs2t = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2t, vs2);
# else
				vs2t = vs2l_sum;
				/* add horizontal */
				vs2l = fmuld8ulx16_lo(vs2t, v1_16);
				vs2h = fmuld8sux16_lo(vs2t, v1_16);
				vs2l = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2l, vs2);
				vs2l = fmuld8ulx16_hi(vs2t, v1_16);
				vs2h = fmuld8sux16_hi(vs2t, v1_16);
				vs2l = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2l, vs2);

				vs2t = vs2h_sum;
				/* add horizontal */
				vs2l = fmuld8ulx16_lo(vs2t, v1_16);
				vs2h = fmuld8sux16_lo(vs2t, v1_16);
				vs2l = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2l, vs2);
				vs2l = fmuld8ulx16_hi(vs2t, v1_16);
				vs2h = fmuld8sux16_hi(vs2t, v1_16);
				vs2l = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2l, vs2);
# endif
			} while (k >= SOVV);
			vs1s = fpadd32(vs1s, vs1s); /* *2 */
			vs1s = fpadd32(vs1s, vs1s); /* *4 */
			vs1s = fpadd32(vs1s, vs1s); /* *8 */
			vs2  = fpadd32(vs2, vs1s);

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
# ifdef HAVE_VIS2
				vs2l = fpand(vs2t, 0x0000FFFF0000FFFFULL);
				vs2h = bshuffle(vs2t, v0);
				vs2t = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2t, vs2);
# else
				vs2l = fmuld8ulx16_lo(vs2t, v1_16);
				vs2h = fmuld8sux16_lo(vs2t, v1_16);
				vs2l = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2l, vs2);
				vs2l = fmuld8ulx16_hi(vs2t, v1_16);
				vs2h = fmuld8sux16_hi(vs2t, v1_16);
				vs2l = fpadd32(vs2l, vs2h);
				vs2  = fpadd32(vs2l, vs2);
# endif

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
		reduce_full(s1);
		reduce_full(s2);
	}

	return s2 << 16 | s1;
}

static char const rcsid_a32sparc[] GCC_ATTR_USED_VAR = "$Id: $";
#endif
/* EOF */
