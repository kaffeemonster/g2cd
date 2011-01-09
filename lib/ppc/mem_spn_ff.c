/*
 * mem_spn_ff.c
 * count 0xff span length, ppc implementation
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

#if defined( __ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <altivec.h>
# include "ppc_altivec.h"

size_t mem_spn_ff(const void *s, size_t len)
{
	vector unsigned char v_ff;
	vector unsigned char v_perm;
	vector unsigned char c;
	vector bool char ft;
	const unsigned char *p;
	ssize_t k;
	size_t block_num;
	unsigned j;

	if(unlikely(!len))
		return 0;

	/* only do one prefetch, this covers nearly 128k */
	block_num = DIV_ROUNDUP(len, 512);
	j  = block_num >= 256 ? 0 : block_num << 16;
	j |= 512;
	vec_dst((const unsigned char *)s, j, 2);

	v_ff = (vector unsigned char)vec_splat_s8(-1);

	k = SOVUC - ALIGN_DOWN_DIFF(s, SOVUC) - (ssize_t)len;

	p = (const unsigned char *)ALIGN_DOWN(s, SOVUC);
	c = vec_ldl(0, (const vector unsigned char *)p);
	v_perm = vec_lvsl(0, (const unsigned char *)s);
	c = vec_perm(c, v_ff, v_perm);
	v_perm = vec_lvsr(0, (const unsigned char *)s);
	c = vec_perm(v_ff, c, v_perm);
	if(unlikely(0 > k))
		goto K_SHIFT;

	k = -k;

	if(vec_all_eq(c, v_ff))
	{
		for(p += SOVUC, k -= SOVUC; k >= (ssize_t)SOVUC;
		    p += SOVUC, k -= SOVUC) {
			c = vec_ldl(0, (const vector unsigned char *)p);
			if(vec_any_ne(c, v_ff))
				goto OUT;
		}
		if(0 >= k)
			goto OUT_LEN;
		c = vec_ldl(0, (const vector unsigned char *)p);
		k = SOVUC - k;
K_SHIFT:
		v_perm = vec_lvsr(0, (unsigned char *)k);
		c = vec_perm(v_ff, c, v_perm);
		v_perm = vec_lvsl(0, (unsigned char *)k);
		c = vec_perm(c, v_ff, v_perm);
		if(vec_all_eq(c, v_ff))
			goto OUT_LEN;
	}

OUT:
	vec_dss(2);
	ft = vec_cmpeq(c, v_ff);
	return (size_t)(p - (const unsigned char *)s) + vec_zpos(ft);
OUT_LEN:
	vec_dss(2);
	return len;
}

static char const rcsid_msfalt[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/mem_spn_ff.c"
#endif
/* EOF */
