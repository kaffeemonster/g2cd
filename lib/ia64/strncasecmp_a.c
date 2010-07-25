/*
 * strncasecmp_a.c
 * strncasecmp ascii only, ia64 implementation
 *
 * Copyright (c) 2010 Jan Seiffert
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

#include "ia64.h"

static noinline int strncasecmp_a_u(const char *s1, const char *s2, size_t n)
{
	static const unsigned char tab[256] =
	{
	/*	  0     1     2     3     4     5     6     7         */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 07 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0F */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 17 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 1F */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 27 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 2F */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 37 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 3F */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 47 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 4F */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 57 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 5F */
		0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, /* 67 */
		0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, /* 6F */
		0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, /* 77 */
		0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, /* 7F */
	};
	size_t i, j, cycles;

LOOP_AGAIN:
	i = ALIGN_DIFF(s1, 4096);
	i = i ? i : 4096;
	j = ALIGN_DIFF(s2, 4096);
	j = j ? j : i;
	i = i < j ? i : j;
	j = ROUND_ALIGN(n, 2 * SOULL);
	i = i < j ? i : j;

	cycles = i;
	if(likely(i >= 2 * SOULL))
	{
		const unsigned long long *s1_l, *s2_l;
		unsigned long long w1, w2, w1_x, w2_x;
		unsigned long long m1, m2;
		unsigned shift11, shift12, shift21, shift22;

		shift11  = (unsigned)ALIGN_DOWN_DIFF(s1, SOULL);
		shift12  = SOULL - shift11;
		shift11 *= BITS_PER_CHAR;
		shift12 *= BITS_PER_CHAR;
		shift21  = (unsigned)ALIGN_DOWN_DIFF(s2, SOULL);
		shift22  = SOULL - shift21;
		shift21 *= BITS_PER_CHAR;
		shift22 *= BITS_PER_CHAR;

		s1_l = (const unsigned long long *)ALIGN_DOWN(s1, SOULL);
		s2_l = (const unsigned long long *)ALIGN_DOWN(s2, SOULL);

		w1_x = *s1_l++;
		w2_x = *s2_l++;
		for(; likely(i >= 2 * SOULL); i -= SOULL)
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

			m1   = pcmp1gt(w1, 0x6060606060606060ULL);
			m1  ^= pcmp1gt(w1, 0x7a7a7a7a7a7a7a7aULL);
			m1   = 0x2020202020202020UL & m1;
			w1  -= m1;
			m2   = pcmp1gt(w2, 0x6060606060606060ULL);
			m2  ^= pcmp1gt(w2, 0x7a7a7a7a7a7a7a7aULL);
			m2   = 0x2020202020202020UL & m2;
			w2  -= m2;
			m1   = czx1(w1);
			m2   = czx1(w2);
			m2   = m1 < m2 ? m1 : m2;
			m1   = w1 ^ w2;
			if(m1 || m2 < SOULL)
			{
				m1 = czx1(pcmp1eq(m1, 0));
				m1 = m1 < m2 ? m1 : m2;
				cycles = ROUND_TO(cycles - i, SOULL);
				cycles -= i;
				n -= cycles;
				m1 = m1 < n - 1 ? m1 : n - 1;
				if(HOST_IS_BIGENDIAN)
					m1 = SOULLM1 - m1;
				m1 *= BITS_PER_CHAR;
				return (int)((w1 >> m1) & 0xFF) - (int)((w2 >> m1) & 0xFF);
			}
		}
	}
	cycles = ROUND_TO(cycles - i, SOULL);
	if(cycles >= n)
		return 0;
	n  -= cycles;
	s1 += cycles;
	s2 += cycles;

	i = ALIGN_DIFF(s1, 4096);
	i = i ? i : 4096;
	j = ALIGN_DIFF(s2, 4096);
	j = j ? j : i;
	i = i < j ? i : j;
	i = i < n ? i : n;

	for(; i; i--, n--)
	{
		unsigned c1 = (unsigned) *s1++, c2 = (unsigned) *s2++;
		c1 -= tab[c1];
		c2 -= tab[c2];
		if(!(c1 && c2 && c1 == c2))
			return (int)c1 - (int)c2;
	}

	if(n)
		goto LOOP_AGAIN;

	return 0;
}

static noinline int strncasecmp_a_a(const char *s1, const char *s2, size_t n)
{
	unsigned long long w1, w2;
	unsigned long long m1, m2;
	size_t i, cycles;
	unsigned shift;

	shift = ALIGN_DOWN_DIFF(s1, SOULL);
	s1 = (const char *)ALIGN_DOWN(s1, SOULL);
	s2 = (const char *)ALIGN_DOWN(s2, SOULL);

	w1   = *(const unsigned long long *)s1;
	w2   = *(const unsigned long long *)s2;
	s1  += SOULL;
	s2  += SOULL;
	m1   = pcmp1gt(w1, 0x6060606060606060ULL);
	m1  ^= pcmp1gt(w1, 0x7a7a7a7a7a7a7a7aULL);
	m1   = 0x2020202020202020UL & m1;
	w1  -= m1;
	m2   = pcmp1gt(w2, 0x6060606060606060ULL);
	m2  ^= pcmp1gt(w2, 0x7a7a7a7a7a7a7a7aULL);
	m2   = 0x2020202020202020UL & m2;
	w2  -= m2;
	if(!HOST_IS_BIGENDIAN) {
		w1 |= (~0ULL) >> ((SOULL - shift) * BITS_PER_CHAR);
		w2 |= (~0ULL) >> ((SOULL - shift) * BITS_PER_CHAR);
	} else {
		w1 |= (~0ULL) << ((SOULL - shift) * BITS_PER_CHAR);
		w2 |= (~0ULL) << ((SOULL - shift) * BITS_PER_CHAR);
	}
	m1   = czx1(w1);
	m2   = czx1(w2);
	m2   = m1 < m2 ? m1 : m2;
	m1   = w1 ^ w2;
	if(m1 || m2 < SOULL)
	{
			m1 = czx1(pcmp1eq(m1, 0));
			m1 = m1 < m2 ? m1 : m2;
			m1 = m1 < n - 1 ? m1 : n - 1;
			if(HOST_IS_BIGENDIAN)
				m1 = SOULLM1 - m1;
			m1 *= BITS_PER_CHAR;
			return (int)((w1 >> m1) & 0xFF) - (int)((w2 >> m1) & 0xFF);
	}
	n -= SOULL - shift;
	i  = ROUND_ALIGN(n, SOULL);
	cycles = i;

	for(; likely(SOULL <= i); i -= SOULL)
	{
		w1   = *(const unsigned long long *)s1;
		w2   = *(const unsigned long long *)s2;
		s1  += SOULL;
		s2  += SOULL;
		m1   = pcmp1gt(w1, 0x6060606060606060ULL);
		m1  ^= pcmp1gt(w1, 0x7a7a7a7a7a7a7a7aULL);
		m1   = 0x2020202020202020UL & m1;
		w1  -= m1;
		m2   = pcmp1gt(w2, 0x6060606060606060ULL);
		m2  ^= pcmp1gt(w2, 0x7a7a7a7a7a7a7a7aULL);
		m2   = 0x2020202020202020UL & m2;
		w2  -= m2;
		m1   = czx1(w1);
		m2   = czx1(w2);
		m2   = m1 < m2 ? m1 : m2;
		m1   = w1 ^ w2;
		if(m1 || m2 < SOULL)
		{
			m1 = czx1(pcmp1eq(m1, 0));
			m1 = m1 < m2 ? m1 : m2;
			cycles -= i;
			n -= cycles;
			m1 = m1 < n - 1 ? m1 : n - 1;
			if(HOST_IS_BIGENDIAN)
				m1 = SOULLM1 - m1;
			m1 *= BITS_PER_CHAR;
			return (int)((w1 >> m1) & 0xFF) - (int)((w2 >> m1) & 0xFF);
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

	if(likely(((intptr_t)s1 ^ (intptr_t)s2) & SOULLM1))
		return strncasecmp_a_u(s1, s2, n);
	return strncasecmp_a_a(s1, s2, n);
}

static char const rcsid_snccaia[] GCC_ATTR_USED_VAR = "$Id: $";
