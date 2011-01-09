/*
 * mempopcnt.c
 * popcount a mem region, mips implementation
 *
 * Copyright (c) 2010-2011 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id: $
 */

#include "my_mips.h"


#if defined(__mips_dsp) || defined(__mips_loongson_vector_rev)
typedef a64 sumt_t;
#else
typedef check_t sumt_t;
#endif

static inline check_t popcountst_int1(check_t n)
{
	n -= (n & MK_CC(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_CC(0x33333333L)) + (n & MK_CC(0x33333333L));
	n = ((n >> 4) + n) & MK_CC(0x0f0f0f0fL);
#ifdef HAVE_HW_MULT
	n = (n * MK_CC(0x01010101L)) >> (CHECK_T_BITS - 8);
#else
	n = ((n >> 8) + n);
	n = ((n >> 16) + n);
	if(CHECK_T_BITS >= 64)
		n = ((n >> 32) + n);
	n &= 0xff;
#endif
	return n;
}

static inline check_t popcountst_int2(check_t n, check_t m)
{
	n -= (n & MK_CC(0xaaaaaaaaL)) >> 1;
	m -= (m & MK_C(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_CC(0x33333333L)) + (n & MK_CC(0x33333333L));
	m = ((m >> 2) & MK_CC(0x33333333L)) + (m & MK_CC(0x33333333L));
	n += m;
	n = ((n >> 4) & MK_CC(0x0f0f0f0fL)) + (n & MK_CC(0x0f0f0f0fL));
#ifdef HAVE_HW_MULT
	n = (n * MK_CC(0x01010101L)) >> (CHECK_T_BITS - 8);
#else
	n = ((n >> 8) + n);
	n = ((n >> 16) + n);
	if(CHECK_T_BITS >= 64)
		n = ((n >> 32) + n);
	n &= 0xff;
#endif
	return n;
}

static always_inline check_t popcountst_int4(check_t n, check_t m, check_t o, check_t p)
{
	n -= (n & MK_CC(0xaaaaaaaaL)) >> 1;
	m -= (m & MK_CC(0xaaaaaaaaL)) >> 1;
	o -= (o & MK_CC(0xaaaaaaaaL)) >> 1;
	p -= (p & MK_CC(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_CC(0x33333333L)) + (n & MK_CC(0x33333333L));
	m = ((m >> 2) & MK_CC(0x33333333L)) + (m & MK_CC(0x33333333L));
	o = ((o >> 2) & MK_CC(0x33333333L)) + (o & MK_CC(0x33333333L));
	p = ((p >> 2) & MK_CC(0x33333333L)) + (p & MK_CC(0x33333333L));
	n += m;
	o += p;
	n = ((n >> 4) & MK_CC(0x0f0f0f0fL)) + (n & MK_CC(0x0f0f0f0fL));
	o = ((o >> 4) & MK_CC(0x0f0f0f0fL)) + (o & MK_CC(0x0f0f0f0fL));
#ifdef HAVE_HW_MULT
	if(CHECK_T_BITS >= 64) {
		n = (n * MK_CC(0x01010101L)) >> (CHECK_T_BITS - 8);
		o = (o * MK_CC(0x01010101L)) >> (CHECK_T_BITS - 8);
		n += o;
	} else {
		n += o;
		n = (n * MK_CC(0x01010101L)) >> (CHECK_T_BITS - 8);
	}
#else
	n += o;
	n = ((n >> 8) + n);
	n = ((n >> 16) + n);
	if(CHECK_T_BITS >= 64) {
		n &= MK_CC(0x000000ffL);
		n = ((n >> 32) + n);
	}
	n &= 0x1ff;
#endif
	return n;
}

static inline check_t popcountst_b(check_t n)
{
	n -= (n & MK_CC(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_CC(0x33333333L)) + (n & MK_CC(0x33333333L));
	n = ((n >> 4) + n) & MK_CC(0x0f0f0f0fL);
	return n;
}

static inline sumt_t sideways_add(sumt_t sum, check_t x)
{
#ifdef __mips_loongson_vector_rev
	a64 t;
	asm(
		"biadd	%1, %2\n\t"
		"paddd	%0, %0, %1\n\t"
	: /* %0 */ "=f" (sum),
	  /* %1 */ "=f" (t)
	: /* %2 */ "f" ((a64)x),
	  /* %3 */ "0" (sum)
	);
	return sum;
#elif defined(__mips_dsp)
	asm(
		"dpau.h.qbl	%q0, %1, %2\n\t"
		"dpau.h.qbr	%q0, %1, %2\n\t"
# if MY_MIPS_IS_64 == 1
		"dsrl32	%1, %1, 0\n\t"
		"dpau.h.qbl	%q0, %1, %2\n\t"
		"dpau.h.qbr	%q0, %1, %2\n\t"
# endif
	: /* %0 */ "=a" (sum),
	  /* %1 */ "=r" (x)
	: /* %2 */ "r" (0x01010101),
	  /* %3 */ "0" (sum),
	  /* %4 */ "1" (x)
	);
	return sum;
#else
	static const check_t mask = MK_CC(0x00ff00ffL);
	x = ((x & ~mask) >>  8) + (x & mask);
# ifdef HAVE_HW_MULT
	x = (x * MK_CC(0x00010001L)) >> (CHECK_T_BITS - 16);
# else
	x = (x >> 16) + x;
	if(CHECK_T_BITS >= 64)
		x = (x >> 32) + x;
	x &= 0x7ff;
# endif
	return sum + x;
#endif
}

size_t mempopcnt(const void *s, size_t len)
{
	const check_t *p;
	check_t r, sum = 0;
	unsigned shift = ALIGN_DOWN_DIFF(s, SOCT);
	prefetch(s);

	p = (const check_t *)ALIGN_DOWN(s, SOCT);
	r = *p;
	if(!HOST_IS_BIGENDIAN)
		r >>= shift * BITS_PER_CHAR;
	else
		r <<= shift * BITS_PER_CHAR;
	if(len >= SOCT || len + shift >= SOCT)
	{
		/*
		 * Sometimes you need a new perspective, like the altivec
		 * way of handling things.
		 * Lower address bits? Totaly overestimated.
		 *
		 * We don't precheck for alignment.
		 * Instead we "align hard", do one load "under the address",
		 * mask the excess info out and afterwards we are fine to go.
		 */
		p++;
		len -= SOCT - shift;
		sum += popcountst_int1(r);

		if(len >= SOCT * 8)
		{
			check_t ones, twos, fours, eights;
			sumt_t sum_t;

			sum_t = fours = twos = ones = 0;
			while(len >= SOCT * 8)
			{
				check_t sumb = 0;

				r    = len / (SOCT * 8);
// TODO: 31 rounds till sumb overflows is a guess...
				r    = r > 31 ? 31 : r;
				len -= r * SOCT * 8;
				/*
				 * popcnt instructions, even if nice, are seldomly the fasted
				 * instructions. And when you have to do it by "sideways"
				 * addition, you are screwed (lots of stalls, even if we try to
				 * leverage this by taking several at once, but this needs regs).
				 *
				 * There is another nice trick: compression (Harley's Method).
				 * We shove several words (8) into one word and count that (its
				 * like counting the carry of that). With some simple bin ops and
				 * a few regs you can get a lot better (+33%) then our "unrolled"
				 * approuch. (this is even a win on x86, with too few register
				 * and no 3 operand asm).
				 */
				for(; r; r--, p += 8)
				{
					check_t twos_l, twos_h, fours_l, fours_h;
// TODO: move this all to the MMX FPU on Loongson?
#define CSA(h,l, a,b,c) \
	{check_t u = a ^ b; check_t v = c; \
	 h = (a & b) | (u & v); l = u ^ v;}
					CSA(twos_l, ones, ones, p[0], p[1])
					CSA(twos_h, ones, ones, p[2], p[3])
					CSA(fours_l, twos, twos, twos_l, twos_h)
					CSA(twos_l, ones, ones, p[4], p[5])
					CSA(twos_h, ones, ones, p[6], p[7])
					CSA(fours_h, twos, twos, twos_l, twos_h)
					CSA(eights, fours, fours, fours_l, fours_h)
#undef CSA
					sumb += popcountst_b(eights);
				}
				sum_t = sideways_add(sum_t, sumb);
			}
			sum += 8 * (check_t)sum_t + 4 * popcountst_int1(fours) +
			       2 * popcountst_int1(twos) + popcountst_int1(ones);
		}
		if(len >= SOCT * 4) {
			sum += popcountst_int4(p[0], p[1], p[2], p[3]);
			p += 4;
			len -= SOCT * 4;
		}
		if(len >= SOCT * 2) {
			sum += popcountst_int2(p[0], p[1]);
			p += 2;
			len -= SOCT * 2;
		}
		if(len >= SOCT) {
			sum += popcountst_int1(p[0]);
			p++;
			len -= SOCT;
		}
		if(len)
			r =*p;
	}
	if(len) {
		if(!HOST_IS_BIGENDIAN)
			r <<= (SOCT - len) * BITS_PER_CHAR;
		else
			r >>= (SOCT - len) * BITS_PER_CHAR;
		sum += popcountst_int1(r);
	}
	return (size_t)sum;
}

static char const rcsid_mpgm[] GCC_ATTR_USED_VAR = "$Id: $";
