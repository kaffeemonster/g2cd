/*
 * strnlen.c
 * strnlen for non-GNU platforms, ppc implementation
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

#if defined(__ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <altivec.h>
# include "ppc_altivec.h"

size_t strnlen(const char *s, size_t maxlen)
{
	vector unsigned char v0;
	vector unsigned char v1;
	vector unsigned char v_perm;
	vector unsigned char c;
	size_t r;
	ssize_t f, k;
	char *p;

	prefetch(s);

	v0 = vec_splat_u8(0);
	v1 = vec_splat_u8(1);

	f = ALIGN_DOWN_DIFF(s, SOVUC);
	k = SOST - f - (ssize_t)maxlen;

	p = (char *)ALIGN_DOWN(s, SOVUC);
	c = vec_ldl(0, (vector unsigned char *)p);
	v_perm = vec_lvsl(0, (unsigned char *)(uintptr_t)s);
	c = vec_perm(c, v1, v_perm);
	v_perm = vec_lvsr(0, (unsigned char *)(uintptr_t)s);
	c = vec_perm(v1, c, v_perm);
	if(k > 0)
	{
// TODO: +1 to give a break condition?
		v_perm = vec_lvsr(0, (unsigned char *)k);
		c = vec_perm(v0, c, v_perm);
		v_perm = vec_lvsl(0, (unsigned char *)k);
		c = vec_perm(c, v0, v_perm);
	}

	if(vec_any_eq(c, v0))
		goto OUT;

	maxlen -= SOST - f;
	do
	{
		p += SOVUC;
		c = vec_ldl(0, (vector unsigned char *)p);
		if(maxlen <= SOVUC)
			break;
		maxlen -= SOVUC;
	} while(!vec_any_eq(c, v0));

	k = SOST - (ssize_t)maxlen;
	if(k > 0)
	{
// TODO: +1 to give a break condition?
		v_perm = vec_lvsr(0, (unsigned char *)k);
		c = vec_perm(v0, c, v_perm);
		v_perm = vec_lvsl(0, (unsigned char *)k);
		c = vec_perm(c, v0, v_perm);
	}
OUT:
	r = vec_pmovmskb(vec_cmpeq(c, v0));
	return p - s + __builtin_clz(r) - 16;
}

static char const rcsid_snl[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "ppc.h"
# include "../generic/strnlen.c"
#endif
/* EOF */
