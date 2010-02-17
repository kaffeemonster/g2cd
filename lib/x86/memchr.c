/*
 * memchr.c
 * memchr, x86 implementation
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
static void *memchr_SSE42(const void *s, int c, size_t n)
{
	unsigned char *ret;
	size_t t, z;
	const char *p;

	/*
	 * even if nehalem can handle unaligned load much better
	 * (so they promised), we still align hard to get in
	 * swing with the page boundery.
	 */
	asm (
		"mov	%4, %3\n\t"
		"prefetcht0	(%3)\n\t"
		"mov	%6, %1\n\t"
		"mov	%3, %2\n\t"
		"and	$0x0f, %2\n\t"
		"add	%2, %1\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"movd	%k5, %%xmm1\n\t"
		"pshufb	%%xmm0, %%xmm1\n\t"
		"and	$-16, %3\n\t"
		"mov	$16, %k0\n\t"
		/* ByteM,Norm,Any,Bytes */
		/*             6543210 */
		"pcmpestrm	$0b1000000, (%3), %%xmm1\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"shr	%b2, %0\n\t"
		"shl	%b2, %0\n\t"
		"bsf	%0, %2\n\t"
		"jnz	2f\n\t"
		"cmp	$16, %1\n\t"
		"jbe	3f\n\t"
		"mov	$16, %0\n\t"
		".p2align 2\n"
		"1:\n\t"
		"sub	%0, %1\n\t"
		"add	%0, %3\n\t"
		"prefetcht0	64(%3)\n\t"
		/* LSB,Norm,Any,Bytes */
		/*             6543210 */
		"pcmpestri	$0b0000000, (%3), %%xmm1\n\t"
		"ja	1b\n"
		"setnz	%b0\n\t"
		"jnc	3f\n\t"
		"2:\n\t"
		"lea	(%3,%2),%0\n"
		"3:"
		: /* %0 */ "=&a" (ret),
		  /* %1 */ "=&d" (z),
		  /* %2 */ "=&c" (t),
		  /* %3 */ "=&r" (p)
#  ifdef __i386__
		: /* %4 */ "m" (s),
		  /* %5 */ "m" (c),
		  /* %6 */ "m" (n)
#  else
		: /* %4 */ "r" (s),
		  /* %5 */ "r" (c),
		  /* %6 */ "r" (n)
#  endif
#  ifdef __SSE2__
		: "xmm0", "xmm1"
#  endif
	);
	return ret;
}
# endif

#if HAVE_BINUTILS >= 217
static void *memchr_SSSE3(const void *s, int c, size_t n)
{
	const unsigned char *p;
	void *ret;
	size_t f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"movd	%k5, %%xmm1\n\t"
		"sub	%3, %2\n\t"
		"sub	%4, %2\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"pshufb	%%xmm0, %%xmm1\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"ja	1f\n\t"
		"shr	%b3, %0\n\t"
		"shl	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"jnz	5f\n\t"
		"neg	%2\n\t"
		".p2align 1\n"
		"2:\n\t"
		"movdqa	16(%1), %%xmm0\n\t"
		"add	$16, %1\n\t"
		"prefetcht0	64(%1)\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"cmp	$16, %2\n\t"
		"jbe	3f\n\t"
		"sub	$16, %2\n\t"
		"test	%0, %0\n\t"
		"jz	2b\n"
		"3:\n\t"
		"mov	%2, %3\n\t"
		"sub	$16, %3\n\t"
		"jae	4f\n\t"
		"neg	%3\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n"
		"4:\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%0, %1\n\t"
		"5:\n\t"
		"add	%1, %0\n\t"
		".subsection 2\n"
		".p2align 2\n"
		"1:\n\t"
		"xchg	%2, %3\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n\t"
		"mov	%2, %3\n\t"
		"shr	%b3, %0\n\t"
		"shl	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%0, %1\n\t"
		"jmp	5b\n\t"
		".previous"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=&c" (f)
#ifdef __i386__
	: /* %4 */ "m" (n),
	  /* %5 */ "m" (c),
#else
	: /* %4 */ "r" (n),
	  /* %5 */ "r" (c),
#endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV16)),
	  /* %7 */ "2" (SOV16),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV16))
#ifdef __SSE__
	: "xmm0", "xmm1"
#endif
	);
	return ret;
}
# endif
#endif

static void *memchr_SSE2(const void *s, int c, size_t n)
{
	const unsigned char *p;
	void *ret;
	size_t f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"movzbl	%b5, %k0\n\t"
		"mov	%k0, %k2\n\t"
		"shl	$8, %k2\n\t"
		"or	%k2, %k0\n\t"
		"mov	$16, %2\n\t"
		"movd	%k0, %%xmm1\n\t"
		"sub	%3, %2\n\t"
		"sub	%4, %2\n\t"
		"pshuflw	$0b00000000, %%xmm1, %%xmm1\n\t"
		"pshufd	$0b00000000, %%xmm1, %%xmm1\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"ja	1f\n\t"
		"shr	%b3, %0\n\t"
		"shl	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"jnz	5f\n\t"
		"neg	%2\n\t"
		".p2align 1\n"
		"2:\n\t"
		"movdqa	16(%1), %%xmm0\n\t"
		"add	$16, %1\n\t"
		"prefetcht0	64(%1)\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"cmp	$16, %2\n\t"
		"jbe	3f\n\t"
		"sub	$16, %2\n\t"
		"test	%0, %0\n\t"
		"jz	2b\n"
		"3:\n\t"
		"mov	%2, %3\n\t"
		"sub	$16, %3\n\t"
		"jae	4f\n\t"
		"neg	%3\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n"
		"4:\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%0, %1\n\t"
		"5:\n\t"
		"add	%1, %0\n\t"
		".subsection 2\n"
		".p2align 2\n"
		"1:\n\t"
		"xchg	%2, %3\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n\t"
		"mov	%2, %3\n\t"
		"shr	%b3, %0\n\t"
		"shl	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%0, %1\n\t"
		"jmp	5b\n\t"
		".previous"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=&c" (f)
#ifdef __i386__
	: /* %4 */ "m" (n),
	  /* %5 */ "m" (c),
#else
	: /* %4 */ "r" (n),
	  /* %5 */ "r" (c),
#endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV16)),
	  /* %7 */ "1" (ALIGN_DOWN(s, SOV16))
#ifdef __SSE__
	: "xmm0", "xmm1"
#endif
	);
	return ret;
}

#ifndef __x86_64__
static void *memchr_SSE(const void *s, int c, size_t n)
{
	const unsigned char *p;
	void *ret;
	size_t f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"sub	%3, %2\n\t"
		"sub	%4, %2\n\t"
		"movb	%8, %b0\n\t"
		"movb	%b0, %h0\n\t"
		"pinsrw	$0b00000000, %k0, %%mm1\n\t"
		"pshufw	$0b00000000, %%mm1, %%mm1\n\t"
		"movq	(%1), %%mm0\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pmovmskb	%%mm0, %0\n\t"
		"ja	1f\n\t"
		"shr	%b3, %0\n\t"
		"shl	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"jnz	5f\n\t"
		"neg	%2\n\t"
		".p2align 1\n"
		"2:\n\t"
		"movq	8(%1), %%mm0\n\t"
		"add	$8, %1\n\t"
		"prefetcht0	64(%1)\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pmovmskb	%%mm0, %0\n\t"
		"cmp	$8, %2\n\t"
		"jbe	3f\n\t"
		"sub	$8, %2\n\t"
		"test	%0, %0\n\t"
		"jz	2b\n"
		"3:\n\t"
		"mov	%2, %3\n\t"
		"sub	$8, %3\n\t"
		"jae	4f\n\t"
		"neg	%3\n\t"
		"shl	%b3, %b0\n\t"
		"shr	%b3, %b0\n"
		"4:\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%0, %1\n\t"
		"5:\n\t"
		"add	%1, %0\n\t"
		".subsection 2\n\t"
		".p2align 2\n"
		"1:\n\t"
		"xchg	%2, %3\n\t"
		"shl	%b3, %b0\n\t"
		"shr	%b3, %b0\n\t"
		"mov	%2, %3\n\t"
		"shr	%b3, %0\n\t"
		"shl	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%0, %1\n\t"
		"jmp	5b\n\t"
		".previous"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=c" (f)
	: /* %4 */ "m" (n),
	  /* %5 */ "3" (ALIGN_DOWN_DIFF(s, SOV8)),
	  /* %6 */ "2" (SOV8),
	  /* %7 */ "1" (ALIGN_DOWN(s, SOV8)),
	  /* %8 */ "m" (c)
#ifdef __SSE__
	: "mm0", "mm1"
#endif
	);
	return ret;
}
#endif

static void *memchr_x86(const void *s, int c, size_t n)
{
	void *ret;
	asm (
#ifndef __x86_64__
		"xchg	%1, %%edi\n\t"
#endif
		"repne	scasb\n\t"
#ifndef __x86_64__
		"lea	-1(%%edi), %0\n\t"
		"mov	%1, %%edi\n\t"
#else
		"lea	-1(%1), %0\n\t"
#endif
		"je	1f\n\f" /* a cmov would need processor support */
		"mov	%2, %0\n"
		"1:"
	: /* %0 */ "=a" (ret),
#ifndef __x86_64__
	  /* %1 */ "=r" (s),
#else
	  /* %1 */ "=D" (s),
#endif
	  /* %2 */ "=c" (n)
	: /* %3 */ "0" (c),
	  /* %4 */ "1" (s),
	  /* %5 */ "2" (n)
	);
	return ret;
}


static const struct test_cpu_feature t_feat[] =
{
#if 0
// TODO: advanced code is buggy ATM
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 218
	{.func = (void (*)(void))memchr_SSE42, .flags_needed = CFEATURE_SSE4_2, .callback = test_cpu_feature_cmov_callback},
# endif
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))memchr_SSSE3, .flags_needed = CFEATURE_SSSE3, .callback = test_cpu_feature_cmov_callback},
# endif
#endif
	{.func = (void (*)(void))memchr_SSE2, .flags_needed = CFEATURE_SSE2, .callback = test_cpu_feature_cmov_callback},
#ifndef __x86_64__
	{.func = (void (*)(void))memchr_SSE, .flags_needed = CFEATURE_SSE, .callback = test_cpu_feature_cmov_callback},
#endif
#endif
	{.func = (void (*)(void))memchr_x86, .flags_needed = -1, .callback = NULL},
};

static void *memchr_runtime_sw(const void *s, int c, size_t n);
/*
 * Func ptr
 */
static void *(*memchr_ptr)(const void *s, int c, size_t n) = memchr_runtime_sw;

/*
 * constructor
 */
static GCC_ATTR_CONSTRUCT void memchr_select(void)
{
	memchr_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer failes
 */
static void *memchr_runtime_sw(const void *s, int c, size_t n)
{
	memchr_select();
	return memchr_ptr(s, c, n);
}

void *my_memchr(const void *s, int c, size_t n)
{
	return memchr_ptr(s, c, n);
}

static char const rcsid_mcx[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
