/*
 * strchrnul.c
 * strchrnul for non-GNU-platforms, ppc implementation
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

#if defined(__ALTIVEC__)
# include <altivec.h>
# include "ppc_altivec.h"

char *strchrnul(const char *s, int c)
{
	vector unsigned char v0;
	vector unsigned char v_c;
	vector unsigned char v_perm;
	vector unsigned char x;
	vector bool char m1, m2;
	char *p;

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
	x = vec_ldl(0, (const unsigned char *)p);
	m1 = vec_cmpeq(x, v0);
	m2 = vec_cmpeq(x, v_c);
	m1 = vec_or(m1, m2);
	v_perm = vec_lvsl(0, (unsigned char *)(uintptr_t)s);
	m1 = (vector bool char)vec_perm((vector unsigned char)m1, v0, v_perm);
	v_perm = vec_lvsr(0, (unsigned char *)(uintptr_t)s);
	m1 = (vector bool char)vec_perm(v0, (vector unsigned char)m1, v_perm);

	while(vec_all_eq(m1, v0)) {
		p += SOVUC;
		x = vec_ldl(0, (const unsigned char *)p);
		m1 = vec_cmpeq(x, v0);
		m2 = vec_cmpeq(x, v_c);
		m1 = vec_or(m1, m2);
	}
	vec_dss(2);
	return (char *)(uintptr_t)p + vec_zpos(m1);
}

static char const rcsid_scnav[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "ppc.h"
# include "../generic/strchrnul.c"
#endif
/* EOF */
