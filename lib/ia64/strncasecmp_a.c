/*
 * strncasecmp_a.c
 * strncasecmp ascii only, ia64 implementation
 *
 * Copyright (c) 2010-2012 Jan Seiffert
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

#ifdef HAVE_TIMODE
typedef unsigned __uint128 __attribute__((mode(TI)));
#endif

static noinline int strncasecmp_a_u(const char *s1, const char *s2, size_t n)
{
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
		unsigned shift11, shift21;
#ifndef HAVE_TIMODE
		unsigned shift12, shift22;
#endif

		shift11  = (unsigned)ALIGN_DOWN_DIFF(s1, SOULL);
		shift11 *= BITS_PER_CHAR;
#ifndef HAVE_TIMODE
		shift12  = SOULL * BITS_PER_CHAR - shift11;
#else
		if(HOST_IS_BIGENDIAN)
			shift11  = SOULL * BITS_PER_CHAR - shift11;
#endif
		shift21  = (unsigned)ALIGN_DOWN_DIFF(s2, SOULL);
		shift21 *= BITS_PER_CHAR;
#ifndef HAVE_TIMODE
		shift22  = SOULL * BITS_PER_CHAR - shift21;
#else
		if(HOST_IS_BIGENDIAN)
			shift21  = SOULL * BITS_PER_CHAR - shift21;
#endif

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
#ifdef HAVE_TIMODE
				w1 = ((((__uint128_t)w1) << 64) | w1_x) >> shift11;
				w2 = ((((__uint128_t)w2) << 64) | w2_x) >> shift21;
#else
				w1 = w1 << shift11 | w1_x >> shift12;
				w2 = w2 << shift21 | w2_x >> shift22;
#endif
			} else {
#ifdef HAVE_TIMODE
				w1 = (w1 | (((__uint128_t)w1_x) << 64)) >> shift11;
				w2 = (w2 | (((__uint128_t)w2_x) << 64)) >> shift21;
#else
				w1 = w1 >> shift11 | w1_x << shift12;
				w2 = w2 >> shift21 | w2_x << shift22;
#endif
			}

			m1   = pcmp1gt(w1, 0x6060606060606060ULL);
			m1  ^= pcmp1gt(w1, 0x7a7a7a7a7a7a7a7aULL);
			m1   = 0x2020202020202020ULL & m1;
			w1  -= m1;
			m2   = pcmp1gt(w2, 0x6060606060606060ULL);
			m2  ^= pcmp1gt(w2, 0x7a7a7a7a7a7a7a7aULL);
			m2   = 0x2020202020202020ULL & m2;
			w2  -= m2;
			m2   = czx1(w1);
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
		 * GCC can turn these compares into cmov (lots of predicates FTW)
		 * and puts it into 3 instruction bundles.
		 * so spare IA64 from the byte access into a lookup table
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
	m1   = 0x2020202020202020ULL & m1;
	w1  -= m1;
	m2   = pcmp1gt(w2, 0x6060606060606060ULL);
	m2  ^= pcmp1gt(w2, 0x7a7a7a7a7a7a7a7aULL);
	m2   = 0x2020202020202020ULL & m2;
	w2  -= m2;
	if(shift)
	{
		if(!HOST_IS_BIGENDIAN) {
			w1 |= (~0ULL) >> ((SOULL - shift) * BITS_PER_CHAR);
			w2 |= (~0ULL) >> ((SOULL - shift) * BITS_PER_CHAR);
		} else {
			w1 |= (~0ULL) << ((SOULL - shift) * BITS_PER_CHAR);
			w2 |= (~0ULL) << ((SOULL - shift) * BITS_PER_CHAR);
		}
	}
	m2   = czx1(w1);
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
		m1   = 0x2020202020202020ULL & m1;
		w1  -= m1;
		m2   = pcmp1gt(w2, 0x6060606060606060ULL);
		m2  ^= pcmp1gt(w2, 0x7a7a7a7a7a7a7a7aULL);
		m2   = 0x2020202020202020ULL & m2;
		w2  -= m2;
		m2   = czx1(w1);
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
