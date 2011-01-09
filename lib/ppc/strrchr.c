/*
 * strrchr.c
 * strrchr, ppc implementation
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

#if defined(__ALTIVEC__)
# include <altivec.h>
# include "ppc_altivec.h"
/*
 * We could first do a strlen to find the end of the string,
 * then search backwards. But since we have to walk the string
 * to find the end anyway we could pick up the matches on the
 * way while we are at it.
 */
char *strrchr(const char *s, int c)
{
	vector unsigned char v0;
	vector unsigned char v_c;
	vector unsigned char v_perm;
	vector unsigned char x;
	vector bool char m1, m2, l_m;
	char *p, *l_match;

	/* only prefetch the first block of 512 byte */
	vec_dst((const unsigned char *)s, 0x10200, 2);
	/* transfer lower nibble */
	v_c = vec_lvsl(c & 0x0F, (unsigned char *)NULL);
	/* transfer upper nibble */
	x   = vec_lvsl((c >> 4) & 0x0F, (unsigned char *)NULL);
	x   = vec_sl(x, vec_splat_u8(4));
	/* glue together */
	v_c = vec_or(v_c, x);
	v_c = vec_splat(v_c, 0);

	v0 = vec_splat_u8(0);

	p = (char *)ALIGN_DOWN(s, SOVUC);
	x = vec_ldl(0, (const vector unsigned char *)p);
	m1 = vec_cmpeq(x, v0);
	m2 = vec_cmpeq(x, v_c);
	v_perm = vec_lvsl(0, (unsigned char *)(uintptr_t)s);
	m1 = (vector bool char)vec_perm((vector unsigned char)m1, v0, v_perm);
	m2 = (vector bool char)vec_perm((vector unsigned char)m2, v0, v_perm);
	v_perm = vec_lvsr(0, (unsigned char *)(uintptr_t)s);
	m1 = (vector bool char)vec_perm(v0, (vector unsigned char)m1, v_perm);
	m2 = (vector bool char)vec_perm(v0, (vector unsigned char)m2, v_perm);

	l_match = p;
	l_m = (vector bool char)v0;

	while(vec_all_eq(m1, v0))
	{
		if(vec_any_ne(m2, v0)) {
			l_match = p;
			l_m = m2;
		}
		p += SOVUC;
		x = vec_ldl(0, (const vector unsigned char *)p);
		m1 = vec_cmpeq(x, v0);
		m2 = vec_cmpeq(x, v_c);
	}
	vec_dss(2);
	if(vec_any_ne(m2, v0))
	{
		vector unsigned char v1 = (vector unsigned char)vec_splat_s8(-1);
		uint32_t r = vec_zpos(m1);
		v_perm = vec_lvsr(r, (unsigned char *)0);
		m1 = (vector bool char)vec_perm(v1, v0, v_perm);
		m2 = vec_sel((vector bool char)v0, m2, m1);
		if(vec_any_ne(m2, v0))
			return (char *)(uintptr_t)p + vec_zpos_last(m2);
	}
	if(vec_any_ne(l_m, v0))
		return (char *)(uintptr_t)l_match + vec_zpos_last(l_m);
	return NULL;
}

static char const rcsid_srcpav[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "ppc.h"
# include "../generic/strrchr.c"
#endif
/* EOF */
