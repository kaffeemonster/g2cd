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
static inline size_t popcountst_int(size_t n)
{
	/* according to POWER5 spec */
	size_t tmp;
	__asm__ __volatile__(
		"popcntb\t%0, %1\n\t"
		"mulld\t%0, %0, %2\n\t" /*(RB) = 0x0101_0101_0101_0101*/
		"srdi\t%0, %0, 56\n"
		: "=r" (tmp)
		: "r" (n), "r" (0x0101010101010101)
		: "cc"
	);
	return tmp;
}

size_t mempopcnt(const void *s, size_t len)
{
	const unsigned int *p;
	size_t r;
	size_t sum = 0;
	unsigned shift = ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR;

	prefetch(s);

	p = (const unsigned char *)ALIGN_DOWN(s, SOST);
	r = *(const size_t *)p;
	r <<= shift;
	if(len >= SOST || len + shift >= SOST)
	{
		/*
		 * Sometimes you need a new perspective, like the altivec
		 * way of handling things.
		 * Lower address bits? Totaly overestimated.
		 *
		 * We don't precheck for alignment.
		 * Instead we "align hard", do one load "under the address",
		 * mask the excess info out and afterwards we are fine to go.
		 */
		p += SOST;
		len -= SOST - shift;
		sum += popcountst_int(r);

		r = len / SOST;
		for(; r; r--, p += SOST)
			sum += popcountst_int(*(const size_t *)p);
		len %= SOST;
		if(len)
			r = *(const size_t *)p;
	}
	if(len) {
		r >>= SOST - len;
		sum += popcountst_int(r);
	}
	return sum;
}
#else
# if defined(__ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
#  include <altivec.h>
#  include "ppc_altivec.h"

#define vec_popcnt(x) \
	do { \
		x = vec_sub(x, vec_sr(vec_and(x, v_a), v_1)); \
		x = vec_add(vec_and(vec_sr(x, v_2), v_33), vec_and(x, v_33)); \
		x = vec_and(vec_add(vec_sr(x, v_4), x), v_f); \
		x = vec_add(vec_sr(x, v_8), x); \
		x = vec_add(vec_sr(x, v_16), x); \
		x = vec_and(x, v_ff); \
	} while(0)


size_t mempopcnt(const void *s, size_t len)
{
	vector unsigned char tmp;
	vector unsigned char v_44;
	vector unsigned char v_0;
	vector unsigned int v_1;
	vector unsigned int v_2;
	vector unsigned int v_4;
	vector unsigned int v_8;
	vector unsigned int v_a;
	vector unsigned int v_16;
	vector unsigned int v_33;
	vector unsigned int v_f;
	vector unsigned int v_ff;
	vector unsigned char c, v_perm;
	vector unsigned int sum, x;
	unsigned int *p;
	size_t r;
	uint32_t ret;
	unsigned shift;

	prefetch(s);

	v_0  = vec_splat_u8(0);
	sum  = (vector unsigned int)v_0;
	v_44 = vec_splat_u8(4);
	tmp  = vec_splat_u8(0x0a);
	v_a  = (vector unsigned int)vec_or(vec_sl(tmp, v_44), tmp);
	tmp  = vec_splat_u8(0x03);
	v_1  = vec_splat_u32(1);
	v_2  = vec_splat_u32(2);
	v_4  = vec_splat_u32(4);
	v_8  = vec_splat_u32(8);
	v_33 = (vector unsigned int)vec_or(vec_sl(tmp, v_44), tmp);
	v_f  = (vector unsigned int)vec_splat_u8(0x0f);
	v_16 = vec_splat_u32(0x0f);
	tmp  = (vector unsigned char)v_16;
	v_ff = (vector unsigned int)vec_or(vec_sl(tmp, v_44), tmp);

	/*
	 * Sometimes you need a new perspective, like the altivec
	 * way of handling things.
	 * Lower address bits? Totaly overestimated.
	 *
	 * We don't precheck for alignment.
	 * Instead we "align hard", do one load "under the address",
	 * mask the excess info out and afterwards we are fine to go.
	 */
	p = (unsigned int *)ALIGN_DOWN(s, SOVUI);
	shift = ALIGN_DOWN_DIFF(s, SOVUI) * BITS_PER_CHAR;
	c = vec_ldl(0, (vector const unsigned char *)p);
	v_perm = vec_lvsl(0, (unsigned char *)(uintptr_t)s);
	c = vec_perm(c, v_0, v_perm);
	x = (vector unsigned int)c;
	if(len >= SOVUI || len + shift >= SOVUI)
	{
		p += SOVUI/sizeof(*p);
		len -= SOVUI - shift;
		vec_popcnt(x);
		sum = vec_add(sum, x);

		r = len / SOVUI;
		for(; r; r--, p += SOVUI/sizeof(*p)) {
			x = vec_ldl(0, (vector const unsigned int *)p);
			vec_popcnt(x);
			sum = vec_add(sum, x);
		}
		len %= SOVUI;
		if(len)
			x = vec_ldl(0, (vector const unsigned int *)p);
	}
	if(len) {
		v_perm = vec_lvsr(0, (unsigned char *)(uintptr_t)(SOVUI - len));
		c = (vector unsigned char)x;
		c = vec_perm(v_0, c, v_perm);
		x = (vector unsigned int)c;
		vec_popcnt(x);
		sum = vec_add(sum, x);
	}

	sum = (vector unsigned int)vec_sums((vector signed int)sum, (vector signed int)v_0);
	sum = vec_splat(sum, 3); /* splat */
	vec_ste(sum, 0, &ret); /* transfer */
	return ret;
}
# else
#  include "../generic/mempopcnt.c"
# endif
#endif

static char const rcsid_mpp[] GCC_ATTR_USED_VAR = "$Id:$";
