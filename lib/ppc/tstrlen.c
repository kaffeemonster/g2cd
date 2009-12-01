/*
 * tstrlen.c
 * tstrlen, ppc implementation
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

#if defined(__ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <altivec.h>
# include "ppc_altivec.h"

size_t tstrlen(const tchar_t *s)
{
	vector unsigned short v0;
	vector unsigned char v1;
	vector unsigned char v_perm;
	vector unsigned char c;
	vector unsigned short x;
	uint32_t r;
	tchar_t *p;

	prefetch(s);

	v0 = vec_splat_u16(0);
	v1 = vec_splat_u8(1);

	p = (tchar_t *)ALIGN_DOWN(s, SOVUC);
	c = vec_ldl(0, (const vector unsigned char *)p);
	v_perm = vec_lvsl(0, (unsigned char *)(uintptr_t)s);
	c = vec_perm(c, v1, v_perm);
	v_perm = vec_lvsr(0, (unsigned char *)(uintptr_t)s);
	c = vec_perm(v1, c, v_perm);
	x = (vector unsigned short)c;

	while(!vec_any_eq(x, v0)) {
		p += SOVUC/sizeof(*p);
		x = vec_ldl(0, (const vector unsigned short *)p);
	}
	r = vec_pmovmskb((vector bool char)vec_cmpeq((vector unsigned char)x, (vector unsigned char)v0));
	return (p - s + __builtin_clz(r) - 16)/sizeof(tchar_t);
}

static char const rcsid_tslp[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/tstrlen.c"
#endif
/* EOF */
