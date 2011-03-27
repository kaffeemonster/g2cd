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

#define HAVE_ADLER32_VEC
static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len);
#define MIN_WORK 16

#include "../generic/adler32.c"

#include "ia64.h"

/* for safety, we reduce the rounds a little bit */
#define VNMAX (5*NMAX)

static inline unsigned long long vector_reduce(unsigned long long x)
{
	unsigned long long y;

	y = x & 0x0000FFFF0000FFFFull;
	x = (x >> 16) & 0x0000FFFF0000FFFFull;
	y = psub4(y, x);
	x = x << 4;
	x = x + y;
	return x;
}

static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1, s2;

	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;

	if(likely(len >= 4*SOULL))
	{
		unsigned long long scale_order_lo;
		unsigned long long scale_order_hi;
		unsigned long long vs1, vs2;
		unsigned long long in8;
		unsigned f, n, k;
		const uint8_t *o_buf;

		if(!HOST_IS_BIGENDIAN) {
			scale_order_lo = 0x0005000600070008ULL;
			scale_order_hi = 0x0001000200030004ULL;
		} else {
			scale_order_lo = 0x0004000300020001ULL;
			scale_order_hi = 0x0008000700060005ULL;
		}

		/* align hard down */
		f = (unsigned) ALIGN_DOWN_DIFF(buf, SOULL);
		o_buf = buf;
		buf = (const uint8_t *)ALIGN_DOWN(buf, SOULL);
		n = SOULL - f;

		/* add n times s1 to s2 for start round */
		s2 += s1 * n;

		k = len < VNMAX ? (unsigned)len : VNMAX;
		len -= k;

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

		if(likely(k >= 2*SOULL)) do
		{
			unsigned long long vs1l_s = 0, vs1h_s = 0;
			unsigned long long vs1l_sum = vs1, vs1h_sum = 0;
			unsigned long long vs2l_sum = 0, vs2h_sum = 0;
			unsigned long long a, b, c, x, y;
			unsigned j;

			j = k / 2*SOULL;
			k -= j * 2*SOULL;
			do
			{
				unsigned long long in8_1, in8_2;

				vs1l_s += vs1l_sum;
				vs1h_s += vs1h_sum;
				in8_1 = *(const unsigned long long *)buf;
				buf += SOULL;
				in8_2 = *(const unsigned long long *)buf;
				buf += SOULL;
				asm(
					"psad1	%4=%9, %8\n\t"
					"unpack1.l	%6=%8, %9;;\n\t"

					"add	%0=%0, %4\n\t"
					"unpack1.h	%7=%8, %9\n\t"
					"pmpy2.r	%3=%6, %10;;\n\t"

					"add	%1=%1, %3\n\t"
					"pmpy2.r	%5=%7, %11\n\t"
					"pmpy2.l	%4=%6, %10;;\n\t"

					"add	%2=%2, %5\n\t"
					"pmpy2.l	%3=%7, %11\n\t"
					"add	%1=%1, %4\n\t"
				: /* %0 */ "=&r" (vs1l_sum),
				  /* %1 */ "=&r" (vs2l_sum),
				  /* %2 */ "=&r" (vs2h_sum),
				  /* %3 */ "=&r" (a),
				  /* %4 */ "=&r" (b),
				  /* %5 */ "=&r" (c),
				  /* %6 */ "=&r" (x),
				  /* %7 */ "=&r" (y)
				: /* %8 */ "r" (0),
				  /* %9 */ "r" (in8_1),
				  /* %10 */ "r" (scale_order_lo),
				  /* %11 */ "r" (scale_order_hi),
				  /*  */ "0" (vs1l_sum),
				  /*  */ "1" (vs2l_sum),
				  /*  */ "2" (vs2h_sum)
				);
// TODO: melt into one asm
				vs2h_sum += a;
				asm(
					"psad1	%4=%9, %8\n\t"
					"unpack1.l	%6=%8, %9;;\n\t"

					"add	%0=%0, %4\n\t"
					"unpack1.h	%7=%8, %9\n\t"
					"pmpy2.r	%3=%6, %10;;\n\t"

					"add	%1=%1, %3\n\t"
					"pmpy2.r	%5=%7, %11\n\t"
					"pmpy2.l	%4=%6, %10;;\n\t"

					"add	%2=%2, %5\n\t"
					"pmpy2.l	%3=%7, %11\n\t"
					"add	%1=%1, %4\n\t"
				: /* %0 */ "=&r" (vs1h_sum),
				  /* %1 */ "=&r" (vs2l_sum),
				  /* %2 */ "=&r" (vs2h_sum),
				  /* %3 */ "=&r" (a),
				  /* %4 */ "=&r" (b),
				  /* %5 */ "=&r" (c),
				  /* %6 */ "=&r" (x),
				  /* %7 */ "=&r" (y)
				: /* %8 */ "r" (0),
				  /* %9 */ "r" (in8_2),
				  /* %10 */ "r" (scale_order_lo),
				  /* %11 */ "r" (scale_order_hi),
				  /*  */ "0" (vs1h_sum),
				  /*  */ "1" (vs2l_sum),
				  /*  */ "2" (vs2h_sum)
				);
				vs2h_sum += a;
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
			/* reduce round sum */
			vs1l_s = vector_reduce(vs1l_s);
			vs1h_s = vector_reduce(vs1h_s);
			/* add round sum *16 to vs2 */
			vs2 += (vs1l_s << 4) + (vs1h_s << 4);
			vs2 += vs2l_sum;
			vs2 += vs2h_sum;
// TODO: are the reduce placed right?
			vs2 = vector_reduce(vs2);
			vs1 = vector_reduce(vs1l_sum) + vector_reduce(vs1h_sum);
			len += k;
			k = len < VNMAX ? (unsigned)len : VNMAX;
			len -= k;
		} while(likely(k >= 2*SOULL));

		if(k >= SOULL)
		{
			unsigned long long a, b, c, x, y;

			in8 = *(const unsigned long long *)buf;
			asm(
				"shladd	%1=%0,3,%1\n\t"
				"psad1	%3=%8, %7\n\t"
				"unpack1.l	%5=%7, %8;;\n\t"

				"add	%0=%0, %3\n\t"
				"unpack1.h	%6=%7, %8\n\t"
				"pmpy2.r	%2=%5, %9;;\n\t"

				"add	%1=%1, %2\n\t"
				"pmpy2.r	%4=%6, %10\n\t"
				"pmpy2.l	%3=%5, %9;;\n\t"

				"add	%1=%1, %4\n\t"
				"pmpy2.l	%3=%6, %10;;\n\t"
				"add	%1=%1, %3\n\t"
			: /* %0 */ "=&r" (vs1),
			  /* %1 */ "=&r" (vs2),
			  /* %2 */ "=&r" (a),
			  /* %3 */ "=&r" (b),
			  /* %4 */ "=&r" (c),
			  /* %5 */ "=&r" (x),
			  /* %6 */ "=&r" (y)
			: /* %7 */ "r" (0),
			  /* %8 */ "r" (in8),
			  /* %9 */ "r" (scale_order_lo),
			  /* %10 */ "r" (scale_order_hi),
			  /*  */ "0" (vs1),
			  /*  */ "1" (vs2)
			);
			vs2 += a;
			buf += SOULL;
			k   -= SOULL;
		}

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

	if(unlikely(len)) do {
		s1 += *buf++;
		s2 += s1;
	} while (--len);
	reduce_x(s1);
	reduce_x(s2);
	return s2 << 16 | s1;
}

static char const rcsid_a32ia64[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
