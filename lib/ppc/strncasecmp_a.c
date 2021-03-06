/*
 * strncasecmp_a.c
 * strncasecmp ascii only, ppc implementation
 *
 * Copyright (c) 2008-2011 Jan Seiffert
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

#if defined(__ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <altivec.h>
# include "ppc_altivec.h"

int strncasecmp_a(const char *s1, const char *s2, size_t n)
{
	vector unsigned char c_ex1, c_ex2;
	vector unsigned char c_1, c_2;
	vector unsigned char v_perm1, v_perm2;
	vector unsigned char v_0, v_1, v_20, v_60;
	static const vector unsigned char v_7B =
		{0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B,
		 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B};
	size_t i, j, cycles;
	unsigned c1, c2;

	if(unlikely(!n))
		return 0;

	prefetch(s1);
	prefetch(s2);

	v_0  = vec_splat_u8(0);
	v_1  = vec_splat_u8(1);
	v_20 = vec_sl(v_1, vec_splat_u8(5));
	v_60 = vec_sl(v_20, v_1);
	v_60 = vec_or(v_60, v_20);

LOOP_AGAIN:
	i = ALIGN_DIFF(s1, 4096);
	i = i ? i : 4096;
	j = ALIGN_DIFF(s2, 4096);
	j = j ? j : i;
	i = i < j ? i : j;
	j = ROUND_ALIGN(n, 2 * SOVUC);
	i = i < j ? i : j;

	cycles = i;
	if(likely(i >= 2 * SOVUC))
	{
		v_perm1 = vec_lvsl(0, (const unsigned char *)s1);
		v_perm2 = vec_lvsl(0, (const unsigned char *)s2);
		c_ex1 = vec_ldl(0, (const unsigned char *)s1);
		c_ex2 = vec_ldl(0, (const unsigned char *)s2);
		for(; likely(i >= 2 * SOVUC); i -= SOVUC)
		{
			vector bool char v_mask;
			c_1   = c_ex1;
			c_2   = c_ex2;
			c_ex1 = vec_ldl(1 * SOVUC, (const unsigned char *)s1);
			c_ex2 = vec_ldl(1 * SOVUC, (const unsigned char *)s2);
			c_1   = vec_perm(c_1, c_ex1, v_perm1);
			c_2   = vec_perm(c_2, c_ex2, v_perm2);

			v_mask = vec_cmpgt(c_1, v_60);
			v_mask = vec_and(v_mask, vec_cmplt(c_1, v_7B));
			c_1    = vec_sub(c_1, vec_and(v_20, v_mask));
			v_mask = vec_cmpgt(c_2, v_60);
			v_mask = vec_and(v_mask, vec_cmplt(c_2, v_7B));
			c_2    = vec_sub(c_2, vec_and(v_20, v_mask));

			v_mask = vec_cmpeq(c_1, v_0);
			c_1    = vec_xor(c_1, c_2);
			v_mask = vec_or(v_mask, vec_cmpgt(c_1, v_0));

			if(likely(vec_any_ne(v_mask, v_0)))
			{
				unsigned r;
				r   = vec_zpos(vec_cmpgt((vector unsigned char)v_mask, v_0));
				cycles = (((cycles - i)) / SOVUC) * SOVUC;
				n  -= cycles;
				r   = r < n - 1 ? r : n - 1;
				c1  = *(const unsigned char *)(s1 + r);
				c2  = *(const unsigned char *)(s2 + r);
				c1 -= c1 >= 'a' && c1 <= 'z' ? 0x20 : 0;
				c2 -= c2 >= 'a' && c2 <= 'z' ? 0x20 : 0;
				return (int)c1 - (int)c2;
			}
			s1   += SOVUC;
			s2   += SOVUC;
		}
	}
	cycles = ((cycles - i) / SOVUC) * SOVUC;
	if(cycles >= n)
		return 0;
	n -= cycles;

	i  = ALIGN_DIFF(s1, 4096);
	i  = i ? i : 4096;
	j  = ALIGN_DIFF(s2, 4096);
	j  = j ? j : i;
	i  = i < j ? i : j;
	i  = i < n ? i : n;
	n -= i;

	for(; i; i--, s1++, s2++)
	{
		c1 = *(const unsigned char *)s1;
		c2 = *(const unsigned char *)s2;
		/*
		 * GCC can turn these compares into jumpless arith.
		 * So spare ppc from the mem access to a lookup table.
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

static char const rcsid_sncca[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "ppc.h"
# include "../generic/strncasecmp_a.c"
#endif
/* EOF */
