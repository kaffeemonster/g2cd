/*
 * strncasecmp_a.c
 * strncasecmp ascii only, generic implementation
 *
 * Copyright (c) 2008 Jan Seiffert
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

int strncasecmp_a(const char *s1, const char *s2, size_t n)
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
	size_t w1, w2;
	size_t m1, m2;
	size_t i, j, cycles;
	unsigned c1, c2;

	if(unlikely(!n))
		return 0;

	prefetch(s1);
	prefetch(s2);

LOOP_AGAIN:
	if(UNALIGNED_OK)
	{
		i = ALIGN_DIFF(s1, 4096);
		j = ALIGN_DIFF(s2, 4096);
		i = i < j ? i : j;
		j = ROUND_ALIGN(n, SOST);
		i = i < j ? i : j;
	} else {
		i = ALIGN_DIFF(s1, SOST);
		i = i < n ? i : n;
		goto SINGLE_BYTE;
	}

LOOP_SOST:
	for(cycles = i; likely(SOSTM1 < i); i -= SOST)
	{
		w1   = *(const size_t *)s1;
		w2   = *(const size_t *)s2;
		s1  += SOST;
		s2  += SOST;
		m1   = has_between(w1, 0x60, 0x7B);
	/*	m1  &= ~(c1 & MK_C(0x80808080)); */
		m1 >>= 2;
		w1  -= m1;
		m2   = has_between(w2, 0x60, 0x7B);
	/*	m2  &= ~(c2 & MK_C(0x80808080)); */
		m2 >>= 2;
		w2  -= m2;
		m1   = w1 ^ w2;
		m2   = has_nul_byte(w1) | has_nul_byte(w2);
		if(m1 || m2) {
			unsigned r1, r2;
			m1 = has_greater(m1, 0);
			r1 = nul_byte_index(m1);
			r2 = nul_byte_index(m2);
			r1 = r1 < r2 ? r1 : r2;
			cycles = (((cycles - i)) / SOST) * SOST;
			n -= cycles;
			r1 = r1 < n - 1 ? r1 : n - 1;
			if(HOST_IS_BIGENDIAN)
				r1 = SOSTM1 - r1;
			r1 = r1 * BITS_PER_CHAR;
			return (int)((w1 >> r1) & 0xFF) - (int)((w2 >> r1) & 0xFF);
		}
	}
	cycles = ((cycles - i) / SOST) * SOST;
	if(cycles >= n)
		return 0;
	n -= cycles;

	if(UNALIGNED_OK)
	{
		i = ALIGN_DIFF(s1, 4096);
		j = ALIGN_DIFF(s2, 4096);
		i = i < j ? i : j;
		i = i < n ? i : n;
	}

SINGLE_BYTE:
	for(; i; i--, n--)
	{
		c1 = (unsigned) *s1++;
		c2 = (unsigned) *s2++;
		n--;
		c1 -= tab[c1];
		c2 -= tab[c2];
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
			j = (((intptr_t)s1) & ((SOST * 2) - 1)) ^
			    (((intptr_t)s2) & ((SOST * 2) - 1));
			if(SOSTM1 & j)
				goto SINGLE_BYTE;
			else
				goto LOOP_SOST;
		}
	}

	return 0;
}

static char const rcsid_sncca[] GCC_ATTR_USED_VAR = "$Id: $";
