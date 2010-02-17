/*
 * memchr.c
 * memchr, ppc implementation
 *
 * Copyright (c) 2010 Jan Seiffert
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

void *my_memchr(const void *s, int c, size_t n)
{
	vector unsigned char v0;
	vector unsigned char v_c;
	vector unsigned char v_perm;
	vector unsigned char x;
	uint32_t r;
	ssize_t f, k;
	const unsigned char *p;

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

	f = ALIGN_DOWN_DIFF(s, SOVUC);
	k = SOST - f - (ssize_t)n;

	p = (const unsigned char *)ALIGN_DOWN((const unsigned char *)s, SOVUC);
	x = vec_ldl(0, (const vector unsigned char *)p);
	x = (vector unsigned char)vec_cmpeq(x, v_c);
	v_perm = vec_lvsl(0, (unsigned char *)(uintptr_t)s);
	x = vec_perm(x, v0, v_perm);
	v_perm = vec_lvsr(0, (unsigned char *)(uintptr_t)s);
	x = vec_perm(v0, x, v_perm);
	if(k > 0)
	{
// TODO: +1 to give a break condition?
		v_perm = vec_lvsr(0, (unsigned char *)k);
		x = vec_perm(v0, x, v_perm);
		v_perm = vec_lvsl(0, (unsigned char *)k);
		x = vec_perm(x, v0, v_perm);
	}

	if(vec_any_ne(x, v0)) {
		goto OUT;
	}
	if(k > 0)
		return NULL;

	n -= SOST - f;
	do
	{
		p += SOVUC;
		x = vec_ldl(0, (const vector unsigned char *)p);
		if(n <= SOVUC)
			break;
		n -= SOVUC;
	} while(!vec_any_eq(x, v_c));

	x = (vector unsigned char)vec_cmpeq(x, v_c);
	k = SOST - (ssize_t)n;
	if(k > 0)
	{
// TODO: +1 to give a break condition?
		v_perm = vec_lvsr(0, (unsigned char *)k);
		x = vec_perm(v0, x, v_perm);
		v_perm = vec_lvsl(0, (unsigned char *)k);
		x = vec_perm(x, v0, v_perm);
	}
	if(!vec_any_ne(x, v0))
		return NULL;
OUT:
	r = vec_pmovmskb((vector bool char)x);
	return (char *)(uintptr_t)p + __builtin_clz(r) - 16;
}

static char const rcsid_mc[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/memchr.c"
#endif
/* EOF */
