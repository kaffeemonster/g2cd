/*
 * memchr.c
 * memchr, x86 implementation
 *
 * Copyright (c) 2010-2011 Jan Seiffert
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
static void *memchr_AVX2(const void *s, int c, size_t n)
{
	const unsigned char *p;
	void *ret;
	size_t f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
#  ifndef __x86_64__
		"vpbroadcastb	%5, %%ymm1\n\t"
#  else
		"vmovd	%k5, %%xmm2\n\t"
#  endif
		"vpxor	%%ymm0, %%ymm0, %%ymm0\n\t"
#  ifdef __x86_64__
		"vpbroadcastb	%%xmm2, %%ymm1\n\t"
#  endif
		"mov	%7, %k2\n\t"
		"vmovdqa	(%1), %%ymm0\n\t"
		"sub	%k3, %k2\n\t"
		"vpcmpeqb	%%ymm0, %%ymm1, %%ymm0\n\t"
		"vpmovmskb	%%ymm0, %k0\n\t"
		"shr	%b3, %k0\n\t"
		"shl	%b3, %k0\n\t"
		"sub	%4, %2\n\t"
		"jg	7f\n\t"
		"test	%k0, %k0\n\t"
		"jnz	4f\n\t"
		"neg	%2\n\t"
		"add	%7, %1\n\t"
		"jmp	2f\n"
		"7:\n\t"
		"mov	%k2, %k3\n\t"
		"jmp	3f\n"
		"6:\n\t"
		"xor	%0, %0\n\t"
		"jmp	5f\n"
		"8:\n\t"
		"vmovdqa	%%ymm2, %%ymm0\n\t"
		"add	%7, %1\n"
		"10:\n\t"
		"vpmovmskb	%%ymm0, %k0\n\t"
		"jmp	4f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"vmovdqa	(%1), %%ymm0\n\t"
		"vmovdqa	%c7(%1), %%ymm2\n\t"
		"vpcmpeqb	%%ymm0, %%ymm1, %%ymm0\n\t"
		"vpcmpeqb	%%ymm2, %%ymm1, %%ymm2\n\t"
		"vptest	%%ymm0, %%ymm0\n\t"
		"jnz	10b\n\t"
		"vptest	%%ymm2, %%ymm2\n\t"
		"jnz	8b\n"
		"sub	%7*2, %2\n\t"
		"add	%7*2, %1\n"
		"2:\n\t"
		"cmp	%7*2, %2\n\t"
		"jae	1b\n\t"
		"cmp	%7, %2\n\t"
		"jnae	9f\n\t"
		"vmovdqa	(%1), %%ymm0\n\t"
		"vpcmpeqb	%%ymm0, %%ymm1, %%ymm0\n\t"
		"vptest	%%ymm0, %%ymm0\n\t"
		"jnz	10b\n\t"
		"sub	%7, %2\n\t"
		"add	%7, %1\n"
		"9:\n\t"
		"cmp	$0, %2\n\t"
		"jle	6b\n\t"
		"vmovdqa	(%1), %%ymm0\n\t"
		"vpcmpeqb	%%ymm0, %%ymm1, %%ymm0\n\t"
		"vpmovmskb	%%ymm0, %k0\n\t"
		"mov	%7, %k3\n\t"
		"sub	%k2, %k3\n"
		"3:\n\t"
		"shl	%b3, %k0\n\t"
		"shr	%b3, %k0\n"
		"jz	6b\n"
		"4:\n\t"
		"bsf	%k0, %k0\n\t"
		"add	%1, %0\n"
		"5:\n\t"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=&c" (f)
	: 
#  ifndef __x86_64__
	  /* %4 */ "m" (n),
	  /* %5 */ "m" (c),
#  else
	  /* %4 */ "r" (n),
	  /* %5 */ "r" (c),
#  endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV32)),
	  /* %7 */ "i" (SOV32),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV32))
#  ifdef __AVX__
	: "ymm0", "ymm1", "ymm2"
#  elif defined(__SSE__)
	: "xmm0", "xmm1", "xmm2"
#  endif
	);
	return ret;
}
# endif

# if HAVE_BINUTILS >= 218
# if 0
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

static void *memchr_SSE41(const void *s, int c, size_t n)
{
	const unsigned char *p;
	void *ret;
	size_t f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"movd	%k5, %%xmm1\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"pshufb	%%xmm0, %%xmm1\n\t"
		"mov	%7, %k2\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"sub	%k3, %k2\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"shr	%b3, %k0\n\t"
		"shl	%b3, %k0\n\t"
		"sub	%4, %2\n\t"
		"jg	7f\n\t"
		"test	%k0, %k0\n\t"
		"jnz	4f\n\t"
		"neg	%2\n\t"
		"add	%7, %1\n\t"
		"jmp	2f\n"
		"7:\n\t"
		"mov	%k2, %k3\n\t"
		"jmp	3f\n"
		"6:\n\t"
		"xor	%0, %0\n\t"
		"jmp	5f\n"
		"8:\n\t"
		"movdqa	%%xmm2, %%xmm0\n\t"
		"add	%7, %1\n"
		"10:\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"jmp	4f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"movdqa	%c7(%1), %%xmm2\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm2\n\t"
		"ptest	%%xmm0, %%xmm0\n\t"
		"jnz	10b\n\t"
		"ptest	%%xmm2, %%xmm2\n\t"
		"jnz	8b\n"
		"sub	%7*2, %2\n\t"
		"add	%7*2, %1\n"
		"2:\n\t"
		"cmp	%7*2, %2\n\t"
		"jae	1b\n\t"
		"cmp	%7, %2\n\t"
		"jnae	9f\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"ptest	%%xmm0, %%xmm0\n\t"
		"jnz	10b\n\t"
		"sub	%7, %2\n\t"
		"add	%7, %1\n"
		"9:\n\t"
		"cmp	$0, %2\n\t"
		"jle	6b\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"mov	%7, %k3\n\t"
		"sub	%k2, %k3\n"
		"3:\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n"
		"jz	6b\n"
		"4:\n\t"
		"bsf	%k0, %k0\n\t"
		"add	%1, %0\n"
		"5:\n\t"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=&c" (f)
	: 
#  ifndef __x86_64__
	  /* %4 */ "m" (n),
	  /* %5 */ "m" (c),
#  else
	  /* %4 */ "r" (n),
	  /* %5 */ "r" (c),
#  endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV16)),
	  /* %7 */ "i" (SOV16),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV16))
#  ifdef __SSE__
	: "xmm0", "xmm1", "xmm2"
#  endif
	);
	return ret;
}
# endif

# if HAVE_BINUTILS >= 217
static void *memchr_SSSE3(const void *s, int c, size_t n)
{
	const unsigned char *p;
	void *ret;
	size_t f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"movd	%k5, %%xmm1\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"pshufb	%%xmm0, %%xmm1\n\t"
		"mov	%7, %k2\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"sub	%k3, %k2\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"shr	%b3, %k0\n\t"
		"shl	%b3, %k0\n\t"
		"sub	%4, %2\n\t"
		"jg	7f\n\t"
		"test	%k0, %k0\n\t"
		"jnz	4f\n\t"
		"neg	%2\n\t"
		"add	%7, %1\n\t"
		"jmp	2f\n"
		"7:\n\t"
		"mov	%k2, %k3\n\t"
		"jmp	3f\n"
		"6:\n\t"
		"xor	%0, %0\n\t"
		"jmp	5f\n"
		"8:\n\t"
		"mov	%k3, %k0\n\t"
		"add	%7, %1\n"
		"jmp	4f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"movdqa	%c7(%1), %%xmm2\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm2\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"pmovmskb	%%xmm2, %k3\n\t"
		"test	%k0, %k0\n\t"
		"jnz	4f\n"
		"test	%k3, %k3\n\t"
		"jnz	8b\n"
		"sub	%7*2, %2\n\t"
		"add	%7*2, %1\n"
		"2:\n\t"
		"cmp	%7*2, %2\n\t"
		"jae	1b\n\t"
		"cmp	%7, %2\n\t"
		"jnae	9f\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"test	%k0, %k0\n\t"
		"jnz	4f\n"
		"sub	%7, %2\n\t"
		"add	%7, %1\n"
		"9:\n\t"
		"cmp	$0, %2\n\t"
		"jle	6b\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"mov	%7, %k3\n\t"
		"sub	%k2, %k3\n"
		"3:\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n"
		"jz	6b\n"
		"4:\n\t"
		"bsf	%k0, %k0\n\t"
		"add	%1, %0\n"
		"5:\n\t"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=&c" (f)
	: 
#  ifndef __x86_64__
	  /* %4 */ "m" (n),
	  /* %5 */ "m" (c),
#  else
	  /* %4 */ "r" (n),
	  /* %5 */ "r" (c),
#  endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV16)),
	  /* %7 */ "i" (SOV16),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV16))
#  ifdef __SSE__
	: "xmm0", "xmm1", "xmm2"
#  endif
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
		"mov	%7, %k2\n\t"
		"movd	%k0, %%xmm1\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"pshuflw	$0b00000000, %%xmm1, %%xmm1\n\t"
		"pshufd	$0b00000000, %%xmm1, %%xmm1\n\t"
		"sub	%k3, %k2\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"shr	%b3, %k0\n\t"
		"shl	%b3, %k0\n\t"
		"sub	%4, %2\n\t"
		"jg	7f\n\t"
		"test	%k0, %k0\n\t"
		"jnz	4f\n\t"
		"neg	%2\n\t"
		"add	%7, %1\n\t"
		"jmp	2f\n"
		"7:\n\t"
		"mov	%k2, %k3\n\t"
		"jmp	3f\n"
		"6:\n\t"
		"xor	%0, %0\n\t"
		"jmp	5f\n"
		"8:\n\t"
		"mov	%k3, %k0\n\t"
		"add	%7, %1\n"
		"jmp	4f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"movdqa	%c7(%1), %%xmm2\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm2\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"pmovmskb	%%xmm2, %k3\n\t"
		"test	%k0, %k0\n\t"
		"jnz	4f\n"
		"test	%k3, %k3\n\t"
		"jnz	8b\n"
		"sub	%7*2, %2\n\t"
		"add	%7*2, %1\n"
		"2:\n\t"
		"cmp	%7*2, %2\n\t"
		"jae	1b\n\t"
		"cmp	%7, %2\n\t"
		"jnae	9f\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"test	%k0, %k0\n\t"
		"jnz	4f\n"
		"sub	%7, %2\n\t"
		"add	%7, %1\n"
		"9:\n\t"
		"cmp	$0, %2\n\t"
		"jle	6b\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"mov	%7, %k3\n\t"
		"sub	%k2, %k3\n"
		"3:\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n"
		"jz	6b\n"
		"4:\n\t"
		"bsf	%k0, %k0\n\t"
		"add	%1, %0\n"
		"5:\n\t"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=&c" (f)
	:
#ifndef __x86_64__
	  /* %4 */ "m" (n),
	  /* %5 */ "m" (c),
#else
	  /* %4 */ "r" (n),
	  /* %5 */ "r" (c),
#endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV16)),
	  /* %7 */ "i" (SOV16),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV16))
#ifdef __SSE__
	: "xmm0", "xmm1", "xmm2"
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
		"mov	%7, %k2\n\t"
		"movb	%5, %b0\n\t"
		"movb	%b0, %h0\n\t"
		"pinsrw	$0b00000000, %k0, %%mm1\n\t"
		"pshufw	$0b00000000, %%mm1, %%mm1\n\t"
		"movq	(%1), %%mm0\n\t"
		"sub	%k3, %k2\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pmovmskb	%%mm0, %k0\n\t"
		"shr	%b3, %k0\n\t"
		"shl	%b3, %k0\n\t"
		"sub	%4, %2\n\t"
		"jg	7f\n\t"
		"test	%k0, %k0\n\t"
		"jnz	4f\n\t"
		"neg	%2\n\t"
		"add	%7, %1\n\t"
		"jmp	2f\n"
		"7:\n\t"
		"mov	%k2, %k3\n\t"
		"jmp	3f\n"
		"6:\n\t"
		"xor	%0, %0\n\t"
		"jmp	5f\n"
		"8:\n\t"
		"mov	%k3, %k0\n\t"
		"add	%7, %1\n"
		"jmp	4f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"movq	(%1), %%mm0\n\t"
		"movq	%c7(%1), %%mm2\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pcmpeqb	%%mm1, %%mm2\n\t"
		"pmovmskb	%%mm0, %k0\n\t"
		"pmovmskb	%%mm2, %k3\n\t"
		"test	%k0, %k0\n\t"
		"jnz	4f\n"
		"test	%k3, %k3\n\t"
		"jnz	8b\n"
		"sub	%7*2, %2\n\t"
		"add	%7*2, %1\n"
		"2:\n\t"
		"cmp	%7*2, %2\n\t"
		"jae	1b\n\t"
		"cmp	%7, %2\n\t"
		"jnae	9f\n\t"
		"movq	(%1), %%mm0\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pmovmskb	%%mm0, %k0\n\t"
		"test	%k0, %k0\n\t"
		"jnz	4f\n"
		"sub	%7, %2\n\t"
		"add	%7, %1\n"
		"9:\n\t"
		"cmp	$0, %2\n\t"
		"jle	6b\n\t"
		"movq	(%1), %%mm0\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pmovmskb	%%mm0, %k0\n\t"
		"mov	%7, %k3\n\t"
		"sub	%k2, %k3\n"
		"3:\n\t"
		"shl	%b3, %b0\n\t"
		"shr	%b3, %b0\n"
		"jz	6b\n"
		"4:\n\t"
		"bsf	%k0, %k0\n\t"
		"add	%1, %0\n"
		"5:\n\t"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=&c" (f)
	: /* %4 */ "m" (n),
	  /* %5 */ "m" (c),
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV8)),
	  /* %7 */ "i" (SOV16),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV8))
# ifdef __MMX__
	: "mm0", "mm1", "mm2"
# endif
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


static __init_cdata const struct test_cpu_feature tfeat_my_memchr[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
	{.func = (void (*)(void))memchr_AVX2,  .features = {[4] = CFB(CFEATURE_AVX2)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 218
#if 0
// TODO: advanced code is buggy ATM
	{.func = (void (*)(void))memchr_SSE42, .features = {[1] = CFB(CFEATURE_SSE4_2)}},
#endif
	{.func = (void (*)(void))memchr_SSE41, .features = {[1] = CFB(CFEATURE_SSE4_1)}},
# endif
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))memchr_SSSE3, .features = {[1] = CFB(CFEATURE_SSSE3)}},
# endif
#endif
	{.func = (void (*)(void))memchr_SSE2,  .features = {[0] = CFB(CFEATURE_SSE2)}},
#ifndef __x86_64__
	{.func = (void (*)(void))memchr_SSE,   .features = {[0] = CFB(CFEATURE_SSE)}},
#endif
	{.func = (void (*)(void))memchr_x86,   .features = {}, .flags = CFF_DEFAULT},
};

#ifndef USE_SIMPLE_DISPATCH
# define NO_ALIAS
#endif
DYN_JMP_DISPATCH_ALIAS(void *, my_memchr, (const void *s, int c, size_t n), (s, c, n), memchr)

/*@unused@*/
static char const rcsid_mcx[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
