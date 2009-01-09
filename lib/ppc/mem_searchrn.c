/*
 * mem_searchrn.c
 * search mem for a \r\n, ppc implementation
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

#if defined( __ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <altivec.h>
# include "ppc_altivec.h"

void *mem_searchrn(void *s, size_t len)
{
	vector unsigned char v_cr;
	vector unsigned char v_nl;
	vector unsigned char v0;
	vector unsigned char v_perm;
	vector unsigned char c;
	vector bool char rr, rn;
	vector bool char last_rr;
	char *p;
	ssize_t k;

	prefetch(s);
	if(unlikely(!s || !len))
		return NULL;

	v_cr = vec_splat_u8('\r');
	v_nl = vec_splat_u8('\n');
	v0   = vec_splat_u8(0);
	last_rr = v0;

	k = SOVUC - ALIGN_DOWN_DIFF(s, SOVUC) - (ssize_t)len;

	p = (char *)ALIGN_DOWN(s, SOVUC);
	c = vec_ldl(0, (vector const unsigned char *)p);
	if(unlikely(k > 0))
		goto K_SHIFT;
	v_perm = vec_lvsl(0, (unsigned char *)s);
	c = vec_perm(c, v0, v_perm);
	v_perm = vec_lvsr(0, (unsigned char *)s);
	c = vec_perm(v0, c, v_perm);
	rr = vec_cmpeq(c, v_cr);
	rn = vec_cmpeq(c, v_nl);

	k = -k;
	goto START_LOOP;

	do
	{
		p += SOVUC;
		c = vec_ldl(0, (vector const unsigned char *)p);
		k -= SOVUC;
		if(k > 0)
		{
			rr = vec_cmpeq(c, v_cr);
			rn = vec_cmpeq(c, v_nl);

			if(vec_any_eq(last_rr, rn))
				return p - 1;
START_LOOP:
			last_rr = (vector bool char)vec_sld(v0, (vector unsigned char)rr, 1);
			rn = (vector bool char)vec_sld(v0, (vector unsigned char)rn, 15);
			rr = vec_and(rr, rn); /* get mask */
			if(vec_any_ne(rr, v0)) {
				uint32_t r;
				r = vec_pmovmskb(rr);
				return p + __builtin_clz(r) - 16;
			}
		}
	} while(k > 0);
	k = -k;
K_SHIFT:
	v_perm = vec_lvsr(0, (unsigned char *)k);
	c = vec_perm(v0, c, v_perm);
	v_perm = vec_lvsl(0, (unsigned char *)k);
	c = vec_perm(c, v0, v_perm);
	rr = vec_cmpeq(c, v_cr);
	rn = vec_cmpeq(c, v_nl);
	if(vec_any_eq(last_rr, rn))
		return p - 1;

	rn = (vector bool char)vec_sld(v0, (vector unsigned char)rn, 15);
	rr = vec_and(rr, rn); /* get mask */
	if(vec_any_ne(rr, v0)) {
		uint32_t r;
		r = vec_pmovmskb(rr);
		return p + __builtin_clz(r) - 16;
	}

	return NULL;
}

static char const rcsid_msrn[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/mem_searchrn.c"
#endif
/* EOF */
