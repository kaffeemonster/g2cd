/*
 * strlpcpy.c
 * strlpcpy for efficient concatination, ppc implementation
 *
 * Copyright (c) 2008-2010 Jan Seiffert
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

char *strlpcpy(char *dst, const char *src, size_t maxlen)
{
	vector unsigned char v0, vff, c, v_perm, low, high, mask, t;
	size_t i, s_diff, d_diff, r, u;
	const unsigned char *s;
	unsigned char *d;

	prefetch(src);
	prefetchw(dst);
	if(unlikely(!src || !dst || !maxlen))
		return dst;

	  d = (unsigned char *)dst;
	  s = (const unsigned char *)src;
	 v0 = vec_splat_u8(0);
	vff = (vector unsigned char)vec_splat_s8(-1);

	     c = vec_ldl(0, s);
	   low = vec_ldl(0, d);
	s_diff = ALIGN_DOWN_DIFF(src, SOVUC);
	     i = s_diff + maxlen >= SOVUC ? 0 : SOVUC - s_diff - maxlen;

	v_perm = vec_lvsl(0, s);
	     c = vec_perm(c, vff, v_perm);
	v_perm = vec_lvsr(i, s);
	     c = vec_perm(vff, c, v_perm);
	v_perm = vec_identl(i);
	     c = vec_perm(c, vff, v_perm);

	d_diff = ALIGN_DOWN_DIFF(d, SOVUC);
	v_perm = vec_lvsr(0, d);
	  mask = vec_perm(v0, vff, v_perm);
	     t = vec_perm(c, c, v_perm);
	   low = vec_sel(low, t, mask);
	  high = vec_sel(t, v0, mask);
	     r = vec_any_eq(c, v0);
	if(r || i)
		goto OUT_STORE;

	vec_stl(low, 0, d);
	     s += SOVUC - s_diff;
	     d += SOVUC - s_diff;
	maxlen -= SOVUC - s_diff;

	v_perm = vec_lvsr(0, d);
	d_diff = ALIGN_DOWN_DIFF(d, SOVUC);
	d = (unsigned char *)ALIGN_DOWN(d, SOVUC);
	mask = vec_perm(v0, vff, v_perm);

	do
	{
		low  = high;
		  c  = high = vec_ldl(0, s);
		  s += SOVUC;
		high = vec_perm(high, high, v_perm);
		 low = vec_sel(low, high, mask);
		   r = vec_any_eq(c , v0);
		if(r)
			break;
		if(maxlen < SOVUC)
			break;
		maxlen -= SOVUC;
		vec_stl(low, 0, d);
		d += SOVUC;
	} while(1);

OUT_STORE:
	if(!r) /* did we hit maxlen? */
		u = maxlen - 1;
	else {
		u = vec_zpos(vec_cmpeq(c, v0)); /* get index */
		r = 1;
	}

	if(u >= d_diff)
	{
		vec_stl(low, 0, d);
		   d += SOVUC;
		 low  = high;
		high  = v0;
		   u -= d_diff;
		if(!u)
			return (char *)d - r;
	}
	high = vec_ldl(0, d);
	   c = vec_splat(vec_identl(u), 0);
	mask = (vector unsigned char)vec_cmpgt(c, vec_identl(0));
	 low = vec_sel(low, high, mask);
	vec_stl(low, 0, d);
	return (char *)d + u - r;
}

static char const rcsid_slpcav[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "ppc.h"
# include "../generic/strlpcpy.c"
#endif
/* EOF */
