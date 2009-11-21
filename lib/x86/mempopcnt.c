/*
 * mempopcnt.c
 * popcount a mem region, generic implementation
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

#include "x86_features.h"

static size_t mempopcnt_SSE4(const void *s, size_t len);
static size_t mempopcnt_generic(const void *s, size_t len);

static inline size_t popcountst_intSSE4(size_t n)
{
	size_t tmp;
	__asm__ ("popcnt	%1, %0" : "=r" (tmp) : "g" (n): "cc");
	return tmp;
}

static size_t mempopcnt_SSE4(const void *s, size_t len)
{
	const unsigned char *p;
	size_t r;
	size_t sum = 0;
	unsigned shift = ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR;
	prefetch(s);

	p = (const unsigned char *)ALIGN_DOWN(s, SOST);
	r = *(const size_t *)p;
	r >>= shift;
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
		sum += popcountst_intSSE4(r);

		r = len / SOST;
		for(; r; r--, p += SOST)
			sum += popcountst_intSSE4(*(const size_t *)p);
		len %= SOST;
		if(len)
			r = *(const size_t *)p;
	}
	if(len) {
		r <<= SOST - len;
		sum += popcountst_intSSE4(r);
	}
	return sum;
}

static inline size_t popcountst_int(size_t n)
{
	n -= (n & MK_C(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_C(0x33333333L)) + (n & MK_C(0x33333333L));
	n = ((n >> 4) + n) & MK_C(0x0f0f0f0fL);
#ifdef HAVE_HW_MULT
	n = (n * MK_C(0x01010101L)) >> (SIZE_T_BITS - 8);
#else
	n = ((n >> 8) + n);
	n = ((n >> 16) + n);
	if(SIZE_T_BITS >= 64)
		n = ((n >> 32) + n);
	n &= 0xff;
#endif
	return n;
}

static size_t mempopcnt_generic(const void *s, size_t len)
{
	const unsigned char *p;
	size_t r;
	size_t sum = 0;
	unsigned shift = ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR;
	prefetch(s);

	p = (const unsigned char *)ALIGN_DOWN(s, SOST);
	r = *(const size_t *)p;
	r >>= shift;
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
		r <<= SOST - len;
		sum += popcountst_int(r);
	}
	return sum;
}

static const struct test_cpu_feature t_feat[] =
{
	{.func = (void (*)(void))mempopcnt_SSE4, .flags_needed = CFEATURE_POPCNT},
	{.func = (void (*)(void))mempopcnt_SSE4, .flags_needed = CFEATURE_SSE4A},
	{.func = (void (*)(void))mempopcnt_generic, .flags_needed = -1 },
};

static size_t mempopcnt_runtime_sw(const void *s, size_t n);
/*
 * Func ptr
 */
static size_t (*mempopcnt_ptr)(const void *s, size_t n) = mempopcnt_runtime_sw;

/*
 * constructor
 */
static void mempopcnt_select(void) GCC_ATTR_CONSTRUCT;
static void mempopcnt_select(void)
{
	mempopcnt_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer failes
 */
static size_t mempopcnt_runtime_sw(const void *s, size_t n)
{
	mempopcnt_select();
	return mempopcnt(s, n);
}

/*
 * trampoline
 */
size_t mempopcnt(const void *s, size_t n)
{
	return mempopcnt_ptr(s, n);
}

static char const rcsid_mpx[] GCC_ATTR_USED_VAR = "$Id:$";
