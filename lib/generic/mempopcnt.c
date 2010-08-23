/*
 * mempopcnt.c
 * popcount a mem region, generic implementation
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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

#if !defined(NO_GEN_POPER) && (!defined(ARCH_NAME_SUFFIX) || defined(NEED_GEN_POPER))
static inline size_t popcountst_int1(size_t n)
{
	n -= (n & MK_C(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_C(0x33333333L)) + (n & MK_C(0x33333333L));
	n = ((n >> 4) + n) & MK_C(0x0f0f0f0fL);
# ifdef HAVE_HW_MULT
	n = (n * MK_C(0x01010101L)) >> (SIZE_T_BITS - 8);
# else
	n = ((n >> 8) + n);
	n = ((n >> 16) + n);
	if(SIZE_T_BITS >= 64)
		n = ((n >> 32) + n);
	n &= 0xff;
# endif
	return n;
}

static inline size_t popcountst_int2(size_t n, size_t m)
{
	n -= (n & MK_C(0xaaaaaaaaL)) >> 1;
	m -= (m & MK_C(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_C(0x33333333L)) + (n & MK_C(0x33333333L));
	m = ((m >> 2) & MK_C(0x33333333L)) + (m & MK_C(0x33333333L));
	n += m;
	n = ((n >> 4) & MK_C(0x0f0f0f0fL)) + (n & MK_C(0x0f0f0f0fL));
# ifdef HAVE_HW_MULT
	n = (n * MK_C(0x01010101L)) >> (SIZE_T_BITS - 8);
# else
	n = ((n >> 8) + n);
	n = ((n >> 16) + n);
	if(SIZE_T_BITS >= 64)
		n = ((n >> 32) + n);
	n &= 0xff;
# endif
	return n;
}

static always_inline size_t popcountst_int4(size_t n, size_t m, size_t o, size_t p)
{
	n -= (n & MK_C(0xaaaaaaaaL)) >> 1;
	m -= (m & MK_C(0xaaaaaaaaL)) >> 1;
	o -= (o & MK_C(0xaaaaaaaaL)) >> 1;
	p -= (p & MK_C(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_C(0x33333333L)) + (n & MK_C(0x33333333L));
	m = ((m >> 2) & MK_C(0x33333333L)) + (m & MK_C(0x33333333L));
	o = ((o >> 2) & MK_C(0x33333333L)) + (o & MK_C(0x33333333L));
	p = ((p >> 2) & MK_C(0x33333333L)) + (p & MK_C(0x33333333L));
	n += m;
	o += p;
	n = ((n >> 4) & MK_C(0x0f0f0f0fL)) + (n & MK_C(0x0f0f0f0fL));
	o = ((o >> 4) & MK_C(0x0f0f0f0fL)) + (o & MK_C(0x0f0f0f0fL));
# ifdef HAVE_HW_MULT
	if(SIZE_T_BITS >= 64) {
		n = (n * MK_C(0x01010101L)) >> (SIZE_T_BITS - 8);
		o = (o * MK_C(0x01010101L)) >> (SIZE_T_BITS - 8);
		n += o;
	} else {
		n += o;
		n = (n * MK_C(0x01010101L)) >> (SIZE_T_BITS - 8);
	}
# else
	n += o;
	n = ((n >> 8) + n);
	n = ((n >> 16) + n);
	if(SIZE_T_BITS >= 64) {
		n &= MK_C(0x000000ffL);
		n = ((n >> 32) + n);
	}
	n &= 0x1ff;
# endif
	return n;
}

static inline size_t popcountst_b(size_t n)
{
	n -= (n & MK_C(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_C(0x33333333L)) + (n & MK_C(0x33333333L));
	n = ((n >> 4) + n) & MK_C(0x0f0f0f0fL);
	return n;
}
#endif

#ifndef ARCH_NAME_SUFFIX
# define F_NAME(z, x, y) z x
#else
# define F_NAME(z, x, y) static z x##y
#endif

#ifndef NO_SIDEWAYS_ADD
static inline size_t sideways_add(size_t sum, size_t x)
{
# ifndef HAVE_FULL_POPCNT
	static const size_t mask = MK_C(0x00ff00ffL);
	x = ((x & ~mask) >>  8) + (x & mask);
#  ifdef HAVE_HW_MULT
	x = (x * MK_C(0x00010001L)) >> (SIZE_T_BITS - 16);
#  else
	x = (x >> 16) + x;
	if(SIZE_T_BITS >= 64)
		x = (x >> 32) + x;
	x &= 0x7ff;
#  endif
# endif
	return sum + x;
}
#endif

F_NAME(size_t, mempopcnt, _generic)(const void *s, size_t len)
{
	const size_t *p;
	size_t r;
	size_t sum = 0;
	unsigned shift = ALIGN_DOWN_DIFF(s, SOST);
	prefetch(s);

	p = (const size_t *)ALIGN_DOWN(s, SOST);
	r = *p;
	if(!HOST_IS_BIGENDIAN)
		r >>= shift * BITS_PER_CHAR;
	else
		r <<= shift * BITS_PER_CHAR;
	if(len >= SOST || len + shift >= SOST)
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
		len -= SOST - shift;
		sum += popcountst_int1(r);

		if(len >= SOST * 8)
		{
			size_t ones, twos, fours, eights, sum_t;

			sum_t = fours = twos = ones = 0;
			while(len >= SOST * 8)
			{
				size_t sumb = 0;

				r    = len / (SOST * 8);
#ifndef HAVE_FULL_POPCNT
// TODO: 31 rounds till sumb overflows is a guess...
				r    = r > 31 ? 31 : r;
#endif
				len -= r * SOST * 8;
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
					size_t twos_l, twos_h, fours_l, fours_h;

#define CSA(h,l, a,b,c) \
	{size_t u = a ^ b; size_t v = c; \
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
			sum += 8 * sum_t + 4 * popcountst_int1(fours) +
			       2 * popcountst_int1(twos) + popcountst_int1(ones);
		}
		if(len >= SOST * 4) {
			sum += popcountst_int4(p[0], p[1], p[2], p[3]);
			p += 4;
			len -= SOST * 4;
		}
		if(len >= SOST * 2) {
			sum += popcountst_int2(p[0], p[1]);
			p += 2;
			len -= SOST * 2;
		}
		if(len >= SOST) {
			sum += popcountst_int1(p[0]);
			p++;
			len -= SOST;
		}
		if(len)
			r =*p;
	}
	if(len) {
		if(!HOST_IS_BIGENDIAN)
			r <<= (SOST - len) * BITS_PER_CHAR;
		else
			r >>= (SOST - len) * BITS_PER_CHAR;
		sum += popcountst_int1(r);
	}
	return sum;
}
#undef F_NAME

static char const rcsid_mpg[] GCC_ATTR_USED_VAR = "$Id: $";
