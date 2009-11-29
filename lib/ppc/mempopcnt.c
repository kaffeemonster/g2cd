/*
 * mempopcnt.c
 * popcount a mem region, ppc64 implementation
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
 * $Id:$
 */

#ifdef __powerpc64__
static inline size_t popcountst_int1(size_t n)
{
	size_t tmp;
	__asm__ __volatile__("popcntb	%1, %0" : "=r" (tmp) : "r" (n) : "cc");
	return (tmp * 0x0101010101010101ULL) >> 56;
}

static inline size_t popcountst_int2(size_t n, size_t m)
{
	size_t tmp1, tmp2;
	__asm__ __volatile__("popcntb	%1, %0" : "=r" (tmp1) : "r" (n) : "cc");
	__asm__ __volatile__("popcntb	%1, %0" : "=r" (tmp2) : "r" (m) : "cc");
	return ((tmp1 + tmp2) * 0x0101010101010101ULL) >> 56;
}

static inline size_t popcountst_int4(size_t n, size_t m, size_t o, size_t p)
{
	return popcountst_int2(n, m) + popcountst_int2(o, p);
}

# define NO_GEN_POPER
# include "../generic/mempopcnt.c"
#else
# if defined(__ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
#  include <altivec.h>
#  include "ppc_altivec.h"

#define vec_popcnt(x) \
	vec_add(vec_perm(lutl, luth, x), vec_perm(lutl, lutl, vec_sr(x, v_5)))

size_t mempopcnt(const void *s, size_t len)
{
	vector unsigned char v_0, v_5;
	vector unsigned char lutl, luth;
	vector unsigned char c, v_perm;
	vector unsigned int v_sum;
	unsigned char *p;
	size_t r;
	uint32_t ret;
	unsigned shift;

	prefetch(s);

	lutl  = (vector unsigned char){4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0};
	luth  = vec_add(vec_splat_u8(1), lutl);
	v_5   = vec_splat_u8(5);
	v_0   = vec_splat_u8(0);
	v_sum = (vector unsigned int)v_0;
	/*
	 * Sometimes you need a new perspective, like the altivec
	 * way of handling things.
	 * Lower address bits? Totaly overestimated.
	 *
	 * We don't precheck for alignment.
	 * Instead we "align hard", do one load "under the address",
	 * mask the excess info out and afterwards we are fine to go.
	 */
	p = (unsigned char *)ALIGN_DOWN(s, SOVUC);
	shift = ALIGN_DOWN_DIFF(s, SOVUC) * BITS_PER_CHAR;
	c = vec_ldl(0, (vector const unsigned char *)p);
	v_perm = vec_lvsl(0, (unsigned char *)(uintptr_t)s);
	c = vec_perm(c, v_0, v_perm);
	if(len >= SOVUC || len + shift >= SOVUC)
	{
		p    += SOVUC;
		len  -= SOVUC - shift;
		v_sum = vec_sum4s(vec_popcnt(c), v_sum);

		while(len >= SOVUC * 2)
		{
			vector unsigned char v_sumb = v_0;

			r    = len / (SOVUC * 2);
			r    = r > 15 ? 15 : r;
			len -= r * SOVUC * 2;
			for(; r; r--, p += SOVUC * 2) {
				c      = vec_ldl(0, (vector const unsigned char *)p);
				v_sumb = vec_add(v_sumb, vec_popcnt(c));
				c      = vec_ldl(0, (vector const unsigned char *)(p + SOVUC));
				v_sumb = vec_add(v_sumb, vec_popcnt(c));
			}
			v_sum = vec_sum4s(v_sumb, v_sum);
		}
		if(len >= SOVUC) {
			c     = vec_ldl(0, (vector const unsigned char *)p);
			p    += SOVUC;
			v_sum = vec_sum4s(vec_popcnt(c), v_sum);
			len  -= SOVUC;
		}

		if(len)
			c = vec_ldl(0, (vector const unsigned char *)p);
	}
	if(len) {
		v_perm = vec_lvsr(0, (unsigned char *)(uintptr_t)(SOVUC - len));
		c      = vec_perm(v_0, c, v_perm);
		v_sum  = vec_sum4s(vec_popcnt(c), v_sum);
	}

	v_sum = (vector unsigned int)vec_sums((vector signed int)v_sum, (vector signed int)v_0);
	v_sum = vec_splat(v_sum, 3); /* splat */
	vec_ste(v_sum, 0, &ret); /* transfer */
	return ret;
}
# else
#  include "../generic/mempopcnt.c"
# endif
#endif

static char const rcsid_mpp[] GCC_ATTR_USED_VAR = "$Id:$";
