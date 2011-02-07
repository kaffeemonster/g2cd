/*
 * strncasecmp_a.c
 * strncasecmp ascii only, alpha implementation
 *
 * Copyright (c) 2009-2011 Jan Seiffert
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

#include "alpha.h"

static noinline int strncasecmp_a_u(const char *s1, const char *s2, size_t n)
{
	size_t i, j, cycles;

LOOP_AGAIN:
	i = ALIGN_DIFF(s1, 4096);
	i = i ? i : 4096;
	j = ALIGN_DIFF(s2, 4096);
	j = j ? j : i;
	i = i < j ? i : j;
	j = ROUND_ALIGN(n, 2 * SOUL);
	i = i < j ? i : j;

	cycles = i;
	if(likely(i >= 2 * SOUL))
	{
		const unsigned long *s1_l, *s2_l;
		unsigned long w1, w2, w1_x, w2_x;
		unsigned long m1, m2;
		unsigned shift11, shift12, shift21, shift22;

		shift11  = (unsigned)ALIGN_DOWN_DIFF(s1, SOUL);
		shift12  = SOUL - shift11;
		shift11 *= BITS_PER_CHAR;
		shift12 *= BITS_PER_CHAR;
		shift21  = (unsigned)ALIGN_DOWN_DIFF(s2, SOUL);
		shift22  = SOUL - shift21;
		shift21 *= BITS_PER_CHAR;
		shift22 *= BITS_PER_CHAR;

		s1_l = (const unsigned long *)ALIGN_DOWN(s1, SOUL);
		s2_l = (const unsigned long *)ALIGN_DOWN(s2, SOUL);

		w1_x = *s1_l++;
		w2_x = *s2_l++;
		for(; likely(i >= 2 * SOUL); i -= SOUL)
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

			m1   = cmpbge(0x6060606060606060UL, w1);
			m1  |= cmpbge(w1, 0x7b7b7b7b7b7b7b7bUL);
			m1   = zapnot(0x2020202020202020UL, m1);
			w1  -= m1;
			m2   = cmpbge(0x6060606060606060UL, w2);
			m2  |= cmpbge(w2, 0x7b7b7b7b7b7b7b7bUL);
			m2   = zapnot(0x2020202020202020UL, m2);
			w2  -= m2;
			m1   = w1 ^ w2;
			m2   = cmpbeqz(w1);
			if(m1 || m2)
			{
				unsigned long r1, r2;
				m1 = cmpbeqz(m1);
				r1 = alpha_nul_byte_index_e(m1);
				r2 = alpha_nul_byte_index_e(m2);
				r1 = r1 < r2 ? r1 : r2;
				cycles = ROUND_TO(cycles - i, SOUL);
				n -= cycles;
				r1 = r1 < n - 1 ? r1 : n - 1;
				if(HOST_IS_BIGENDIAN)
					r1 = SOULM1 - r1;
				r1 *= BITS_PER_CHAR;
				return (int)((w1 >> r1) & 0xFF) - (int)((w2 >> r1) & 0xFF);
			}
		}
	}
	cycles = ROUND_TO(cycles - i, SOUL);
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
	n -= i;

	for(; i; i--, s1++, s2++)
	{
		unsigned c1 = *(const unsigned char *)s1, c2 = *(const unsigned char *)s2;
		/*
		 * GCC can turn these compares into jumpless arith.
		 * So save Alpha from byte access into an lookup table
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
	unsigned long w1, w2;
	unsigned long m1, m2;
	size_t i, cycles;
	unsigned shift;

	shift = ALIGN_DOWN_DIFF(s1, SOUL);
	s1 = (const char *)ALIGN_DOWN(s1, SOUL);
	s2 = (const char *)ALIGN_DOWN(s2, SOUL);

	w1   = *(const unsigned long *)s1;
	w2   = *(const unsigned long *)s2;
	s1  += SOUL;
	s2  += SOUL;
	m1   = cmpbge(0x6060606060606060UL, w1);
	m1  |= cmpbge(w1, 0x7b7b7b7b7b7b7b7bUL);
	m1   = zapnot(0x2020202020202020UL, m1);
	w1  -= m1;
	m2   = cmpbge(0x6060606060606060UL, w2);
	m2  |= cmpbge(w2, 0x7b7b7b7b7b7b7b7bUL);
	m2   = zapnot(0x2020202020202020UL, m2);
	w2  -= m2;
	m1   = w1 ^ w2;
	m2   = cmpbeqz(w1);
	if(!HOST_IS_BIGENDIAN) {
		m1 >>= shift * BITS_PER_CHAR;
		m2 >>= shift;
	} else {
		m1 <<= shift * BITS_PER_CHAR;
		m2 <<= shift + SOULM1 * BITS_PER_CHAR;
	}
	if(m1 || m2)
	{
		unsigned long r1, r2;
		m1 = cmpbeqz(m1);
		if(!HOST_IS_BIGENDIAN)
			m1 <<= shift + SOULM1 * BITS_PER_CHAR;
		else
			m1 >>= shift;
		r1 = alpha_nul_byte_index_e(m1);
		r2 = alpha_nul_byte_index_e(m2);
		r1 = r1 < r2 ? r1 : r2;
		r1 = r1 < n - 1 ? r1 : n - 1;
		if(HOST_IS_BIGENDIAN)
			r1 = SOULM1 - r1;
		r1 *= BITS_PER_CHAR;
		return (int)((w1 >> r1) & 0xFF) - (int)((w2 >> r1) & 0xFF);
	}
	n -= SOUL - shift;
	i  = ROUND_ALIGN(n, SOUL);
	cycles = i;

	for(; likely(SOUL <= i); i -= SOUL)
	{
		w1   = *(const unsigned long *)s1;
		w2   = *(const unsigned long *)s2;
		s1  += SOUL;
		s2  += SOUL;
		m1   = cmpbge(0x6060606060606060UL, w1);
		m1  |= cmpbge(w1, 0x7b7b7b7b7b7b7b7bUL);
		m1   = zapnot(0x2020202020202020UL, m1);
		w1  -= m1;
		m2   = cmpbge(0x6060606060606060UL, w2);
		m2  |= cmpbge(w2, 0x7b7b7b7b7b7b7b7bUL);
		m2   = zapnot(0x2020202020202020UL, m2);
		w2  -= m2;
		m1   = w1 ^ w2;
		m2   = cmpbeqz(w1);
		if(m1 || m2)
		{
			unsigned long r1, r2;
			m1 = cmpbeqz(m1);
			r1 = alpha_nul_byte_index_e(m1);
			r2 = alpha_nul_byte_index_e(m2);
			r1 = r1 < r2 ? r1 : r2;
			cycles -= i;
			n -= cycles;
			r1 = r1 < n - 1 ? r1 : n - 1;
			if(HOST_IS_BIGENDIAN)
				r1 = SOULM1 - r1;
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

	if(likely(((intptr_t)s1 ^ (intptr_t)s2) & SOULM1))
		return strncasecmp_a_u(s1, s2, n);
	return strncasecmp_a_a(s1, s2, n);
}

static char const rcsid_snccaa[] GCC_ATTR_USED_VAR = "$Id: $";
