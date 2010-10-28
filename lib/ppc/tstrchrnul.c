/*
 * tstrchrnul.c
 * tstrchrnul, ppc implementation
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

#if defined(__ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <altivec.h>
# include "ppc_altivec.h"

tchar_t *tstrchrnul(const tchar_t *s, tchar_t c)
{
	vector unsigned short v0;
	vector unsigned short v_c;
	vector unsigned char v_perm;
	vector unsigned short x;
	vector bool short m1, m2;
	char *p;

	prefetch(s);

	v0 = vec_splat_u16(0);

	v_c = v0;
	v_c = vec_lde(0, &c);
	v_perm = vec_lvsl(0, &c);
	v_c = vec_perm(v_c, v0, v_perm);
	v_c = vec_splat(v_c, 0);

	p = (char *)ALIGN_DOWN(s, SOVUC);
	x = vec_ldl(0, (const vector unsigned short *)p);
	m1 = vec_cmpeq(x, v0);
	m2 = vec_cmpeq(x, v_c);
	m1 = vec_or(m1, m2);
	v_perm = vec_lvsl(0, (unsigned short *)(uintptr_t)s);
	m1 = (vector bool short)vec_perm((vector unsigned short)m1, v0, v_perm);
	v_perm = vec_lvsr(0, (unsigned short *)(uintptr_t)s);
	m1 = (vector bool short)vec_perm(v0, (vector unsigned short)m1, v_perm);

	while(vec_all_eq(m1, v0))
	{
		p += SOVUC;
		x = vec_ldl(0, (const vector unsigned short *)p);
		m1 = vec_cmpeq(x, v0);
		m2 = vec_cmpeq(x, v_c);
		m1 = vec_or(m1, m2);
	}
	return ((tchar_t *)(uintptr_t)p) + vec_zpos((vector bool char)m1)/sizeof(tchar_t);
}

static char const rcsid_tscn[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "ppc.h"
# include "../generic/tstrchrnul.c"
#endif
/* EOF */
