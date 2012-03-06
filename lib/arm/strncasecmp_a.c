/*
 * strncasecmp_a.c
 * strncasecmp ascii only, arm implementation
 *
 * Copyright (c) 2012 Jan Seiffert
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

#include "my_neon.h"

#if defined(ARM_DSP_SANE)
static noinline int strncasecmp_a_u(const char *s1, const char *s2, size_t n)
{
	size_t i = n;

LOOP_AGAIN:
#if 0
	/*
	 * ARM has to few register for all this aligment correction stuff and
	 * the magic compares.
	 * If we have some kind of CAN_UNALIGNED this code could be used
	 * (without the shifting magic).
	 */
	, j, cycles;
	i = ALIGN_DIFF(s1, 4096);
	i = i ? i : 4096;
	j = ALIGN_DIFF(s2, 4096);
	j = j ? j : i;
	i = i < j ? i : j;
	j = ROUND_ALIGN(n, 2 * SOST);
	i = i < j ? i : j;

	cycles = i;
	if(likely(i >= 2 * SOST))
	{
		const size_t *s1_l, *s2_l;
		size_t w1, w2, w1_x, w2_x;
		size_t m1, m2;
		unsigned shift11, shift12, shift21, shift22;

		shift11  = (unsigned)ALIGN_DOWN_DIFF(s1, SOST);
		shift12  = SOST - shift11;
		shift11 *= BITS_PER_CHAR;
		shift12 *= BITS_PER_CHAR;
		shift21  = (unsigned)ALIGN_DOWN_DIFF(s2, SOST);
		shift22  = SOST - shift21;
		shift21 *= BITS_PER_CHAR;
		shift22 *= BITS_PER_CHAR;

		s1_l = (const size_t *)ALIGN_DOWN(s1, SOST);
		s2_l = (const size_t *)ALIGN_DOWN(s2, SOST);

		w1_x = *s1_l++;
		w2_x = *s2_l++;
		for(; likely(i >= 2 * SOST); i -= SOST)
		{
			w1   = w1_x;
			w2   = w2_x;
			w1_x = *s1_l++;
			w2_x = *s2_l++;
			if(HOST_IS_BIGENDIAN) {
				w1 = w1 << shift11 | w1_x >> shift12;
				w2 = w2 << shift21 | w2_x >> shift22;
			} else {
				w1 = w1 >> shift11 | w1_x << shift12;
				w2 = w2 >> shift21 | w2_x << shift22;
			}

			m1   = alu_usub8(w1, MK_C(0x20202020UL));
			w1   = alu_ucmp_between(w1, MK_C(0x60606060), MK_C(0x7b7b7b7b), w1, m1);
			m2   = alu_usub8(w2, MK_C(0x20202020UL));
			w2   = alu_ucmp_between(w2, MK_C(0x60606060), MK_C(0x7b7b7b7b), w2, m2);
			m1   = w1 ^ w2;
			m2   = alu_ucmp_eqz_msk(w1) & ACMP_MSK;
			if(m1 || m2)
			{
				size_t r1;
				/* invert mask to make non matching char stick out */
				m1  = (alu_ucmp_eqz_msk(m1) & ACMP_MSK) ^ ACMP_MSK;
				/* add Nul-byte mask on top */
				m1 |= m2;
				r1 = arm_nul_byte_index_e(m1);
				cycles = ROUND_TO(cycles - i, SOST);
				n -= cycles;
				r1 = r1 < n - 1 ? r1 : n - 1;
				if(HOST_IS_BIGENDIAN)
					r1 = SOSTM1 - r1;
				r1 *= BITS_PER_CHAR;
				return (int)((w1 >> r1) & 0xFF) - (int)((w2 >> r1) & 0xFF);
			}
		}
	}
	cycles = ROUND_TO(cycles - i, SOST);
	if(cycles >= n)
		return 0;
	n  -= cycles;
	s1 += cycles;
	s2 += cycles;

	i  = ALIGN_DIFF(s1, 4096);
	i  = i ? i : 4096;
	j  = ALIGN_DIFF(s2, 4096);
	j  = j ? j : i;
	i  = i < j ? i : j;
	i  = i < n ? i : n;
#endif
	n -= i;

	for(; i; i--, s1++, s2++)
	{
		unsigned c1 = *(const unsigned char *)s1, c2 = *(const unsigned char *)s2;
		/*
		 * GCC can turn these compares into jumpless arith.
		 */
		c1 -= c1 >= 'a' && c1 <= 'z' ? 0x20 : 0;
		c2 -= c2 >= 'a' && c2 <= 'z' ? 0x20 : 0;
		if(!(c1 && c1 == c2))
			return (int)c1 - (int)c2;
	}

	if(n)
		goto LOOP_AGAIN;
	return 0;
}

static noinline int strncasecmp_a_a(const char *s1, const char *s2, size_t n)
{
	size_t w1, w2;
	size_t m1, m2;
	size_t i, cycles;
	unsigned shift;

	shift = ALIGN_DOWN_DIFF(s1, SOST);
	s1 = (const char *)ALIGN_DOWN(s1, SOST);
	s2 = (const char *)ALIGN_DOWN(s2, SOST);

	w1   = *(const size_t *)s1;
	w2   = *(const size_t *)s2;
	s1  += SOST;
	s2  += SOST;
	m1   = alu_usub8(w1, MK_C(0x20202020UL));
	w1   = alu_ucmp_between(w1, MK_C(0x60606060), MK_C(0x7b7b7b7b), w1, m1);
	m2   = alu_usub8(w2, MK_C(0x20202020UL));
	w2   = alu_ucmp_between(w2, MK_C(0x60606060), MK_C(0x7b7b7b7b), w2, m2);
	if(shift)
	{
		if(!HOST_IS_BIGENDIAN) {
			w1 |= (~0ul) >> ((SOST - shift) * BITS_PER_CHAR);
			w2 |= (~0ul) >> ((SOST - shift) * BITS_PER_CHAR);
		} else {
			w1 |= (~0ul) << ((SOST - shift) * BITS_PER_CHAR);
			w2 |= (~0ul) << ((SOST - shift) * BITS_PER_CHAR);
		}
	}
	m1   = w1 ^ w2;
	m2   = alu_ucmp_eqz_msk(w1) & ACMP_MSK;
	if(m1 || m2)
	{
		size_t r1;
		/* invert mask to make the non matching character stick out */
		m1  = (alu_ucmp_eqz_msk(m1) & ACMP_MSK) ^ ACMP_MSK;
		/* add Nul-byte msk into it */
		m1 |= m2;
		r1  = arm_nul_byte_index_e(m1);
		r1 = r1 < n - 1 + shift ? r1 : n - 1 + shift;
		if(HOST_IS_BIGENDIAN)
			r1 = SOSTM1 - r1;
		r1 *= BITS_PER_CHAR;
		return (int)((w1 >> r1) & 0xFF) - (int)((w2 >> r1) & 0xFF);
	}
	n -= SOST - shift;
	i  = ROUND_ALIGN(n, SOST);
	cycles = i;

	for(; likely(SOST >= i); i -= SOST)
	{
		w1   = *(const size_t *)s1;
		w2   = *(const size_t *)s2;
		s1  += SOST;
		s2  += SOST;
		m1   = alu_usub8(w1, MK_C(0x20202020UL));
		w1   = alu_ucmp_between(w1, MK_C(0x60606060), MK_C(0x7b7b7b7b), w1, m1);
		m2   = alu_usub8(w2, MK_C(0x20202020UL));
		w2   = alu_ucmp_between(w2, MK_C(0x60606060), MK_C(0x7b7b7b7b), w2, m2);
		m1   = w1 ^ w2;
		m2   = alu_ucmp_eqz_msk(w1) & ACMP_MSK;
		if(m1 || m2)
		{
			size_t r1;
			/* invert mask to make the non matching char stick out */
			m1 = (alu_ucmp_eqz_msk(m1) & ACMP_MSK) ^ ACMP_MSK;
			/* add Nul-byte msk into it */
			r1 = arm_nul_byte_index_e(m1);
			cycles -= i;
			n -= cycles;
			r1 = r1 < n - 1 ? r1 : n - 1;
			if(HOST_IS_BIGENDIAN)
				r1 = SOSTM1 - r1;
			r1 *= BITS_PER_CHAR;
			return (int)((w1 >> r1) & 0xFF) - (int)((w2 >> r1) & 0xFF);
		}
	}
	return 0;
}

int strncasecmp_a(const char *s1, const char *s2, size_t n)
{
	if(unlikely(!n))
		return 0;

	prefetch(s1);
	prefetch(s2);

	if(likely(((intptr_t)s1 ^ (intptr_t)s2) & SOSTM1))
		return strncasecmp_a_u(s1, s2, n);
	return strncasecmp_a_a(s1, s2, n);
}

static char const rcsid_snccaar[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/strncasecmp_a.c"
#endif
