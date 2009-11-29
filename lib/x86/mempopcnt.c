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
static size_t mempopcnt_SSSE3(const void *s, size_t len);
static size_t mempopcnt_SSE2(const void *s, size_t len);
#ifndef __x86_64__
static size_t mempopcnt_SSE(const void *s, size_t len);
static size_t mempopcnt_MMX(const void *s, size_t len);
#endif
static size_t mempopcnt_generic(const void *s, size_t len);

#ifdef __x86_64__
# define CL "&"
# define CO "r"
# define DI "8"
# define BX "%%rbx"
#else
# define CL
# define CO "m"
# define DI "4"
# define BX "%%ebx"
#endif

static const uint8_t reg_num_mask[16] GCC_ATTR_ALIGNED(16) =
		{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
static const uint32_t vals[][4] GCC_ATTR_ALIGNED(16) =
{
	{0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa},
	{0x33333333, 0x33333333, 0x33333333, 0x33333333},
	{0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f},
	{0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff}
};

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
	unsigned shift = ALIGN_DOWN_DIFF(s, SOST);
	prefetch(s);

	p = (const unsigned char *)ALIGN_DOWN(s, SOST);
	r = *(const size_t *)p;
	r >>= shift * BITS_PER_CHAR;
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

		r = len / (SOST * 4);
		if(r)
		{
			size_t t1, t2;
#ifndef __PIC__
			size_t t3;
#endif
			asm (
#ifdef __PIC__
				"push	"BX"\n"
#endif
				".p2align	2\n\t"
				"1:\n\t"
				"prefetchnta	0x60(%1)\n\t"
				"popcnt	0*"DI"(%1), %4\n\t"
				"lea	(%2, %4), %3\n\t"
				"popcnt	1*"DI"(%1), "BX"\n\t"
				"lea	(%3, "BX"), %4\n\t"
				"popcnt	2*"DI"(%1), %2\n\t"
				"lea	(%4, %2), "BX"\n\t"
				"popcnt	3*"DI"(%1), %3\n\t"
				"lea	("BX", %3), %2\n\t"
				"dec	%0\n\t"
				"lea	4*"DI"(%1), %1\n\t"
				"jnz	1b\n\t"
#ifdef __PIC__
				"pop "BX"\n\t"
#endif
				: /* %0 */ "=r" (r),
				  /* %1 */ "=r" (p),
				  /* %2 */ "=r" (sum),
				  /* %3 */ "=r" (t1),
#ifndef __PIC__
				  /* %4 */ "=r" (t2),
				  /* %5 */ "=b" (t3)
#else
				  /* %4 */ "=r" (t2)
#endif
				: /* %6 */ "0" (r),
				  /* %7 */ "4" (sum),
				  /* %8 */ "5" (p)
			);
		}
		len %= SOST * 4;
		switch(len / SOST)
		{
		case 3:
			sum += popcountst_intSSE4(*(const size_t *)p);
			p += SOST;
		case 2:
			sum += popcountst_intSSE4(*(const size_t *)p);
			p += SOST;
		case 1:
			sum += popcountst_intSSE4(*(const size_t *)p);
			p += SOST;
		case 0:
			break;
		}
		len %= SOST;
		if(len)
			r = *(const size_t *)p;
	}
	if(len) {
		r <<= (SOST - len) * BITS_PER_CHAR;
		sum += popcountst_intSSE4(r);
	}
	return sum;
}

static size_t mempopcnt_SSSE3(const void *s, size_t len)
{
	static const uint8_t lut_st[16] GCC_ATTR_ALIGNED(16) =
		{0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
	static const uint8_t sh_mask[16] GCC_ATTR_ALIGNED(16) =
		{0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f};
	size_t ret, cnt1, cnt2;

	asm(
		"prefetchnta	(%3)\n\t"
		"prefetchnta	0x20(%3)\n\t"
		"prefetchnta	0x40(%3)\n\t"
		"movdqa	%5, %%xmm7\n\t" /* lut */
		"movdqa	%8, %%xmm6\n\t" /* 0x0f0f0f0f0f */
		"pxor	%%xmm5, %%xmm5\n\t" /* sum */
		"movdqa	%7, %%xmm1\n\t"
		"mov	$16, %0\n\t"
		"mov	%3, %1\n\t"
		"and	$-16, %3\n\t"
		"movdqa	(%3), %%xmm0\n\t"
		"sub	%3, %1\n\t"
		"sub	%1, %0\n\t"
		"imul	$0x01010101, %1\n\t"
		"movd	%1, %%xmm2\n\t"
		"mov	%6, %1\n\t"
		"pshufd	$0, %%xmm2, %%xmm2\n\t"
		"pcmpgtb	%%xmm2, %%xmm1\n\t"
		"pand	%%xmm1, %%xmm0\n\t"
		"sub	%0, %1\n\t"
		"mov	%1, %0\n\t"
		"jbe	9f\n\t"
		"shr	$5, %1\n"
		"jz	7f\n\t"
		"movdqa	%%xmm0, %%xmm1\n\t"
		"movdqa	%%xmm6, %%xmm3\n\t"
		"pand	%%xmm6, %%xmm0\n\t"
		"pandn	%%xmm1, %%xmm3\n\t"
		"movdqa	%%xmm7, %%xmm1\n\t"
		"movdqa	%%xmm7, %%xmm2\n\t"
		"psrlw	$4, %%xmm3\n\t"
		"pshufb	%%xmm0, %%xmm1\n\t"
		"add	$16, %3\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"pshufb	%%xmm3, %%xmm2\n\t"
		"paddb	%%xmm1, %%xmm2\n\t"
		"psadbw	%%xmm0, %%xmm2\n\t"
		"paddq %%xmm2, %%xmm5\n"
		"1:\n\t"
		"test	%1, %1\n\t"
		"jz	5f\n\t"
		"mov	$15, %2\n\t"
		"cmp	%2, %1\n\t"
		"cmovb	%1, %2\n\t"
		"sub	%2, %1\n\t"
		"pxor	%%xmm4, %%xmm4\n\t"
		".p2align 2\n"
		"2:\n\t"
		"prefetchnta	0x60(%3)\n\t"
		"movdqa	(%3), %%xmm0\n"
		"movdqa	%%xmm6, %%xmm1\n\t"
		"pandn	%%xmm0, %%xmm1\n\t"
		"pand	%%xmm6, %%xmm0\n\t"
		"psrlw	$4, %%xmm1\n\t"
		"movdqa	%%xmm7, %%xmm3\n\t"
		"movdqa	%%xmm7, %%xmm2\n\t"
		"pshufb	%%xmm0, %%xmm3\n\t"
		"movdqa	16(%3), %%xmm0\n\t"
		"add	$32, %3\n\t"
		"pshufb	%%xmm1, %%xmm2\n\t"
		"movdqa	%%xmm6, %%xmm1\n\t"
		"paddb	%%xmm3, %%xmm4\n\t"
		"pandn	%%xmm0, %%xmm1\n\t"
		"movdqa	%%xmm7, %%xmm3\n\t"
		"paddb	%%xmm2, %%xmm4\n\t"
		"movdqa	%%xmm7, %%xmm2\n\t"
		"dec	%2\n\t"
		"pand	%%xmm6, %%xmm0\n\t"
		"psrlw	$4, %%xmm1\n\t"
		"pshufb	%%xmm0, %%xmm3\n\t"
		"pshufb	%%xmm1, %%xmm2\n\t"
		"paddb	%%xmm3, %%xmm4\n\t"
		"paddb	%%xmm2, %%xmm4\n\t"
		"jnz	2b\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"psadbw	%%xmm0, %%xmm4\n\t"
		"paddq %%xmm4, %%xmm5\n\t"
		"jmp	1b\n"
		"7:\n\t"
		"and	$31, %0\n\t"
		"jz	4f\n\t"
		"jmp	8f\n"
		"9:\n\t"
		"add	$16, %0\n\t"
		"jmp	3f\n"
		"5:\n\t"
		"and	$31, %0\n\t"
		"jz	4f\n\t"
		"cmp	$16, %0\n\t"
		"jb	6f\n\t"
		"movdqa	(%3), %%xmm0\n"
		"8:\n\t"
		"movdqa	%%xmm0, %%xmm1\n\t"
		"movdqa	%%xmm6, %%xmm3\n\t"
		"pand	%%xmm6, %%xmm0\n\t"
		"add	$16, %3\n\t"
		"pandn	%%xmm1, %%xmm3\n\t"
		"movdqa	%%xmm7, %%xmm1\n\t"
		"movdqa	%%xmm7, %%xmm2\n\t"
		"psrlw	$4, %%xmm3\n\t"
		"pshufb	%%xmm0, %%xmm1\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"pshufb	%%xmm3, %%xmm2\n\t"
		"paddb	%%xmm1, %%xmm2\n\t"
		"psadbw	%%xmm0, %%xmm2\n\t"
		"paddq %%xmm2, %%xmm5\n"
		"6:\n\t"
		"and	$15, %0\n\t"
		"jz	4f\n\t"
		"mov	%0, %1\n\t"
		"movdqa	(%3), %%xmm0\n"
		"3:\n\t"
		"movdqa	%7, %%xmm1\n\t"
		"imul	$0x01010101, %0\n\t"
		"movd	%0, %%xmm2\n\t"
		"pshufd	$0, %%xmm2, %%xmm2\n\t"
		"pcmpgtb	%%xmm2, %%xmm1\n\t"
		"pandn	%%xmm0, %%xmm1\n\t"
		"movdqa	%%xmm1, %%xmm0\n\t"
		"movdqa	%%xmm6, %%xmm3\n\t"
		"pand	%%xmm6, %%xmm0\n\t"
		"add	$16, %3\n\t"
		"pandn	%%xmm1, %%xmm3\n\t"
		"movdqa	%%xmm7, %%xmm1\n\t"
		"movdqa	%%xmm7, %%xmm2\n\t"
		"psrlw	$4, %%xmm3\n\t"
		"pshufb	%%xmm0, %%xmm1\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"pshufb	%%xmm3, %%xmm2\n\t"
		"paddb	%%xmm1, %%xmm2\n\t"
		"psadbw	%%xmm0, %%xmm2\n\t"
		"paddq %%xmm2, %%xmm5\n"
		"4:\n\t"
		"movdqa	%%xmm5, %%xmm0\n\t"
		"punpckhqdq	%%xmm0, %%xmm0\n\t"
		"paddq	%%xmm5, %%xmm0\n\t"
#ifdef __x86_64__
		"movq	%%xmm0, %0\n\t"
#else
		"movd	%%xmm0, %0\n\t"
#endif
	: /* %0 */ "="CL"r" (ret),
	  /* %1 */ "="CL"r" (cnt1),
	  /* %2 */ "="CL"r" (cnt2),
	  /* %3 */ "="CL"r" (s)
	: /* %4 */ "3" (s),
	  /* %5 */ "m" (*lut_st),
	  /* %6 */ CO  (len),
	  /* %7 */ "m" (*reg_num_mask),
	  /* %8 */ "m" (*sh_mask)
#ifdef __SSE__
	: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
	);

	return ret;
}

static size_t mempopcnt_SSE2(const void *s, size_t len)
{
	size_t ret, cnt, t;

	asm(
		"prefetchnta	(%2)\n\t"
		"prefetchnta	0x20(%2)\n\t"
		"prefetchnta	0x40(%2)\n\t"
		"movdqa	   %5, %%xmm7\n\t"
		"movdqa	16+%5, %%xmm5\n\t"
		"movdqa	32+%5, %%xmm6\n\t"
		"pxor	%%xmm3, %%xmm3\n\t"
		"pxor	%%xmm2, %%xmm2\n\t"
		"movdqa	%7, %%xmm1\n\t"
		"mov	$16, %0\n\t"
		"mov	%2, %1\n\t"
		"and	$-16, %2\n\t"
		"movdqa	(%2), %%xmm0\n\t"
		"sub	%2, %1\n\t"
		"sub	%1, %0\n\t"
		"imul	$0x01010101, %1\n\t"
		"movd	%1, %%xmm2\n\t"
		"mov	%6, %1\n\t"
		"pshufd	$0, %%xmm2, %%xmm2\n\t"
		"pcmpgtb	%%xmm2, %%xmm1\n\t"
		"pand	%%xmm1, %%xmm0\n\t"
		"sub	%0, %1\n\t"
		"mov	%1, %0\n\t"
		"jbe	9f\n\t"
		"shr	$5, %1\n\t"
		"jz	1f\n\t"
		"movdqa	%%xmm0, %%xmm1\n\t"
		"pand	%%xmm7, %%xmm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%xmm0\n\t"
		"add	$16, %2\n\t"
		"psubd	%%xmm0, %%xmm1\n\t"
		"movdqa	%%xmm1, %%xmm0\n\t"
		"pand	%%xmm5, %%xmm1\n\t" /* 0x33333333 */
		"psrld	$2, %%xmm0\n\t"
		"pand	%%xmm5, %%xmm0\n\t" /* 0x33333333 */
		"paddd	%%xmm0, %%xmm1\n\t"
		"pxor	%%xmm2, %%xmm2\n\t"
		"movdqa	%%xmm1, %%xmm0\n\t"
		"psrld	$4, %%xmm0\n\t"
		"paddd	%%xmm1, %%xmm0\n\t"
		"pand	%%xmm6, %%xmm0\n\t" /* 0x0f0f0f0f */
		"psadbw	%%xmm2, %%xmm0\n\t"
		"paddq %%xmm0, %%xmm3\n"
		"2:\n\t"
		"mov	$15, %3\n\t"
		"cmp	%3, %1\n\t"
		"cmovb	%1, %3\n\t"
		"sub	%3, %1\n\t"
		"pxor	%%xmm4, %%xmm4\n\t"
		".p2align 2\n"
		"22:\n\t"
		"prefetchnta	0x60(%2)\n\t"
		"movdqa	(%2), %%xmm1\n"
		"movdqa	16(%2), %%xmm2\n"
		"movdqa	%%xmm1, %%xmm0\n\t"
		"pand	%%xmm7, %%xmm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%xmm0\n\t"
		"add	$32, %2\n\t"
		"psubd	%%xmm0, %%xmm1\n\t"
		"movdqa	%%xmm2, %%xmm0\n\t"
		"pand	%%xmm7, %%xmm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%xmm0\n\t"
		"psubd %%xmm0, %%xmm2\n\t"
		"movdqa	%%xmm1, %%xmm0\n\t"
		"pand	%%xmm5, %%xmm1\n\t" /* 0x33333333 */
		"psrld	$2, %%xmm0\n\t"
		"pand	%%xmm5, %%xmm0\n\t" /* 0x33333333 */
		"paddd	%%xmm0, %%xmm1\n\t"
		"movdqa	%%xmm2, %%xmm0\n\t"
		"dec	%3\n\t"
		"pand	%%xmm5, %%xmm2\n\t" /* 0x33333333 */
		"psrld	$2, %%xmm0\n\t"
		"pand	%%xmm5, %%xmm0\n\t" /* 0x33333333 */
		"paddd	%%xmm0, %%xmm2\n\t"
		"paddd	%%xmm1, %%xmm2\n\t"
		"movdqa	%%xmm2, %%xmm0\n\t"
		"pand	%%xmm6, %%xmm2\n\t" /* 0x0f0f0f0f */
		"psrld	$4, %%xmm0\n\t"
		"pand	%%xmm6, %%xmm0\n\t" /* 0x0f0f0f0f */
		"paddb	%%xmm0, %%xmm2\n\t"
		"paddb	%%xmm2, %%xmm4\n\t"
		"jnz	22b\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"psadbw	%%xmm0, %%xmm4\n\t"
		"paddq %%xmm4, %%xmm3\n\t"
		"test	%1, %1\n\t"
		"jnz	2b\n\t"
		"jmp	5f\n"
		"1:\n\t"
		"and	$31, %0\n\t"
		"jz	4f\n\t"
		"jmp	8f\n"
		"9:\n\t"
		"add	$16, %0\n\t"
		"jmp	3f\n"
		"5:\n\t"
		"and	$31, %0\n\t"
		"jz	4f\n\t"
		"cmp	$16, %0\n\t"
		"jb	6f\n\t"
		"movdqa	(%2), %%xmm0\n"
		"8:\n\t"
		"movdqa	%%xmm0, %%xmm1\n\t"
		"pand	%%xmm7, %%xmm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%xmm0\n\t"
		"add	$16, %2\n\t"
		"psubd	%%xmm0, %%xmm1\n\t"
		"movdqa	%%xmm1, %%xmm2\n\t"
		"pand	%%xmm5, %%xmm1\n\t" /* 0x33333333 */
		"psrld	$2, %%xmm2\n\t"
		"pand	%%xmm5, %%xmm2\n\t" /* 0x33333333 */
		"paddd	%%xmm1, %%xmm2\n\t"
		"pxor	%%xmm1, %%xmm1\n\t"
		"movdqa	%%xmm2, %%xmm0\n\t"
		"psrld	$4, %%xmm0\n\t"
		"paddd	%%xmm2, %%xmm0\n\t"
		"pand	%%xmm6, %%xmm0\n\t" /* 0x0f0f0f0f */
		"psadbw	%%xmm1, %%xmm0\n\t"
		"paddq %%xmm0, %%xmm3\n"
		"6:\n\t"
		"and	$15, %0\n\t"
		"jz	4f\n\t"
		"mov	%0, %1\n\t"
		"movdqa	(%2), %%xmm0\n"
		"3:\n\t"
		"movdqa	%7, %%xmm1\n\t"
		"imul	$0x01010101, %0\n\t"
		"movd	%0, %%xmm2\n\t"
		"pshufd	$0, %%xmm2, %%xmm2\n\t"
		"pcmpgtb	%%xmm2, %%xmm1\n\t"
		"pandn	%%xmm0, %%xmm1\n\t"
		"movdqa	%%xmm1, %%xmm0\n\t"
		"pand	%%xmm7, %%xmm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%xmm0\n\t"
		"psubd	%%xmm0, %%xmm1\n\t"
		"movdqa	%%xmm1, %%xmm2\n\t"
		"pand	%%xmm5, %%xmm1\n\t" /* 0x33333333 */
		"psrld	$2, %%xmm2\n\t"
		"pand	%%xmm5, %%xmm2\n\t" /* 0x33333333 */
		"paddd	%%xmm1, %%xmm2\n\t"
		"pxor	%%xmm1, %%xmm1\n\t"
		"movdqa	%%xmm2, %%xmm0\n\t"
		"psrld	$4, %%xmm0\n\t"
		"paddd	%%xmm2, %%xmm0\n\t"
		"pand	%%xmm6, %%xmm0\n\t" /* 0x0f0f0f0f */
		"psadbw	%%xmm1, %%xmm0\n\t"
		"paddq %%xmm0, %%xmm3\n"
		"4:\n\t"
		"movdqa	%%xmm3, %%xmm0\n\t"
		"punpckhqdq	%%xmm0, %%xmm0\n\t"
		"paddq	%%xmm3, %%xmm0\n\t"
#ifdef __x86_64__
		"movq	%%xmm0, %0\n\t"
#else
		"movd	%%xmm0, %0\n\t"
#endif
	: /* %0 */ "="CL"r" (ret),
	  /* %1 */ "="CL"r" (cnt),
	  /* %2 */ "="CL"r" (s),
	  /* %3 */ "="CL"r" (t)
	: /* %4 */ "2" (s),
	  /* %5 */ "m" (**vals),
	  /* %6 */ CO  (len),
	  /* %7 */ "m" (*reg_num_mask)
#ifdef __SSE__
	: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
	);

	return ret;
}

#ifndef __x86_64__
static size_t mempopcnt_SSE(const void *s, size_t len)
{
	size_t ret, cnt, t;

	asm(
		"movq	   %5, %%mm7\n\t"
		"movq	16+%5, %%mm5\n\t"
		"movq	32+%5, %%mm6\n\t"
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
		"mov	%6, %1\n\t"
		"sub	%0, %1\n\t"
		"jbe 3f\n\t"
		"mov	%1, %0\n\t"
		"shr	$4, %1\n\t"
		"jz	1f\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"pand	%%mm7, %%mm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%mm0\n\t"
		"add	$8, %2\n\t"
		"psubd	%%mm0, %%mm1\n\t"
		"movq	%%mm1, %%mm0\n\t"
		"pand	%%mm5, %%mm1\n\t" /* 0x33333333 */
		"psrld	$2, %%mm0\n\t"
		"pand	%%mm5, %%mm0\n\t" /* 0x33333333 */
		"paddd	%%mm0, %%mm1\n\t"
		"pxor	%%mm2, %%mm2\n\t"
		"movq	%%mm1, %%mm0\n\t"
		"psrld	$4, %%mm0\n\t"
		"paddd	%%mm1, %%mm0\n\t"
		"pand	%%mm6, %%mm0\n\t" /* 0x0f0f0f0f */
		"psadbw	%%mm2, %%mm0\n\t"
		"paddd %%mm0, %%mm3\n"
		"2:\n\t"
		"mov	$15, %3\n\t"
		"cmp	%3, %1\n\t"
		"cmovb	%1, %3\n\t"
		"sub	%3, %1\n\t"
		"pxor	%%mm4, %%mm4\n\t"
		".p2align 2\n"
		"22:\n\t"
		"movq	(%2), %%mm1\n"
		"movq	8(%2), %%mm2\n"
		"movq	%%mm1, %%mm0\n\t"
		"pand	%%mm7, %%mm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%mm0\n\t"
		"add	$16, %2\n\t"
		"psubd	%%mm0, %%mm1\n\t"
		"movq	%%mm2, %%mm0\n\t"
		"pand	%%mm7, %%mm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%mm0\n\t"
		"psubd %%mm0, %%mm2\n\t"
		"movq	%%mm1, %%mm0\n\t"
		"pand	%%mm5, %%mm1\n\t" /* 0x33333333 */
		"psrld	$2, %%mm0\n\t"
		"pand	%%mm5, %%mm0\n\t" /* 0x33333333 */
		"paddd	%%mm0, %%mm1\n\t"
		"movq	%%mm2, %%mm0\n\t"
		"dec	%3\n\t"
		"pand	%%mm5, %%mm2\n\t" /* 0x33333333 */
		"psrld	$2, %%mm0\n\t"
		"pand	%%mm5, %%mm0\n\t" /* 0x33333333 */
		"paddd	%%mm0, %%mm2\n\t"
		"paddd	%%mm1, %%mm2\n\t"
		"movq	%%mm2, %%mm0\n\t"
		"pand	%%mm6, %%mm2\n\t" /* 0x0f0f0f0f */
		"psrld	$4, %%mm0\n\t"
		"pand	%%mm6, %%mm0\n\t" /* 0x0f0f0f0f */
		"paddb	%%mm0, %%mm2\n\t"
		"paddb	%%mm2, %%mm4\n\t"
		"jnz	22b\n\t"
		"pxor	%%mm0, %%mm0\n\t"
		"psadbw	%%mm0, %%mm4\n\t"
		"paddd %%mm4, %%mm3\n\t"
		"test	%1, %1\n\t"
		"jnz	2b\n\t"
		"jmp	5f\n"
		"1:\n\t"
		"and	$15, %0\n\t"
		"jz	4f\n\t"
		"jmp	8f\n"
		"9:\n\t"
		"add	$8, %0\n\t"
		"jmp	3f\n"
		"5:\n\t"
		"and	$15, %0\n\t"
		"jz	4f\n\t"
		"cmp	$8, %0\n\t"
		"jb	6f\n\t"
		"movq	(%2), %%mm0\n"
		"8:\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"pand	%%mm7, %%mm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%mm0\n\t"
		"add	$8, %2\n\t"
		"psubd	%%mm0, %%mm1\n\t"
		"movq	%%mm1, %%mm2\n\t"
		"pand	%%mm5, %%mm1\n\t" /* 0x33333333 */
		"psrld	$2, %%mm2\n\t"
		"pand	%%mm5, %%mm2\n\t" /* 0x33333333 */
		"paddd	%%mm1, %%mm2\n\t"
		"pxor	%%mm1, %%mm1\n\t"
		"movq	%%mm2, %%mm0\n\t"
		"psrld	$4, %%mm0\n\t"
		"paddd	%%mm2, %%mm0\n\t"
		"pand	%%mm6, %%mm0\n\t" /* 0x0f0f0f0f */
		"psadbw	%%mm1, %%mm0\n\t"
		"paddd %%mm0, %%mm3\n"
		"6:\n\t"
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
		"psubd	%%mm0, %%mm1\n\t"
		"movq	%%mm1, %%mm2\n\t"
		"pand	%%mm5, %%mm1\n\t" /* 0x33333333 */
		"psrld	$2, %%mm2\n\t"
		"pand	%%mm5, %%mm2\n\t" /* 0x33333333 */
		"paddd	%%mm1, %%mm2\n\t"
		"pxor	%%mm1, %%mm1\n\t"
		"movq	%%mm2, %%mm0\n\t"
		"psrld	$4, %%mm0\n\t"
		"paddd	%%mm2, %%mm0\n\t"
		"pand	%%mm6, %%mm0\n\t" /* 0x0f0f0f0f */
		"psadbw	%%mm1, %%mm0\n\t"
		"paddd %%mm0, %%mm3\n"
		"4:\n\t"
		"movq	%%mm3, %%mm0\n\t"
		"punpckhdq	%%mm0, %%mm0\n\t"
		"paddd	%%mm3, %%mm0\n\t"
		"movd	%%mm0, %0\n\t"
	: /* %0 */ "="CL"r" (ret),
	  /* %1 */ "="CL"r" (cnt),
	  /* %2 */ "="CL"r" (s),
	  /* %3 */ "="CL"r" (t)
	: /* %4 */ "2" (s),
	  /* %5 */ "m" (**vals),
	  /* %6 */ CO (len)
#ifdef __MMX__
	: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
#endif
	);

	return ret;
}

static size_t mempopcnt_MMX(const void *s, size_t len)
{
	size_t ret, cnt;

	asm(
		"movq	   %4, %%mm7\n\t"
		"movq	16+%4, %%mm5\n\t"
		"movq	32+%4, %%mm6\n\t"
		"movq	48+%4, %%mm4\n\t"
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
		"shr	$4, %1\n\t"
		"jz	1f\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"pand	%%mm7, %%mm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%mm0\n\t"
		"add	$8, %2\n\t"
		"psubd	%%mm0, %%mm1\n\t"
		"movq	%%mm1, %%mm0\n\t"
		"psrld	$2, %%mm1\n\t"
		"pand	%%mm5, %%mm0\n\t" /* 0x33333333 */
		"pand	%%mm5, %%mm1\n\t" /* 0x33333333 */
		"paddd	%%mm0, %%mm1\n\t"
		"movq	%%mm1, %%mm0\n\t"
		"psrld	$4, %%mm0\n\t"
		"paddd	%%mm1, %%mm0\n\t"
		"pand	%%mm6, %%mm0\n\t" /* 0x0f0f0f0f */
		"movq	%%mm0, %%mm1\n\t"
		"psrld $8, %%mm1\n\t"
		"paddd %%mm1, %%mm0\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"psrld $16, %%mm1\n\t"
		"paddd	%%mm1, %%mm0\n\t"
		"pand	%%mm4, %%mm0\n\t" /* 0x000000ff */
		"paddd %%mm0, %%mm3\n\t"
		"jmp	2f\n"
		"1:\n\t"
		"and	$15, %0\n\t"
		"jz	4f\n\t"
		"jmp	8f\n\t"
		".p2align 2\n"
		"2:\n\t"
		"movq	(%2), %%mm0\n"
		"movq	8(%2), %%mm2\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"pand	%%mm7, %%mm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%mm0\n\t"
		"add	$16, %2\n\t"
		"psubd	%%mm0, %%mm1\n\t"
		"movq	%%mm2, %%mm0\n\t"
		"pand	%%mm7, %%mm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%mm0\n\t"
		"psubd %%mm0, %%mm2\n\t"
		"movq	%%mm1, %%mm0\n\t"
		"psrld	$2, %%mm1\n\t"
		"pand	%%mm5, %%mm0\n\t" /* 0x33333333 */
		"pand	%%mm5, %%mm1\n\t" /* 0x33333333 */
		"paddd	%%mm0, %%mm1\n\t"
		"movq	%%mm2, %%mm0\n\t"
		"psrld	$2, %%mm2\n\t"
		"pand	%%mm5, %%mm0\n\t" /* 0x33333333 */
		"pand	%%mm5, %%mm2\n\t" /* 0x33333333 */
		"paddd	%%mm0, %%mm2\n\t"
		"paddd	%%mm1, %%mm2\n\t"
		"movq	%%mm2, %%mm0\n\t"
		"pand	%%mm6, %%mm2\n\t" /* 0x0f0f0f0f */
		"psrld	$4, %%mm0\n\t"
		"pand	%%mm6, %%mm0\n\t" /* 0x0f0f0f0f */
		"paddd	%%mm2, %%mm0\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"psrld $8, %%mm1\n\t"
		"dec	%1\n\t"
		"paddd	%%mm1, %%mm0\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"psrld $16, %%mm1\n\t"
		"paddd	%%mm1, %%mm0\n\t"
		"pand	%%mm4, %%mm0\n\t" /* 0x000000ff */
		"paddd %%mm0, %%mm3\n\t"
		"jnz	2b\n"
		"5:\n\t"
		"and	$15, %0\n\t"
		"jz	4f\n\t"
		"cmp	$8, %0\n\t"
		"jb	6f\n\t"
		"movq	(%2), %%mm0\n"
		"8:\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"pand	%%mm7, %%mm0\n\t" /* 0xaaaaaaaa */
		"psrld	$1, %%mm0\n\t"
		"add	$8, %2\n\t"
		"psubd	%%mm0, %%mm1\n\t"
		"movq	%%mm1, %%mm0\n\t"
		"psrld	$2, %%mm1\n\t"
		"pand	%%mm5, %%mm0\n\t" /* 0x33333333 */
		"pand	%%mm5, %%mm1\n\t" /* 0x33333333 */
		"paddd	%%mm0, %%mm1\n\t"
		"movq	%%mm1, %%mm0\n\t"
		"psrld	$4, %%mm0\n\t"
		"paddd	%%mm1, %%mm0\n\t"
		"pand	%%mm6, %%mm0\n\t" /* 0x0f0f0f0f */
		"movq	%%mm0, %%mm1\n\t"
		"psrld $8, %%mm1\n\t"
		"paddd %%mm1, %%mm0\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"psrld $16, %%mm1\n\t"
		"paddd	%%mm1, %%mm0\n\t"
		"pand	%%mm4, %%mm0\n\t" /* 0x000000ff */
		"paddd %%mm0, %%mm3\n"
		"6:\n\t"
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
		"psubd	%%mm0, %%mm1\n\t"
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
	: /* %0 */ "="CL"r" (ret),
	  /* %1 */ "="CL"r" (cnt),
	  /* %2 */ "="CL"r" (s)
	: /* %3 */ "2" (s),
	  /* %4 */ "m" (**vals),
	  /* %5 */ CO  (len)
#ifdef __MMX__
	: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
#endif
	);

	return ret;
}
#endif

#define ARCH_NAME_SUFFIX _generic
#define NEED_GEN_POPER
#include "../generic/mempopcnt.c"

static const struct test_cpu_feature t_feat[] =
{
	{.func = (void (*)(void))mempopcnt_SSE4, .flags_needed = CFEATURE_POPCNT},
	{.func = (void (*)(void))mempopcnt_SSE4, .flags_needed = CFEATURE_SSE4A},
	{.func = (void (*)(void))mempopcnt_SSSE3, .flags_needed = CFEATURE_SSSE3, .callback = test_cpu_feature_cmov_callback},
	{.func = (void (*)(void))mempopcnt_SSE2, .flags_needed = CFEATURE_SSE2, .callback = test_cpu_feature_cmov_callback},
#ifndef __x86_64__
	{.func = (void (*)(void))mempopcnt_SSE, .flags_needed = CFEATURE_SSE, .callback = test_cpu_feature_cmov_callback},
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
