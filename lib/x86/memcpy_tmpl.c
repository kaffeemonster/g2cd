/*
 * memcpy_templ.c
 * memcpy - x86 version, template
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

#include "x86.h"

static GCC_ATTR_FASTCALL void *DFUNC_NAME(memcpy, ARCH_NAME_SUFFIX)(void *restrict dst, const void *restrict src, size_t len);

static GCC_ATTR_FASTCALL void *DFUNC_NAME(memcpy, ARCH_NAME_SUFFIX)(void *restrict dst, const void *restrict src, size_t len)
{
	size_t i;
	char *restrict old_dst = dst;

	__asm__ __volatile__(
		PREFETCH(  (%0))
		PREFETCH(32(%0))
		PREFETCH(64(%0))
		PREFETCH(96(%0))
		PREFETCHW(  (%1))
		PREFETCHW(32(%1))
		PREFETCHW(64(%1))
		PREFETCHW(96(%1))
		"" : /* no out */ : "r" (src), "r" (dst)
	);

	i = ALIGN_DIFF((char *)dst, 16);
	len -= i;
	asm (
		"rep	movsb"
		: "=D" (dst), "=S" (src), "=c" (i), "=m" (*(char *)dst)
		: "0" (dst), "1" (src), "2" (i), "m" (*(const char *)src)
	);
	if(likely(!IS_ALIGN((const char *)src, 16)))
	{
		if(likely(!IS_ALIGN((const char *)src, 8)))
		{
			asm (
				"and	$127, %2\n\t"
				"shr	$7, %3\n\t"
				"jz	2f\n\t"
				"add	$64, %1\n\t"
				"sub	$64, %0\n\t"
				".p2align 3\n"
				"1:\n\t"
				"sub	$-128, %0\n\t"
				SSE_PREFETCH(112(%1))
				SSE_LOAD(-64(%1), %%xmm0)
				SSE_LOAD(-48(%1), %%xmm1)
				SSE_LOAD(-32(%1), %%xmm2)
				SSE_LOAD(-16(%1), %%xmm3)
				SSE_LOAD(   (%1), %%xmm4)
				SSE_LOAD( 16(%1), %%xmm5)
				SSE_LOAD( 32(%1), %%xmm6)
				SSE_LOAD( 48(%1), %%xmm7)
				"sub	$-128, %1\n\t"
				"dec	%3\n\t"
#ifdef WANT_BIG
				SSE_STORE(%%xmm0,-64(%0))
				SSE_STORE(%%xmm1,-48(%0))
				SSE_STORE(%%xmm2,-32(%0))
				SSE_STORE(%%xmm3,-16(%0))
				SSE_STORE(%%xmm4,   (%0))
				SSE_STORE(%%xmm5, 16(%0))
				SSE_STORE(%%xmm6, 32(%0))
				SSE_STORE(%%xmm7, 48(%0))
#else
				SSE_PREFETCHW(112(%0))
				SSE_MOVE(%%xmm0,-64(%0))
				SSE_MOVE(%%xmm1,-48(%0))
				SSE_MOVE(%%xmm2,-32(%0))
				SSE_MOVE(%%xmm3,-16(%0))
				SSE_MOVE(%%xmm4,   (%0))
				SSE_MOVE(%%xmm5, 16(%0))
				SSE_MOVE(%%xmm6, 32(%0))
				SSE_MOVE(%%xmm7, 48(%0))
#endif
				"jnz	1b\n"
				"sub	$64, %1\n\t"
				"add	$64, %0\n\t"
				"2:\n\t"
				"mov	%2, %3\n\t"
				"and	$15, %2\n\t"
				"shr	$4, %3\n\t"
				"jz	4f\n\t"
				".p2align 2\n"
				"3:\n\t"
				SSE_LOAD((%1), %%xmm0)
				"add	$16, %1\n\t"
				"dec	%3\n\t"
#ifdef WANT_BIG
				SSE_STORE(%%xmm0, (%0))
#else
				SSE_MOVE(%%xmm0, (%0))
#endif
				"lea	16(%0), %0\n\t"
				"jnz	3b\n"
				"4:\n\t"
				: "=D" (dst), "=S" (src), "=r" (len), "=r" (i), "=m" (*(char *)dst)
				: "0" (dst), "1" (src), "2" (len), "3" (len), "m" (*(const char *)src)
			);
		}
		else
		{
			asm (
				"and	$127, %2\n\t"
				"shr	$7, %3\n\t"
				"jz	2f\n\t"
				"add	$64, %1\n\t"
				"sub	$64, %0\n\t"
				".p2align 3\n"
				"1:\n\t"
				"sub	$-128, %0\n\t"
				SSE_PREFETCH(112(%1))
				SSE_LOAD8(-64(%1), %%xmm0)
				SSE_LOAD8(-48(%1), %%xmm1)
				SSE_LOAD8(-32(%1), %%xmm2)
				SSE_LOAD8(-16(%1), %%xmm3)
				SSE_LOAD8(  0(%1), %%xmm4)
				SSE_LOAD8( 16(%1), %%xmm5)
				SSE_LOAD8( 32(%1), %%xmm6)
				SSE_LOAD8( 48(%1), %%xmm7)
				"sub	$-128, %1\n\t"
				"dec	%3\n\t"
#ifdef WANT_BIG
				SSE_STORE(%%xmm0,-64(%0))
				SSE_STORE(%%xmm1,-48(%0))
				SSE_STORE(%%xmm2,-32(%0))
				SSE_STORE(%%xmm3,-16(%0))
				SSE_STORE(%%xmm4,   (%0))
				SSE_STORE(%%xmm5, 16(%0))
				SSE_STORE(%%xmm6, 32(%0))
				SSE_STORE(%%xmm7, 48(%0))
#else
				SSE_PREFETCHW(112(%0))
				SSE_MOVE(%%xmm0,-64(%0))
				SSE_MOVE(%%xmm1,-48(%0))
				SSE_MOVE(%%xmm2,-32(%0))
				SSE_MOVE(%%xmm3,-16(%0))
				SSE_MOVE(%%xmm4,   (%0))
				SSE_MOVE(%%xmm5, 16(%0))
				SSE_MOVE(%%xmm6, 32(%0))
				SSE_MOVE(%%xmm7, 48(%0))
#endif
				"jnz	1b\n"
				"sub	$64, %1\n\t"
				"add	$64, %0\n\t"
				"2:\n\t"
				"mov	%2, %3\n\t"
				"and	$15, %2\n\t"
				"shr	$4, %3\n\t"
				"jz	4f\n\t"
				".p2align 2\n"
				"3:\n\t"
				SSE_LOAD8(0(%1), %%xmm0)
				"add	$16, %1\n\t"
				"dec	%3\n\t"
#ifdef WANT_BIG
				SSE_STORE(%%xmm0, (%0))
#else
				SSE_MOVE(%%xmm0, (%0))
#endif
				"lea	16(%0), %0\n\t"
				"jnz	3b\n"
				"4:\n\t"
				: "=D" (dst), "=S" (src), "=r" (len), "=r" (i), "=m" (*(char *)dst)
				: "0" (dst), "1" (src), "2" (len), "3" (len), "m" (*(const char *)src)
			);
		}
	}
	else
	{
		asm (
			"and	$127, %2\n\t"
			"shr	$7, %3\n\t"
			"jz	2f\n\t"
			"add	$64, %1\n\t"
			"sub	$64, %0\n\t"
			".p2align 3\n"
			"1:\n\t"
			"sub	$-128, %0\n\t"
			SSE_PREFETCH(112(%1))
			SSE_MOVE(-64(%1), %%xmm0)
			SSE_MOVE(-48(%1), %%xmm1)
			SSE_MOVE(-32(%1), %%xmm2)
			SSE_MOVE(-16(%1), %%xmm3)
			SSE_MOVE(   (%1), %%xmm4)
			SSE_MOVE( 16(%1), %%xmm5)
			SSE_MOVE( 32(%1), %%xmm6)
			SSE_MOVE( 48(%1), %%xmm7)
			"sub	$-128, %1\n\t"
			"dec	%3\n\t"
#ifdef WANT_BIG
			SSE_STORE(%%xmm0,-64(%0))
			SSE_STORE(%%xmm1,-48(%0))
			SSE_STORE(%%xmm2,-32(%0))
			SSE_STORE(%%xmm3,-16(%0))
			SSE_STORE(%%xmm4,   (%0))
			SSE_STORE(%%xmm5, 16(%0))
			SSE_STORE(%%xmm6, 32(%0))
			SSE_STORE(%%xmm7, 48(%0))
#else
			SSE_PREFETCHW(112(%0))
			SSE_MOVE(%%xmm0,-64(%0))
			SSE_MOVE(%%xmm1,-48(%0))
			SSE_MOVE(%%xmm2,-32(%0))
			SSE_MOVE(%%xmm3,-16(%0))
			SSE_MOVE(%%xmm4,   (%0))
			SSE_MOVE(%%xmm5, 16(%0))
			SSE_MOVE(%%xmm6, 32(%0))
			SSE_MOVE(%%xmm7, 48(%0))
#endif
			"jnz	1b\n"
			"sub	$64, %1\n\t"
			"add	$64, %0\n\t"
			"2:\n\t"
			"mov	%2, %3\n\t"
			"and	$15, %2\n\t"
			"shr	$4, %3\n\t"
			"jz	4f\n\t"
			".p2align 2\n"
			"3:\n\t"
			SSE_MOVE((%1), %%xmm0)
			"add	$16, %1\n\t"
			"dec	%3\n\t"
#ifdef WANT_BIG
			SSE_STORE(%%xmm0,   (%0))
#else
			SSE_MOVE(%%xmm0,   (%0))
#endif
			"lea	16(%0), %0\n\t"
			"jnz	3b\n"
			"4:\n\t"
			: "=D" (dst), "=S" (src), "=r" (len), "=r" (i), "=m" (*(char *)dst)
			: "0" (dst), "1" (src), "2" (len), "3" (len), "m" (*(const char *)src)
		);
	}
	asm (
		SSE_FENCE
		"and	$3, %3\n\t"
		"shr	$2, %2\n\t"
		"rep	movsd\n\t"
		"mov	%3, %2\n\t"
		"rep	movsb"
		: "=D" (dst), "=S" (src), "=c" (len), "=r" (i), "=m" (*(char *)dst)
		: "0" (dst), "1" (src), "2" (len), "3" (len), "m" (*(const char *)src)
	);

	return old_dst;
}

/* EOF */
