/*
 * str_spn_space.c
 * count white space span length, ppc implementation
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

#if defined(__ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <altivec.h>
# include "ppc_altivec.h"

static noinline size_t str_spn_space_slow(const char *str)
{
	vector unsigned char v0;
	vector unsigned char v_space, v_bs, v_so;
	vector unsigned char v_perm;
	vector unsigned char x;
	vector bool char n, r;
	const unsigned char *p;

	/* only prefetch the first block of 512 byte */
	vec_dst((const unsigned char *)str, 0x10200, 2);

	/* create constatns */
	v0 = vec_splat_u8(0);
	v_space = vec_sl(vec_splat_u8(1), vec_splat_u8(5));
	v_bs = vec_splat_u8(8);
	v_so = vec_splat_u8(14);

	/* align input vector */
	p = (const unsigned char *)ALIGN_DOWN(str, SOVUC);
	x = vec_ldl(0, p);

	/* find Nul-bytes */
	n = vec_cmpeq(x, v0);
	/* find space */
	r = vec_cmpeq(x, v_space);
	/* add everything between backspace and shift out */
	r = vec_or(r, vec_and(vec_cmpgt(x, v_bs), vec_cmplt(x, v_so)));
	/* mask out exes bytes from alignment */
	v_perm = vec_lvsl(0, (unsigned char *)(uintptr_t)str);
	n = (vector bool char)vec_perm((vector unsigned char)n, v0, v_perm);
	r = (vector bool char)vec_perm((vector unsigned char)r, v0, v_perm);
	v_perm = vec_lvsr(0, (unsigned char *)(uintptr_t)str);
	n = (vector bool char)vec_perm(v0, (vector unsigned char)n, v_perm);
	r = (vector bool char)vec_perm(vec_splat_u8(-1), (vector unsigned char)n, v_perm);

	/* as long as we didn't hit a Nul-byte and all bytes are whitespace */
	while(vec_all_eq(n, v0) && vec_all_ne(r, v0))
	{
		p += SOVUC;
		x  = vec_ldl(0, p);
		n = vec_cmpeq(x, v0);
		r = vec_cmpeq(x, v_space);
		r = vec_or(r, vec_and(vec_cmpgt(x, v_bs), vec_cmplt(x, v_so)));
	}
	vec_dss(2);

	/* invert whitespace match to single out first non white space */
	r = vec_nor(r, r);
	/* add Nul-byte mask on top */
	r = vec_or(r, n);
	return (p + vec_zpos(r)) - (const unsigned char *)str;
}

static char const rcsid_strspnsa[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "ppc.h"
# include "../generic/str_spn_space.c"
#endif
/* EOF */
