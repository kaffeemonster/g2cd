/*
 * tstrchrnul.c
 * tstrchrnul, x86 implementation
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

/*
 * Intel has a good feeling how to create incomplete and
 * unsymetric instruction sets, so sometimes there is a
 * last "missing link".
 * Hammering on multiple integers was to be implemented with
 * MMX, but SSE finaly brought usefull comparison, but not
 * for 128 Bit SSE datatypes, only the old 64 Bit MMX...
 * SSE2 finaly brought fun for 128Bit.
 */
/*
 * Sometimes you need a new perspective, like the altivec
 * way of handling things.
 * Lower address bits? Totaly overestimated.
 *
 * We don't precheck for alignment, 16 or 8 is very unlikely
 * instead we "align hard", do one load "under the address",
 * mask the excess info out and afterwards we are fine to go.
 */
/*
 * Normaly we could generate this with the gcc vector
 * __builtins, but since gcc whines when the -march does
 * not support the operation and we want to always provide
 * them and switch between them...
 * And near by, sorry gcc, your bsf handling sucks.
 * bsf generates flags, no need to test beforehand,
 * but AFTERWARDS!!!
 */

#include "x86_features.h"

#define SOV16	16
#define SOV8	8

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 218
static tchar_t *tstrchrnul_SSE42(const tchar_t *s, tchar_t c)
{
	tchar_t *ret;
	size_t t, z;
	const tchar_t *p;

	/*
	 * even if nehalem can handle unaligned load much better
	 * (so they promised), we still align hard to get in
	 * swing with the page boundery.
	 */
	asm (
		"mov	%4, %3\n\t"
		"prefetcht0	(%3)\n\t"
#ifdef __i386__
		"movd	%k2, %%xmm1\n\t"
#else
		"movd	%k5, %%xmm1\n\t"
#endif
		"and	$-16, %3\n\t"
		"mov	$8, %k0\n\t"
		"mov	%0, %1\n\t"
		/* ByteM,Norm,Any,Words */
		/*             6543210 */
		"pcmpestrm	$0b1000001, (%3), %%xmm1\n\t"
		"mov	%4, %2\n\t"
		"and	$0x0f, %2\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"shr	%b2, %0\n\t"
		"shl	%b2, %0\n\t"
		"bsf	%0, %2\n\t"
		"jnz	2f\n\t"
		"mov	%1, %0\n\t"
		".p2align 1\n"
		"1:\n\t"
		"add	%1, %3\n\t"
		"prefetcht0	64(%3)\n\t"
		/* LSB,Norm,Any,Words */
		/*             6543210 */
		"pcmpestri	$0b0000001, (%3), %%xmm1\n\t"
		"ja	1b\n\t"
		"2:"
		"lea	(%3,%2),%0\n\t"
		: /* %0 */ "=&a" (ret),
		  /* %1 */ "=&d" (z),
		  /* %2 */ "=&c" (t),
		  /* %3 */ "=&r" (p)
#  ifdef __i386__
		: /* %4 */ "m" (s),
		  /* %5 */ "2" (c)
#  else
		: /* %4 */ "r" (s),
		  /* %5 */ "r" (c)
#  endif
#  ifdef __SSE2__
		: "xmm0", "xmm1"
#  endif
	);
	return ret;
}
# endif
#endif

static tchar_t *tstrchrnul_SSE2(const tchar_t *s, tchar_t c)
{
	tchar_t *ret;
	const tchar_t *p;
	size_t t;

	asm (
		/*
		 * hate doing this, but otherwise the compiler thinks
		 * he absolutly needs to put everything in the wrong
		 * reg, so he better shuffeles it all around, and for
		 * this he needs another reg, ohh, lets spill one...
		 */
		"mov	%3, %1\n\t"
		"prefetcht0	(%1)\n\t"
#ifdef __i386__
		"movd	%k2, %%xmm2\n\t"
#else
		"movd	%k5, %%xmm2\n\t"
#endif
		"mov	%1, %2\n\t"
		"and	$-16, %1\n\t"
		"and	$0x0f, %2\n\t"
		"pxor	%%xmm1, %%xmm1\n\t"
		"pshuflw	$0b00000000, %%xmm2, %%xmm2\n\t"
		"pshufd	$0b00000000, %%xmm2, %%xmm2\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"movdqa	%%xmm0, %%xmm3\n\t"
		"pcmpeqw	%%xmm1, %%xmm0\n\t"
		"pcmpeqw	%%xmm2, %%xmm3\n\t"
		"por	%%xmm3, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"shr	%b2, %0\n\t"
		"shl	%b2, %0\n\t"
		"bsf	%0, %0\n\t"
		"jnz	2f\n\t"
		".p2align 1\n"
		"1:\n\t"
		"movdqa	16(%1), %%xmm0\n\t"
		"movdqa	%%xmm0, %%xmm3\n\t"
		"add	$16, %1\n\t"
		"pcmpeqw	%%xmm1, %%xmm0\n\t"
		"pcmpeqw	%%xmm2, %%xmm3\n\t"
		"por	%%xmm3, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"bsf	%0, %0\n\t"
		"jz	1b\n\t"
		"2:"
		"add	%1, %0\n\t"
		: /* %0 */ "=&r" (ret),
		  /* %1 */ "=&r" (p),
		  /* %2 */ "=&c" (t)
#ifdef __i386__
		: /* %4 */ "m" (s),
		  /* %5 */ "2" (c)
#else
		: /* %4 */ "r" (s),
		  /* %5 */ "r" (c)
#endif
#ifdef __SSE2__
		: "xmm0", "xmm1", "xmm2", "xmm3"
#endif
	);
	return ret;
}

#ifndef __x86_64__
static tchar_t *tstrchrnul_SSE(const tchar_t *s, tchar_t c)
{
	tchar_t *ret;
	const tchar_t *p;
	size_t t;

	asm (
		"mov	%3, %1\n\t"
		"prefetcht0 (%1)\n\t"
		"pinsrw	$0b00000000, %k2, %%mm2\n\t"
		"mov	%1, %2\n\t"
		"and	$-8, %1\n\t"
		"and	$0x7, %2\n\t"
		"pxor	%%mm1, %%mm1\n\t"
		"pshufw	$0b00000000, %%mm2, %%mm2\n\t"
		"movq	(%1), %%mm0\n\t"
		"movq	%%mm0, %%mm3\n\t"
		"pcmpeqw	%%mm1, %%mm0\n\t"
		"pcmpeqw	%%mm2, %%mm3\n\t"
		"por	%%mm3, %%mm0\n\t"
		"pmovmskb	%%mm0, %0\n\t"
		"shr	%b2, %0\n\t"
		"shl	%b2, %0\n\t"
		"bsf	%0, %0\n\t"
		"jnz	2f\n\t"
		".p2align 1\n"
		"1:\n\t"
		"movq	8(%1), %%mm0\n\t"
		"movq	%%mm0, %%mm3\n\t"
		"add	$8, %1\n\t"
		"pcmpeqw	%%mm1, %%mm0\n\t"
		"pcmpeqw	%%mm2, %%mm3\n\t"
		"por	%%mm3, %%mm0\n\t"
		"pmovmskb	%%mm0, %0\n\t"
		"bsf	%0, %0\n\t"
		"jz	1b\n\t"
		"2:\n\t"
		"add	%1, %0\n\t"
		: /* %0 */ "=&r" (ret),
		  /* %1 */ "=r" (p),
		  /* %2 */ "=c" (t)
		: /* %3 */ "m" (s),
		  /* %4 */ "2" (c)
#ifdef __SSE__
		: "mm0", "mm1", "mm2", "mm3"
#endif
	);
	return ret;
}
#endif

static tchar_t *tstrchrnul_x86(const tchar_t *s, tchar_t c)
{
	const tchar_t *p;
	tchar_t *ret;
	size_t t, t2, mask;

	/* only do this in scalar code the SIMD versions are fast,
	 * better avoid the overhead there */
	if(unlikely(!c))
		return ((tchar_t *)(intptr_t)s) + tstrlen(s);
	asm (
#ifndef __x86_64__
		"imul	$0x10001, %4, %4\n\t"
		"push	%2\n\t"
#else
		"imul	%10, %4\n\t"
		"neg	%10\n\t"
#endif
		"mov	(%1), %2\n\t"
		"mov	%2, %3\n\t"
#ifndef __x86_64__
		"lea	-0x10001(%2),%0\n\t"
#else
		"mov	%2, %0\n\t"
		"add	%10, %0\n\t"
#endif
		"xor	%4, %3\n\t"
		"not	%2\n\t"
		"and	%2, %0\n\t"
#ifndef __x86_64__
		"lea	-0x10001(%3),%2\n\t"
#else
		"mov	%3, %2\n\t"
		"add	%10, %2\n\t"
#endif
		"not	%3\n\t"
		"and	%3, %2\n\t"
		"or	%2, %0\n\t"
#ifndef __x86_64__
		"pop	%2\n\t"
#endif
		"and	%9, %0\n\t"
		"shr	%b8, %0\n\t"
		"shl	%b8, %0\n\t"
		"bsf	%0, %0\n\t"
		"jnz	2f\n\t"
		".p2align 1\n"
		"1:\n\t"
		"add	%6, %1\n\t"
		"mov	(%1), %2\n\t"
		"mov	%2, %3\n\t"
#ifndef __x86_64__
		"lea	-0x10001(%2),%0\n\t"
#else
		"mov	%2, %0\n\t"
		"add	%10, %0\n\t"
#endif
		"xor	%4, %3\n\t"
		"not	%2\n\t"
		"and	%2, %0\n\t"
#ifndef __x86_64__
		"lea	-0x10001(%3),%2\n\t"
#else
		"mov	%3, %2\n\t"
		"add	%10, %2\n\t"
#endif
		"not	%3\n\t"
		"and	%3, %2\n\t"
		"or	%2, %0\n\t"
		"and	%9, %0\n\t"
		"bsf	%0, %0\n\t"
		"jz	1b\n"
		"2:\n\t"
		"shr	$3, %0\n\t"
		"add	%1, %0\n\t"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=&r" (p),
#ifndef __x86_64__
	  /* %2 */ "=c" (t),
#else
	  /* %2 */ "=&r" (t),
#endif
	  /* %3 */ "=&r" (t2),
	  /* %4 */ "=r" (mask)
	: /* %5 */ "1" (ALIGN_DOWN(s, SOST)),
	  /* %6 */ "K" (SOST),
	  /* %7 */ "4" (c),
#ifndef __x86_64__
	  /* %8 */ "2" (ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR),
	  /* %9 */ "e" (0x80008000)
#else
	  /*
	   * amd64 has more register and a regcall abi
	   * so keep in reg.
	   */
	  /* %8 */ "c" (ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR),
	  /* %9 */ "r"  (0x8000800080008000LL),
	  /* %10 */ "r" (0x0001000100010001LL)
#endif
	);
	return ret;
}


static const struct test_cpu_feature t_feat[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 218
	{.func = (void (*)(void))tstrchrnul_SSE42, .flags_needed = CFEATURE_SSE4_2, .callback = NULL},
# endif
#endif
	{.func = (void (*)(void))tstrchrnul_SSE2, .flags_needed = CFEATURE_SSE2, .callback = NULL},
#ifndef __x86_64__
	{.func = (void (*)(void))tstrchrnul_SSE, .flags_needed = CFEATURE_SSE, .callback = NULL},
#endif
	{.func = (void (*)(void))tstrchrnul_x86, .flags_needed = -1, .callback = NULL},
};

static tchar_t *tstrchrnul_runtime_sw(const tchar_t *s, tchar_t c);
/*
 * Func ptr
 */
static tchar_t *(*tstrchrnul_ptr)(const tchar_t *s, tchar_t c) = tstrchrnul_runtime_sw;

/*
 * constructor
 */
static GCC_ATTR_CONSTRUCT void tstrchrnul_select(void)
{
	tstrchrnul_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer failes
 */
static tchar_t *tstrchrnul_runtime_sw(const tchar_t *s, tchar_t c)
{
	tstrchrnul_select();
	return tstrchrnul_ptr(s, c);
}

tchar_t *tstrchrnul(const tchar_t *s, tchar_t c)
{
	return tstrchrnul_ptr(s, c);
}

static char const rcsid_tscn[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
