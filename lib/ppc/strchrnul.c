/*
 * strchrnul.c
 * strchrnul for non-GNU-platforms, ppc implementation
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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

#if defined(__ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <altivec.h>
# include "ppc_altivec.h"

char *strchrnul(const char *s, int c)
{
	vector unsigned char v0;
	vector unsigned char v_c;
	vector unsigned char v_perm;
	vector unsigned char x;
	vector bool char m1, m2;
	uint32_t r;
	char *p;

	prefetch(s);

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
	m1 = vec_or(m1, m2);
	v_perm = vec_lvsl(0, (unsigned char *)(uintptr_t)s);
	m1 = (vector bool char)vec_perm((vector unsigned char)m1, v0, v_perm);
	v_perm = vec_lvsr(0, (unsigned char *)(uintptr_t)s);
	m1 = (vector bool char)vec_perm(v0, (vector unsigned char)m1, v_perm);

	while(vec_all_eq(m1, v0)) {
		p += SOVUC;
		x = vec_ldl(0, (const vector unsigned char *)p);
		m1 = vec_cmpeq(x, v0);
		m2 = vec_cmpeq(x, v_c);
		m1 = vec_or(m1, m2);
	}
	r = vec_pmovmskb(m1);
	return (char *)(uintptr_t)p + __builtin_clz(r) - 16;
}

static char const rcsid_scn[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "ppc.h"
# include "../generic/strchrnul.c"
#endif
/* EOF */
