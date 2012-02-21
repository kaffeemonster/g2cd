/*
 * strnlen.c
 * strnlen for non-GNU platforms, x86 implementation
 *
 * Copyright (c) 2008-2011 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd.
 * If not, see <http://www.gnu.org/licenses/>.
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
 * But bsf can be slow, and thanks to the _new_ Atom, which
 * again needs 17 clock cycles, the P4 also isn't very
 * glorious...
 * On the other hand, the bsf HAS to happen at some point.
 * Since most strings are short, the first is a hit, and
 * we can save all the other handling, jumping, etc.
 * I think i measured that at one point...
 * Hmmm, but not on the offenders...
 */

#include "x86_features.h"

#define SOV32	32
#define SOV16	16
#define SOV8	8

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
static size_t strnlen_AVX2(const char *s, size_t maxlen)
{
	const char *p;
	size_t len, f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"sub	%3, %2\n\t"
		"sub	%4, %2\n\t"
		"vpxor	%%ymm1, %%ymm1, %%ymm1\n\t"
		"vpcmpeqb	(%1), %%ymm1, %%ymm0\n\t"
		"vpmovmskb	%%ymm0, %0\n\t"
		"ja	1f\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"jnz	5f\n\t"
		"mov	$32, %b0\n\t"
		"neg	%2\n\t"
		"jz	6f\n\t"
		".p2align 1\n"
		"2:\n\t"
		"vpcmpeqb	32(%1), %%ymm1, %%ymm0\n\t"
		"add	$32, %1\n\t"
		"prefetcht0	64(%1)\n\t"
		"cmp	$32, %2\n\t"
		"jbe	3f\n\t"
		"sub	$32, %2\n\t"
		"vptest	%%ymm0, %%ymm1\n\t"
		"jc	2b\n"
		"3:\n\t"
		"vpmovmskb	%%xmm0, %0\n\t"
		"mov	%2, %3\n\t"
		"sub	$32, %3\n\t"
		"jae	4f\n\t"
		"neg	%3\n\t"
		"shl	%b3, %0\n\t"
		"shr	%b3, %0\n"
		"4:\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%2, %0\n"
		"6:\n\t"
		"add	%1, %0\n\t"
		"sub	%5, %0\n\t"
#ifdef HAVE_SUBSECTION
		".subsection 2\n\t"
#else
		"jmp	5f\n\t"
#endif
		".p2align 2\n"
		"1:\n\t"
		"xchg	%2, %3\n\t"
		"shl	%b3, %0\n\t"
		"shr	%b3, %0\n\t"
		"mov	%2, %3\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%4, %0\n\t"
#ifdef HAVE_SUBSECTION
		"jmp	5f\n\t"
		".previous\n"
#endif
		"5:"
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
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV32)),
	  /* %7 */ "2" (SOV32),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV32))
#ifdef __SSE__
	: "xmm0", "xmm1"
#endif
	);
	return len;
}
# endif

# if HAVE_BINUTILS >= 219
/*
 * This code does not use any AVX feature, it only uses the new
 * v* opcodes, so the upper half of the register gets 0-ed,
 * and the CPU is not caught with lower/upper half merges
 */
static size_t strnlen_AVX(const char *s, size_t maxlen)
{
	const char *p;
	size_t len, f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"sub	%3, %2\n\t"
		"sub	%4, %2\n\t"
		"vpxor	%%xmm1, %%xmm1, %%xmm1\n\t"
		"vpcmpeqb	(%1), %%xmm1, %%xmm0\n\t"
		"vpmovmskb	%%xmm0, %0\n\t"
		"ja	1f\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"jnz	5f\n\t"
		"mov	$16, %b3\n\t"
		"neg	%2\n\t"
		"jz	6f\n\t"
		"mov	$0xFF01, %k0\n\t"
		"vmovd	%k0, %%xmm1\n\t"
		"mov	$2, %k0\n\t"
		"add	%3, %2\n\t"
		".p2align 1\n"
		"2:\n\t"
		"sub	$16, %2\n\t"
		"add	$16, %1\n\t"
		"prefetcht0	64(%1)\n\t"
		/* LSB,Invert,Range,Bytes */
		/*             6543210 */
		"vpcmpestri	$0b0010100, (%1), %%xmm1\n\t"
		"ja	2b\n\t"
		"cmovnc	%2, %3\n"
		"6:\n\t"
		"lea	(%3, %1), %0\n\t"
		"sub	%5, %0\n\t"
#ifdef HAVE_SUBSECTION
		".subsection 2\n\t"
#else
		"jmp	5f\n\t"
#endif
		".p2align 2\n"
		"1:\n\t"
		"xchg	%2, %3\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n\t"
		"mov	%2, %3\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%4, %0\n\t"
#ifdef HAVE_SUBSECTION
		"jmp	5f\n\t"
		".previous\n"
#endif
		"5:"
	: /* %0 */ "=&a" (len),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&d" (t),
	  /* %3 */ "=&c" (f)
#  ifdef __i386__
	: /* %4 */ "m" (maxlen),
	  /* %5 */ "m" (s),
#  else
	: /* %4 */ "r" (maxlen),
	  /* %5 */ "r" (s),
#  endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV16)),
	  /* %7 */ "2" (SOV16),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV16))
#  ifdef __SSE__
	: "xmm0", "xmm1"
#  endif
	);
	return len;
}
# endif

# if HAVE_BINUTILS >= 218
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
		"mov	$16, %b3\n\t"
		"neg	%2\n\t"
		"jz	6f\n\t"
		"mov	$0xFF01, %k0\n\t"
		"movd	%k0, %%xmm1\n\t"
		"mov	$2, %k0\n\t"
		"add	%3, %2\n\t"
		".p2align 1\n"
		"2:\n\t"
		"sub	$16, %2\n\t"
		"add	$16, %1\n\t"
		"prefetcht0	64(%1)\n\t"
		/* LSB,Invert,Range,Bytes */
		/*             6543210 */
		"pcmpestri	$0b0010100, (%1), %%xmm1\n\t"
		"ja	2b\n\t"
		"cmovnc	%2, %3\n"
		"6:\n\t"
		"lea	(%3, %1), %0\n\t"
		"sub	%5, %0\n\t"
#ifdef HAVE_SUBSECTION
		".subsection 2\n\t"
#else
		"jmp	5f\n\t"
#endif
		".p2align 2\n"
		"1:\n\t"
		"xchg	%2, %3\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n\t"
		"mov	%2, %3\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%4, %0\n\t"
#ifdef HAVE_SUBSECTION
		"jmp	5f\n\t"
		".previous\n"
#endif
		"5:"
	: /* %0 */ "=&a" (len),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&d" (t),
	  /* %3 */ "=&c" (f)
#  ifdef __i386__
	: /* %4 */ "m" (maxlen),
	  /* %5 */ "m" (s),
#  else
	: /* %4 */ "r" (maxlen),
	  /* %5 */ "r" (s),
#  endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV16)),
	  /* %7 */ "2" (SOV16),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV16))
#  ifdef __SSE__
	: "xmm0", "xmm1"
#  endif
	);
	return len;
}

static size_t strnlen_SSE41(const char *s, size_t maxlen)
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
		"mov	$16, %b0\n\t"
		"neg	%2\n\t"
		"jz	6f\n\t"
		".p2align 1\n"
		"2:\n\t"
		"movdqa	16(%1), %%xmm0\n\t"
		"add	$16, %1\n\t"
		"prefetcht0	64(%1)\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"cmp	$16, %2\n\t"
		"jbe	3f\n\t"
		"sub	$16, %2\n\t"
		"ptest	%%xmm0, %%xmm1\n\t"
		"jc	2b\n"
		"3:\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"mov	%2, %3\n\t"
		"sub	$16, %3\n\t"
		"jae	4f\n\t"
		"neg	%3\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n"
		"4:\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%2, %0\n"
		"6:\n\t"
		"add	%1, %0\n\t"
		"sub	%5, %0\n\t"
#ifdef HAVE_SUBSECTION
		".subsection 2\n\t"
#else
		"jmp	5f\n\t"
#endif
		".p2align 2\n"
		"1:\n\t"
		"xchg	%2, %3\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n\t"
		"mov	%2, %3\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%4, %0\n\t"
#ifdef HAVE_SUBSECTION
		"jmp	5f\n\t"
		".previous\n"
#endif
		"5:"
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
# endif
#endif

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
		"mov	$16, %b0\n\t"
		"neg	%2\n\t"
		"jz	6f\n\t"
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
		"cmovz	%2, %0\n"
		"6:\n\t"
		"add	%1, %0\n\t"
		"sub	%5, %0\n\t"
#ifdef HAVE_SUBSECTION
		".subsection 2\n\t"
#else
		"jmp	5f\n\t"
#endif
		".p2align 2\n"
		"1:\n\t"
		"xchg	%2, %3\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n\t"
		"mov	%2, %3\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%4, %0\n\t"
#ifdef HAVE_SUBSECTION
		"jmp	5f\n\t"
		".previous\n"
#endif
		"5:"
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
		"mov	$8, %b0\n\t"
		"neg	%2\n\t"
		"jz	6f\n\t"
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
		"cmovz	%2, %0\n"
		"6:\n\t"
		"add	%1, %0\n\t"
		"sub	%5, %0\n\t"
#ifdef HAVE_SUBSECTION
		".subsection 2\n\t"
#else
		"jmp	5f\n\t"
#endif
		".p2align 2\n"
		"1:\n\t"
		"xchg	%2, %3\n\t"
		"shl	%b3, %b0\n\t"
		"shr	%b3, %b0\n\t"
		"mov	%2, %3\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"cmovz	%4, %0\n\t"
#ifdef HAVE_SUBSECTION
		"jmp	5f\n\t"
		".previous\n"
#endif
		"5:"
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

static __init_cdata const struct test_cpu_feature tfeat_strnlen[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
	{.func = (void (*)(void))strnlen_AVX2 , .features = {[4] = CFB(CFEATURE_AVX2)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 219
	{.func = (void (*)(void))strnlen_AVX ,  .features = {[1] = CFB(CFEATURE_AVX)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 218
	{.func = (void (*)(void))strnlen_SSE42, .features = {[1] = CFB(CFEATURE_SSE4_2),  [0] = CFB(CFEATURE_CMOV)}},
	{.func = (void (*)(void))strnlen_SSE41, .features = {[1] = CFB(CFEATURE_SSE4_1),  [0] = CFB(CFEATURE_CMOV)}},
# endif
#endif
	{.func = (void (*)(void))strnlen_SSE2,  .features = {[0] = CFB(CFEATURE_SSE2)|CFB(CFEATURE_CMOV)}},
#ifndef __x86_64__
	{.func = (void (*)(void))strnlen_SSE,   .features = {[0] = CFB(CFEATURE_SSE)|CFB(CFEATURE_CMOV)}},
	{.func = (void (*)(void))strnlen_SSE,   .features = {[2] = CFB(CFEATURE_MMXEXT), [0] = CFB(CFEATURE_CMOV)}},
#endif
	{.func = (void (*)(void))strnlen_x86,   .features = {}, .flags = CFF_DEFAULT},
};

DYN_JMP_DISPATCH(size_t, strnlen, (const char *s, size_t maxlen), (s, maxlen))

/*@unused@*/
static char const rcsid_snl[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
