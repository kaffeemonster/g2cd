/*
 * mempopcnt.c
 * popcount a mem region, x86 implementation
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
static size_t mempopcnt_SSE2(const void *s, size_t len);
#ifndef __x86_64__
static size_t mempopcnt_MMX(const void *s, size_t len);
#endif
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

static size_t mempopcnt_SSE2(const void *s, size_t len)
{
	static const uint32_t vals[][4] GCC_ATTR_ALIGNED(16) =
	{
		{0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa},
		{0x33333333, 0x33333333, 0x33333333, 0x33333333},
		{0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f},
		{0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff}
	};
	size_t ret, cnt;

	asm(
		"movdqa	   %4, %%xmm7\n\t"
		"movdqa	16+%4, %%xmm5\n\t"
		"movdqa	32+%4, %%xmm6\n\t"
		"movdqa	48+%4, %%xmm4\n\t"
		"pxor	%%xmm3, %%xmm3\n\t"
		"mov	%2, %1\n\t"
		"and	$-16, %2\n\t"
		"movdqa	(%2), %%xmm0\n\t"
		"sub	%2, %1\n\t"
#ifdef __i386__
# ifndef __PIC__
		"lea	6f(,%1,8), %0\n\t"
# else
		"call	i686_get_pc\n\t"
		"lea	6f-.(%0,%1,8), %0\n\t"
		".subsection 2\n"
		"i686_get_pc:\n\t"
		"movl (%%esp), %0\n\t"
		"ret\n\t"
		".previous\n\t"
# endif
#else
		"lea	6f(%%rip), %0\n\t"
		"lea	(%0,%1,8), %0\n\t"
#endif
		"jmp	*%0\n\t"
		".p2align 3\n"
		"6:\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$1, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$2, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$3, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$4, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$5, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$6, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$7, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$8, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$9, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$10, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$11, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$12, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$13, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$14, %%xmm0\n\t"
		"jmp	7f\n\t"
		".p2align 3\n\t"
		"psrldq	$15, %%xmm0\n"
		"7:\n\t"
		"mov	$16, %0\n\t"
		"sub	%1, %0\n\t"
		"mov	%5, %1\n\t"
		"cmp	%0, %1\n\t"
		"jbe	3f\n\t"
		"sub	%0, %1\n\t"
		"mov	%1, %0\n\t"
		"shr	$4, %1\n\t"
		"inc	%1\n\t"
		"jmp	2f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"movdqa	(%2), %%xmm0\n"
		"2:\n\t"
		"movdqa	%%xmm0, %%xmm1\n\t"
		"pand	%%xmm7, %%xmm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%xmm0\n\t"
		"add	$16, %2\n\t"
		"psubd %%xmm0, %%xmm1\n\t"
		"movdqa	%%xmm1, %%xmm2\n\t"
		"pand	%%xmm5, %%xmm1\n\t" /* 0x33333333 */
		"psrld	$2, %%xmm2\n\t"
		"pand	%%xmm5, %%xmm2\n\t" /* 0x33333333 */
		"paddd	%%xmm1, %%xmm2\n\t"
		"movdqa	%%xmm2, %%xmm0\n\t"
		"psrld	$4, %%xmm0\n\t"
		"paddd	%%xmm2, %%xmm0\n\t"
		"pand	%%xmm6, %%xmm0\n\t" /* 0x0f0f0f0f */
		"movdqa	%%xmm0, %%xmm1\n\t"
		"psrld $8, %%xmm1\n\t"
		"dec	%1\n\t"
		"paddd %%xmm1, %%xmm0\n\t"
		"movdqa	%%xmm0, %%xmm1\n\t"
		"psrld $16, %%xmm1\n\t"
		"paddd	%%xmm1, %%xmm0\n\t"
		"pand	%%xmm4, %%xmm0\n\t" /* 0x000000ff */
		"paddd %%xmm0, %%xmm3\n\t"
		"jnz	1b\n\t"
		"and	$15, %0\n\t"
		"jz	4f\n\t"
		"mov	%0, %1\n\t"
		"movdqa	(%2), %%xmm0\n"
		"3:\n\t"
		"mov	$16, %0\n\t"
		"sub	%1, %0\n\t"
#ifdef __i386__
# ifndef __PIC__
		"lea	8f(,%0,8), %0\n\t"
# else
		"mov	%0, %1\n\t"
		"call	i686_get_pc\n\t"
		"lea	8f-.(%0,%1,8), %0\n\t"
# endif
#else
		"lea	8f(%%rip), %1\n\t"
		"lea	(%1,%0,8), %0\n\t"
#endif
		"jmp	*%0\n\t"
		".p2align 3\n"
		"8:\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$1, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$2, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$3, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$4, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$5, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$6, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$7, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$8, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$9, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$10, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$11, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$12, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$13, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$14, %%xmm0\n\t"
		"jmp	9f\n\t"
		".p2align 3\n\t"
		"pslldq	$15, %%xmm0\n"
		"9:\n\t"
		"movdqa	%%xmm0, %%xmm1\n\t"
		"pand	%%xmm7, %%xmm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%xmm0\n\t"
		"psubd %%xmm0, %%xmm1\n\t"
		"movdqa	%%xmm1, %%xmm2\n\t"
		"pand	%%xmm5, %%xmm1\n\t" /* 0x33333333 */
		"psrld	$2, %%xmm2\n\t"
		"pand	%%xmm5, %%xmm2\n\t" /* 0x33333333 */
		"paddd	%%xmm1, %%xmm2\n\t"
		"movdqa	%%xmm2, %%xmm0\n\t"
		"psrld	$4, %%xmm0\n\t"
		"paddd	%%xmm2, %%xmm0\n\t"
		"pand	%%xmm6, %%xmm0\n\t" /* 0x0f0f0f0f */
		"movdqa	%%xmm0, %%xmm1\n\t"
		"psrld $8, %%xmm1\n\t"
		"paddd %%xmm1, %%xmm0\n\t"
		"movdqa	%%xmm0, %%xmm1\n\t"
		"psrld $16, %%xmm1\n\t"
		"paddd	%%xmm1, %%xmm0\n\t"
		"pand	%%xmm4, %%xmm0\n\t" /* 0x000000ff */
		"paddd %%xmm0, %%xmm3\n"
		"4:\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"movdqa	%%xmm3, %%xmm2\n\t"
		"punpckhdq	%%xmm0, %%xmm3\n\t"
		"punpckldq	%%xmm0, %%xmm2\n\t"
		"paddq	%%xmm3, %%xmm2\n\t"
		"movdqa	%%xmm2, %%xmm0\n\t"
		"punpckhqdq	%%xmm0, %%xmm0\n\t"
		"paddq	%%xmm2, %%xmm0\n\t"
#ifdef __x86_64__
		"movq	%%xmm0, %0\n\t"
#else
		"movd	%%xmm0, %0\n\t"
#endif
	: /* %0 */ "=r" (ret),
	  /* %1 */ "=r" (cnt),
	  /* %2 */ "=r" (s)
	: /* %3 */ "2" (s),
	  /* %4 */ "m" (**vals),
	  /* %5 */ "m" (len)
#ifdef __SSE__
	: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
	);

	return ret;
}

#ifndef __x86_64__
static size_t mempopcnt_MMX(const void *s, size_t len)
{
	static const uint32_t vals[][2] GCC_ATTR_ALIGNED(8) =
	{
		{0xaaaaaaaa, 0xaaaaaaaa},
		{0x33333333, 0x33333333},
		{0x0f0f0f0f, 0x0f0f0f0f},
		{0x000000ff, 0x000000ff}
	};
	size_t ret, cnt;

	asm(
		"movq	   %4, %%mm7\n\t"
		"movq	 8+%4, %%mm5\n\t"
		"movq	16+%4, %%mm6\n\t"
		"movq	24+%4, %%mm4\n\t"
		"pxor	%%mm3, %%mm3\n\t"
		"pxor	%%mm2, %%mm2\n\t"
		"xor	%0, %0\n\t"
		"mov	%2, %1\n\t"
		"and	$-8, %2\n\t"
		"movq	(%2), %%mm0\n\t"
		"sub	%2, %1\n\t"
		"lea	(%0,%1,8), %0\n\t"
		"movd	%0, %%mm2\n\t"
		"psrlq	%%mm2, %%mm0\n\t"
		"mov	$8, %0\n\t"
		"sub	%1, %0\n\t"
		"mov	%5, %1\n\t"
		"sub	%0, %1\n\t"
		"jbe 3f\n\t"
		"mov	%1, %0\n\t"
		"shr	$3, %1\n\t"
		"inc	%1\n\t"
		"jmp	2f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"movq	(%2), %%mm0\n"
		"2:\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"pand	%%mm7, %%mm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%mm0\n\t"
		"add	$8, %2\n\t"
		"psubd %%mm0, %%mm1\n\t"
		"movq	%%mm1, %%mm2\n\t"
		"pand	%%mm5, %%mm1\n\t" /* 0x33333333 */
		"psrld	$2, %%mm2\n\t"
		"pand	%%mm5, %%mm2\n\t" /* 0x33333333 */
		"paddd	%%mm1, %%mm2\n\t"
		"movq	%%mm2, %%mm0\n\t"
		"psrld	$4, %%mm0\n\t"
		"paddd	%%mm2, %%mm0\n\t"
		"pand	%%mm6, %%mm0\n\t" /* 0x0f0f0f0f */
		"movq	%%mm0, %%mm1\n\t"
		"psrld $8, %%mm1\n\t"
		"dec	%1\n\t"
		"paddd %%mm1, %%mm0\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"psrld $16, %%mm1\n\t"
		"paddd	%%mm1, %%mm0\n\t"
		"pand	%%mm4, %%mm0\n\t" /* 0x000000ff */
		"paddd %%mm0, %%mm3\n\t"
		"jnz	1b\n\t"
		"and	$7, %0\n\t"
		"jz	4f\n\t"
		"lea	-8(%0), %1\n\t"
		"movq	(%2), %%mm0\n\t"
		"pxor	%%mm2, %%mm2\n"
		"3:\n\t"
		"psllq	%%mm2, %%mm0\n\t"
		"neg	%1\n\t"
		"shl	$3, %1\n\t"
		"movd	%1, %%mm2\n\t"
		"psllq	%%mm2, %%mm0\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"pand	%%mm7, %%mm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%mm0\n\t"
		"psubd %%mm0, %%mm1\n\t"
		"movq	%%mm1, %%mm2\n\t"
		"pand	%%mm5, %%mm1\n\t" /* 0x33333333 */
		"psrld	$2, %%mm2\n\t"
		"pand	%%mm5, %%mm2\n\t" /* 0x33333333 */
		"paddd	%%mm1, %%mm2\n\t"
		"movq	%%mm2, %%mm0\n\t"
		"psrld	$4, %%mm0\n\t"
		"paddd	%%mm2, %%mm0\n\t"
		"pand	%%mm6, %%mm0\n\t" /* 0x0f0f0f0f */
		"movq	%%mm0, %%mm1\n\t"
		"psrld $8, %%mm1\n\t"
		"paddd %%mm1, %%mm0\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"psrld $16, %%mm1\n\t"
		"paddd	%%mm1, %%mm0\n\t"
		"pand	%%mm4, %%mm0\n\t" /* 0x000000ff */
		"paddd %%mm0, %%mm3\n"
		"4:\n\t"
		"movq	%%mm3, %%mm0\n\t"
		"punpckhdq	%%mm0, %%mm0\n\t"
		"paddd	%%mm3, %%mm0\n\t"
		"movd	%%mm0, %0\n\t"
	: /* %0 */ "=r" (ret),
	  /* %1 */ "=r" (cnt),
	  /* %2 */ "=r" (s)
	: /* %3 */ "2" (s),
	  /* %4 */ "m" (**vals),
	  /* %5 */ "m" (len)
#ifdef __MMX__
	: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
#endif
	);

	return ret;
}
#endif

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
	{.func = (void (*)(void))mempopcnt_SSE2, .flags_needed = CFEATURE_SSE2},
#ifndef __x86_64__
	{.func = (void (*)(void))mempopcnt_MMX, .flags_needed = CFEATURE_MMX},
#endif
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
