/*
 * strncasecmp_a.c
 * strncasecmp ascii only, parisc implementation
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

#include "parisc.h"

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
		int t1, t2;

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

			m1   = pcmp1gt(w1, MK_C(0x60606060UL));
			m1  ^= pcmp1gt(w1, MK_C(0x7a7a7a7aUL));
			m1   = m1 << 5; /* create 0x20 out of 0x01 */
			w1  -= m1;
			m2   = pcmp1gt(w2, MK_C(0x60606060UL));
			m2  ^= pcmp1gt(w2, MK_C(0x7a7a7a7aUL));
			m2   = m2 << 5; /* create 0x20 out of 0x01 */
			w2  -= m2;
			t1   = pa_is_z(w1);
			m1   = w1 ^ w2;
			if(t1 || m1)
			{
				t1 = t1 ? pa_find_z(w1) : (int)SOUL;
				t2 = m1 ? pa_find_nz(m1) : (int)SOUL;
				t1 = t1 < t2 ? t1 : t2;

				cycles = ROUND_TO(cycles - i, SOUL);
				cycles -= i;
				n -= cycles;
				m1 = (unsigned)t1 < n - 1 ? (unsigned)t1 : n - 1;
				if(HOST_IS_BIGENDIAN)
					m1 = SOULM1 - m1;
				m1 *= BITS_PER_CHAR;
				return (int)((w1 >> m1) & 0xFF) - (int)((w2 >> m1) & 0xFF);
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
		 * GCC can turn these compares into cmov (annuling FTW).
		 * So spare parisc from the mem access into a lookup table
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
	int t1, t2;
	unsigned shift;

	shift = ALIGN_DOWN_DIFF(s1, SOUL);
	s1 = (const char *)ALIGN_DOWN(s1, SOUL);
	s2 = (const char *)ALIGN_DOWN(s2, SOUL);

	w1   = *(const unsigned long *)s1;
	w2   = *(const unsigned long *)s2;
	s1  += SOUL;
	s2  += SOUL;
	m1   = pcmp1gt(w1, MK_C(0x60606060UL));
	m1  ^= pcmp1gt(w1, MK_C(0x7a7a7a7aUL));
	m1   = m1 << 5; /* create 0x20 out of 0x01 */
	w1  -= m1;
	m2   = pcmp1gt(w2, MK_C(0x60606060UL));
	m2  ^= pcmp1gt(w2, MK_C(0x7a7a7a7aUL));
	m2   = m2 << 5; /* create 0x20 out of 0x01 */
	w2  -= m2;
	if(!HOST_IS_BIGENDIAN) {
		w1 |= (~0UL) >> ((SOUL - shift) * BITS_PER_CHAR);
		w2 |= (~0UL) >> ((SOUL - shift) * BITS_PER_CHAR);
	} else {
		w1 |= (~0UL) << ((SOUL - shift) * BITS_PER_CHAR);
		w2 |= (~0UL) << ((SOUL - shift) * BITS_PER_CHAR);
	}
	t1   = pa_is_z(w1);
	m1   = w1 ^ w2;
	if(t1 || m1)
	{
		t1 = t1 ? pa_find_z(w1) : (int)SOUL;
		t2 = m1 ? pa_find_nz(m1) : (int)SOUL;
		t1 = t1 < t2 ? t1 : t2;
		m1 = (unsigned)t1 < n - 1 ? (unsigned)t1 : n - 1;
		if(HOST_IS_BIGENDIAN)
			m1 = SOULM1 - m1;
		m1 *= BITS_PER_CHAR;
		return (int)((w1 >> m1) & 0xFF) - (int)((w2 >> m1) & 0xFF);
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
		m1   = pcmp1gt(w1, MK_C(0x60606060UL));
		m1  ^= pcmp1gt(w1, MK_C(0x7a7a7a7aUL));
		m1   = m1 << 5; /* create 0x20 out of 0x01 */
		w1  -= m1;
		m2   = pcmp1gt(w2, MK_C(0x60606060UL));
		m2  ^= pcmp1gt(w2, MK_C(0x7a7a7a7aUL));
		m2   = m2 << 5; /* create 0x20 out of 0x01 */
		w2  -= m2;
		t1   = pa_is_z(w1);
		m1   = w1 ^ w2;
		if(t1 || m1)
		{
			t1 = t1 ? pa_find_z(w1) : (int)SOUL;
			t2 = m1 ? pa_find_nz(m1) : (int)SOUL;
			t1 = t1 < t2 ? t1 : t2;
			cycles -= i;
			n -= cycles;
			m1 = (unsigned)t1 < n - 1 ? (unsigned)t1 : n - 1;
			if(HOST_IS_BIGENDIAN)
				m1 = SOULM1 - m1;
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

	if(likely(((intptr_t)s1 ^ (intptr_t)s2) & SOULM1))
		return strncasecmp_a_u(s1, s2, n);
	return strncasecmp_a_a(s1, s2, n);
}

static char const rcsid_snccaia[] GCC_ATTR_USED_VAR = "$Id: $";
