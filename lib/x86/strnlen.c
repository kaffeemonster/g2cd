/*
 * strnlen.c
 * strnlen for non-GNU platforms, x86 implementation
 *
 * Copyright (c) 2008-2009 Jan Seiffert
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
 *
 * I would have a pure MMX version, but it is slower than the
 * generic version (58000ms vs. 54000ms), MMX was a really
 * braindead idea without a MMX->CPU-Reg move instruction
 * to do things not possible with MMX.
 */
/*
 * Sometimes you need a new perspective, like the altivec
 * way of handling things.
 * Lower address bits? Totaly overestimated.
 *
 * We don't precheck for alignment, 16 or 8 is very unlikely
 * instead we "align hard", do one load "under the address",
 * mask the excess info out and afterwards we are fine to go.
 *
 * Even this beeing strNlen, this n can be seen as a "hint"
 * We can overread and underread, but should cut the result
 * (and not pass a page boundery, but we cannot because
 * we are aligned).
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

static size_t strnlen_SSE42(const char *s, size_t maxlen)
{
	const char *p;
	size_t len, f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"sub	%3, %2\n\t"
		"sub	%4, %2\n\t"
		"pxor	%%xmm1, %%xmm1\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"ja	1f\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"jnz	5f\n\t"
		"neg	%2\n\t"
		"mov	$0xFF01, %k0\n\t"
		"movd	%k0, %%xmm1\n\t"
		"xor	%0, %0\n\t"
		"add	$2, %0\n\t"
		"xor	%3, %3\n\t"
		"add	$16, %3\n\t"
		"add	%3, %2\n\t"
		".p2align 1\n"
		"2:\n\t"
		"sub	%3, %2\n\t"
		"add	%3, %1\n\t"
		"prefetcht0	64(%1)\n\t"
		/* LSB,Invert,Range,Bytes */
		/*             6543210 */
		"pcmpestri	$0b0010100, (%1), %%xmm1\n\t"
		"ja	2b\n\t"
		"cmovnc	%2, %3\n\t"
		"lea	(%3, %1), %0\n\t"
		"sub	%5, %0\n"
		"5:\n\t"
		".subsection 2\n"
		".p2align 2\n"
		"1:\n\t"
		"xchg	%2, %3\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n\t"
		"mov	%2, %3\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%4, %0\n\t"
		"jmp	5b\n"
		".previous"
	: /* %0 */ "=&a" (len),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&d" (t),
	  /* %3 */ "=&c" (f)
#ifdef __i386__
	: /* %4 */ "m" (maxlen),
	  /* %5 */ "m" (s),
#else
	: /* %4 */ "r" (maxlen),
	  /* %5 */ "r" (s),
#endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV16)),
	  /* %7 */ "2" (SOV16),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV16))
#ifdef __SSE__
	: "xmm0", "xmm1"
#endif
	);
	return len;
}

static size_t strnlen_SSE2(const char *s, size_t maxlen)
{
	const char *p;
	size_t len, f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"sub	%3, %2\n\t"
		"sub	%4, %2\n\t"
		"pxor	%%xmm1, %%xmm1\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"ja	1f\n\t"
		"shr	%b3, %0\n\t"
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
		"cmovz	%2, %0\n\t"
		"add	%1, %0\n\t"
		"sub	%5, %0\n"
		"5:\n\t"
		".subsection 2\n"
		".p2align 2\n"
		"1:\n\t"
		"xchg	%2, %3\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n\t"
		"mov	%2, %3\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%4, %0\n\t"
		"jmp	5b\n\t"
		".previous"
	: /* %0 */ "=&a" (len),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=&c" (f)
#ifdef __i386__
	: /* %4 */ "m" (maxlen),
	  /* %5 */ "m" (s),
#else
	: /* %4 */ "r" (maxlen),
	  /* %5 */ "r" (s),
#endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV16)),
	  /* %7 */ "2" (SOV16),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV16))
#ifdef __SSE__
	: "xmm0", "xmm1"
#endif
	);
	return len;
}

#ifndef __x86_64__
static size_t strnlen_SSE(const char *s, size_t maxlen)
{
	const char *p;
	size_t len, f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"sub	%3, %2\n\t"
		"sub	%4, %2\n\t"
		"pxor	%%mm1, %%mm1\n\t"
		"movq	(%1), %%mm0\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pmovmskb	%%mm0, %0\n\t"
		"ja	1f\n\t"
		"shr	%b3, %0\n\t"
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
		"cmovz	%2, %0\n\t"
		"add	%1, %0\n\t"
		"sub	%5, %0\n"
		"5:\n\t"
		".subsection 2\n\t"
		".p2align 2\n"
		"1:\n\t"
		"xchg	%2, %3\n\t"
		"shl	%b3, %b0\n\t"
		"shr	%b3, %b0\n\t"
		"mov	%2, %3\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%4, %0\n\t"
		"jmp	5b\n"
		".previous"
	: /* %0 */ "=&a" (len),
	  /* %1 */ "=r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=c" (f)
	: /* %4 */ "m" (maxlen),
	  /* %5 */ "m" (s),
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV8)),
	  /* %7 */ "2" (SOV8),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV8))
#ifdef __SSE__
	: "mm0", "mm1"
#endif
	);
	return len;
}
#endif

static size_t strnlen_x86(const char *s, size_t maxlen)
{
	const char *p;
	size_t r;
	ssize_t f, k;
	prefetch(s);

	f = ALIGN_DOWN_DIFF(s, SOST);
	k = SOST - f - (ssize_t)maxlen;
	k = k > 0 ? k  : 0;

	p = (const char *)ALIGN_DOWN(s, SOST);
	r = has_nul_byte(*(const size_t *)p);
	r <<= k * BITS_PER_CHAR;
	r >>= k * BITS_PER_CHAR;
	r >>= f * BITS_PER_CHAR;
	if(r)
		return nul_byte_index(r);
	else if(k)
		return maxlen;

	maxlen -= SOST - f;
	do
	{
		p += SOST;
		r = has_nul_byte(*(const size_t *)p);
		if(maxlen <= SOST)
			break;
		maxlen -= SOST;
	} while(!r);
	k = SOST - (ssize_t)maxlen;
	k = k > 0 ? k : 0;
	r <<= k * BITS_PER_CHAR;
	r >>= k * BITS_PER_CHAR;
	if(likely(r))
		r = nul_byte_index(r);
	else
		r = maxlen;
	return p - s + r;
}

static const struct test_cpu_feature t_feat[] =
{
	{.func = (void (*)(void))strnlen_SSE42, .flags_needed = CFEATURE_SSE4_2, .callback = NULL},
	{.func = (void (*)(void))strnlen_SSE2, .flags_needed = CFEATURE_SSE2, .callback = NULL},
#ifndef __x86_64__
	{.func = (void (*)(void))strnlen_SSE, .flags_needed = CFEATURE_SSE, .callback = NULL},
#endif
	{.func = (void (*)(void))strnlen_x86, .flags_needed = -1, .callback = NULL},
};

static size_t strnlen_runtime_sw(const char *s, size_t maxlen);
/*
 * Func ptr
 */
static size_t (*strnlen_ptr)(const char *s, size_t maxlen) = strnlen_runtime_sw;

/*
 * constructor
 */
static GCC_ATTR_CONSTRUCT void strnlen_select(void)
{
	strnlen_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer fails
 */
static size_t strnlen_runtime_sw(const char *s, size_t maxlen)
{
	strnlen_select();
	return strnlen_ptr(s, maxlen);
}

size_t strnlen(const char *s, size_t maxlen)
{
	return strnlen_ptr(s, maxlen);
}

static char const rcsid_snl[] GCC_ATTR_USED_VAR = "$Id: $";
/* eof */
