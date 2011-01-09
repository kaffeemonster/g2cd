/*
 * mem_searchrn.c
 * search mem for a \r\n, ppc implementation
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
	size_t block_num;
	unsigned f;

	if(unlikely(!s || !len))
		return NULL;

	/* only do one prefetch, this covers nearly 128k */
	block_num = DIV_ROUNDUP(len, 512);
	f  = block_num >= 256 ? 0 : block_num << 16;
	f |= 512;
	vec_dst((const unsigned char *)s, f, 2);

	v_cr = vec_splat_u8('\r');
	v_nl = vec_splat_u8('\n');
	v0   = vec_splat_u8(0);
	last_rr = (vector bool char)v0;

	k = SOVUC - ALIGN_DOWN_DIFF(s, SOVUC) - (ssize_t)len;

	p = (char *)ALIGN_DOWN(s, SOVUC);
	c = vec_ldl(0, (const vector unsigned char *)p);
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
		c = vec_ldl(0, (const vector unsigned char *)p);
		k -= SOVUC;
		if(k > 0)
		{
			rr = vec_cmpeq(c, v_cr);
			rn = vec_cmpeq(c, v_nl);

			if(vec_any_eq(last_rr, rn)) {
				vec_dss(2);
				return p - 1;
			}
START_LOOP:
			last_rr = (vector bool char)vec_sld(v0, (vector unsigned char)rr, 1);
			rn = (vector bool char)vec_sld(v0, (vector unsigned char)rn, 15);
			rr = vec_and(rr, rn); /* get mask */
			if(vec_any_ne(rr, v0)) {
				vec_dss(2);
				return p + vec_zpos(rr);
			}
		}
	} while(k > 0);
	k = -k;
K_SHIFT:
	vec_dss(2);
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
	if(vec_any_ne(rr, v0))
		return p + vec_zpos(rr);

	return NULL;
}

static char const rcsid_msrn[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "ppc.h"
# include "../generic/mem_searchrn.c"
#endif
/* EOF */
