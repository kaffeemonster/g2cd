/*
 * tstrncmp.c
 * tstrncmp, generic implementation
 *
 * Copyright (c) 2009 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 * $Id: $
 */

int tstrncmp(const tchar_t *s1, const tchar_t *s2, size_t n)
{
#if 0
	size_t w1, w2;
	size_t m1, m2;
	size_t i, j, cycles;
	unsigned c1, c2;

	if(unlikely(!n))
		return 0;

	n *= sizeof(*s1);
	prefetch(s1);
	prefetch(s2);

LOOP_AGAIN:
	if(UNALIGNED_OK)
	{
		i = ALIGN_DIFF(s1, 4096);
		i = i ? i : 4096;
		j = ALIGN_DIFF(s2, 4096);
		j = j ? j : i;
		i = i < j ? i : j;
		j = ROUND_ALIGN(n, SOST);
		i = i < j ? i : j;
	} else {
		i = ALIGN_DIFF(s1, SOST);
		i = i < n ? i : n;
		goto SINGLE_WORD;
	}

LOOP_SOST:
	for(cycles = i; likely(SOSTM1 < i); i -= SOST)
	{
		if(UNALIGNED_OK) {
			w1 = get_unaligned((const size_t *)s1);
			w2 = get_unaligned((const size_t *)s2);
		} else {
			w1 = *(const size_t *)s1;
			w2 = *(const size_t *)s2;
		}
		s1  += SOST/sizeof(*s1);
		s2  += SOST/sizeof(*s2);
		m1   = w1 ^ w2;
		m2   = has_nul_word(w1) | has_nul_word(w2);
		if(m1 || m2) {
			unsigned r1, r2;
			m1 = has_word_greater(m1, 0);
			r1 = nul_word_index(m1);
			r2 = nul_word_index(m2);
			r1 = r1 < r2 ? r1 : r2;
			cycles = ROUND_TO(cycles - i, SOST);
			n -= cycles;
			r1 = r1 < n - 1 ? r1 : n - 1;
			if(HOST_IS_BIGENDIAN)
				r1 = SOSTM1 - r1;
			r1 *= BITS_PER_CHAR * sizeof(tchar_t);
			return (int)((w1 >> r1) & 0xFFFF) - (int)((w2 >> r1) & 0xFFFF);
		}
	}
	cycles = ROUND_TO(cycles - i, SOST);
	if(cycles >= n)
		return 0;
	n -= cycles;

	if(UNALIGNED_OK)
	{
		i = ALIGN_DIFF(s1, 4096);
		i = i ? i : 4096;
		j = ALIGN_DIFF(s2, 4096);
		j = j ? j : i;
		i = i < j ? i : j;
		i = i < n ? i : n;
	}

SINGLE_WORD:
	for(; i; i -= sizeof(tchar_t), n -= sizeof(tchar_t))
	{
		c1 = (unsigned) *s1++;
		c2 = (unsigned) *s2++;
		if(!(c1 && c2 && c1 == c2))
			return (int)c1 - (int)c2;
	}

	if(n)
	{
		if(UNALIGNED_OK)
			goto LOOP_AGAIN;
		else
		{
			i = n;
			if(ALIGN_DOWN_DIFF(s1, SOST) ^ ALIGN_DOWN_DIFF(s2, SOST))
				goto SINGLE_WORD;
			else
				goto LOOP_SOST;
		}
	}
#else
	for(; n; n--)
	{
		unsigned c1, c2;
		c1 = (unsigned) *s1++;
		c2 = (unsigned) *s2++;
		if(!(c1 && c2 && c1 == c2))
			return (int)c1 - (int)c2;
	}
#endif

	return 0;
}

static char const rcsid_tsnc[] GCC_ATTR_USED_VAR = "$Id: $";
