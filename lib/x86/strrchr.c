/*
 * strrchr.c
 * strrchr, x86 implementation
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

static noinline char *strrchr_null(const char *s, int c GCC_ATTR_UNUSED_PARAM)
{
	return (char *)(intptr_t)s + strlen(s);
}

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
static char *strrchr_AVX2(const char *s, int c)
{
	char *ret;
	const char *p, *p_o;
	size_t t, m;

	asm (
		/*
		 * hate doing this, but otherwise the compiler thinks
		 * he absolutly needs to put everything in the wrong
		 * reg, so he better shuffeles it all around, and for
		 * this he needs another reg, ohh, lets spill one...
		 */
		"mov	%5, %1\n\t"
		"prefetcht0	(%1)\n\t"
#  ifdef __i386__
		"vpbroadcastb	%6, %%ymm2\n\t"
#  else
		"vmovd	%k6, %%xmm0\n\t"
#  endif
		"mov	%1, %2\n\t"
		"and	%7*$-1, %1\n\t"
		"and	%7-1, %2\n\t"
		"vpxor	%%ymm1, %%ymm1, %%ymm1\n\t"
#  ifndef __i386__
		"vpbroadcastb	%%xmm0, %%ymm2\n\t"
#  endif
		"vmovdqa	(%1), %%ymm0\n\t"
		"vpcmpeqb	%%ymm0, %%ymm2, %%ymm3\n\t"
		"vpcmpeqb	%%ymm0, %%ymm1, %%ymm0\n\t"
		"vpmovmskb	%%ymm0, %k0\n\t"
		"vpmovmskb	%%ymm3, %k3\n\t"
		"shr	%b2, %0\n\t"
		"shr	%b2, %3\n\t"
		"shl	%b2, %0\n\t"
		"shl	%b2, %3\n\t"
		"vpxor	%%ymm4, %%ymm4, %%ymm4\n\t"
		"mov	%1, %4\n\t"
		"test	%0, %0\n\t"
		"jnz	3f\n\t"
		"test	%3, %3\n\t"
		"jz	1f\n\t"
		"vmovdqa	%%ymm3, %%ymm4\n"
		"jmp	1f\n"
		"6:\n\t"
		"vpmovmskb	%%ymm4, %k2\n\t"
		"test	%2, %2\n\t"
		"jz	5f\n\t"
		"bsr	%2, %0\n\t"
		"mov	%4, %1\n"
		"jmp	9f\n"
		"5:\n\t"
		"xor	%0, %0\n\t"
		"jmp	10f\n\t"
		".p2align 1\n"
		"1:\n\t"
		"vmovdqa	%c7(%1), %%ymm0\n\t"
		"add	%7, %1\n\t"
		"vpcmpeqb	%%ymm0, %%ymm2, %%ymm3\n\t"
		"vpcmpeqb	%%ymm0, %%ymm1, %%ymm0\n\t"
		"vptest	%%ymm0, %%ymm1\n\t"
		"jnc	2f\n\t"
		"vptest	%%ymm3, %%ymm1\n\t"
		"jc	1b\n\t"
		"mov	%1, %4\n\t"
		"vmovdqa	%%ymm3, %%ymm4\n\t"
		"jmp	1b\n"
		"2:\n\t"
		"vpmovmskb	%%ymm0, %k0\n\t"
		"vpmovmskb	%%ymm3, %k3\n"
		"3:\n\t"
		"test	%3, %3\n\t"
		"jz	6b\n\t"
		"bsf	%0, %2\n\t"
		"sub	%7, %2\n\t"
		"neg	%2\n\t"
		"shl	%b2, %k3\n\t"
		"shr	%b2, %k3\n\t"
		"test	%3, %3\n\t"
		"jz	6b\n\t"
		"bsr	%3, %0\n\t"
		"9:\n\t"
		"add	%1, %0\n"
		"10:"
		: /* %0 */ "=&a" (ret),
		  /* %1 */ "=&r" (p),
		  /* %2 */ "=&c" (t),
		  /* %3 */ "=&r" (m),
		  /* %4 */ "=&r" (p_o)
#  ifdef __i386__
		: /* %5 */ "m" (s),
		  /* %6 */ "m" (c),
#  else
		: /* %5 */ "r" (s),
		  /* %6 */ "r" (c),
#  endif
		  /* %7 */ "i" (SOV32)
#  ifdef __AVX__
		: "ymm0", "ymm1", "ymm2", "ymm3", "ymm4"
#  elif defined(__SSE__)
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4"
#  endif
	);
	return ret;
}
# endif

# if HAVE_BINUTILS >= 218
#if 0
/* needs adaption to match the proper last elem */
static char *strrchr_SSE42(const char *s, int c)
{
	char *ret;
	size_t t, z, m;
	const char *p, *p_o;

	if(unlikely(!c))
		return strrchr_null(s, c);

	/*
	 * even if nehalem can handle unaligned load much better
	 * (so they promised), we still align hard to get in
	 * swing with the page boundery.
	 */
	asm (
		"prefetcht0	(%5)\n\t"
		"mov	%5, %3\n\t"
		"pcmpeqb	%%xmm0, %%xmm0\n\t"
		"pslldq	$1, %%xmm0\n\t"
		"movd	%k4, %%xmm1\n\t"
		"pshufb	%%xmm0, %%xmm1\n\t"
		"and	$-16, %3\n\t"
		"mov	$16, %k0\n\t"
		"mov	%0, %1\n\t"
		/* ByteM,Norm,Any,Bytes */
		/*             6543210 */
		"pcmpestrm	$0b1000000, (%3), %%xmm1\n\t"
		"mov	%5, %2\n\t"
		"and	$0x0f, %2\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"shr	%b2, %0\n\t"
		"shl	%b2, %0\n\t"
		"bsf	%0, %2\n\t"
		"jz	7f\n\t"
		"cmp	%b4, (%3, %2)\n\t"
		"je	2f\n"
		"5:\n\t"
		"xor	%0, %0\n\t"
		"jmp	10f\n"
		"7:\n\t"
		"mov	%0, %4\n\t"
		"mov	%3, %5\n\t"
		"jmp	6f\n\t"
		".p2align 1\n"
		"3:\n\t"
		"mov	%2, %4\n\t"
		"mov	%3, %5\n\t"
		"1:\n\t"
		"jz	4f\n"
		"6:\n\t"
		"add	%1, %3\n\t"
		"prefetcht0	64(%3)\n\t"
		/* MSB,Norm,Any,Bytes */
		/*             6543210 */
		/* we need an MSB match, but this ingores input
		 * Element validity??? */
		"pcmpistri	$0b1000000, (%3), %%xmm1\n\t"
		"jc	3b\n\t"
		"jmp	1b\n"
		"4:\n\t"
		"test	%4, %4\n\t"
		"jz	5b\n\t"
		"mov	%5, %3\n\t"
		"mov	%4, %2\n"
		"2:\n\t"
		"lea	(%3,%2),%0\n"
		"10:"
		: /* %0 */ "=a" (ret),
		  /* %1 */ "=d" (z),
		  /* %2 */ "=c" (t),
		  /* %3 */ "=r" (p),
		  /* %4 */ "=r" (m),
		  /* %5 */ "=r" (p_o)
		: /* %6 */ "5" (s),
		  /* %7 */ "4" (c)
#  ifdef __SSE2__
		: "xmm0", "xmm1"
#  endif
	);
	return ret;
}
#endif

static char *strrchr_SSE41(const char *s, int c)
{
	char *ret;
	const char *p, *p_o;
	size_t t, m;

	asm (
		/*
		 * hate doing this, but otherwise the compiler thinks
		 * he absolutly needs to put everything in the wrong
		 * reg, so he better shuffeles it all around, and for
		 * this he needs another reg, ohh, lets spill one...
		 */
		"mov	%5, %1\n\t"
		"prefetcht0	(%1)\n\t"
		"movd	%k6, %%xmm2\n\t"
		"mov	%1, %2\n\t"
		"and	$-16, %1\n\t"
		"and	$0x0f, %2\n\t"
		"pxor	%%xmm1, %%xmm1\n\t"
		"pshufb	%%xmm1, %%xmm2\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"movdqa	%%xmm0, %%xmm3\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pcmpeqb	%%xmm2, %%xmm3\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"pmovmskb	%%xmm3, %3\n\t"
		"shr	%b2, %0\n\t"
		"shr	%b2, %3\n\t"
		"shl	%b2, %0\n\t"
		"shl	%b2, %3\n\t"
		"pxor	%%xmm4, %%xmm4\n\t"
		"mov	%1, %4\n\t"
		"test	%0, %0\n\t"
		"jnz	3f\n\t"
		"test	%3, %3\n\t"
		"jz	1f\n\t"
		"movdqa	%%xmm3, %%xmm4\n"
		"jmp	1f\n"
		"6:\n\t"
		"pmovmskb	%%xmm4, %2\n\t"
		"test	%2, %2\n\t"
		"jz	5f\n\t"
		"bsr	%2, %0\n\t"
		"mov	%4, %1\n"
		"jmp	9f\n"
		"5:\n\t"
		"xor	%0, %0\n\t"
		"jmp	10f\n\t"
		".p2align 1\n"
		"1:\n\t"
		"movdqa	16(%1), %%xmm0\n\t"
		"movdqa	%%xmm0, %%xmm3\n\t"
		"add	$16, %1\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pcmpeqb	%%xmm2, %%xmm3\n\t"
		"ptest	%%xmm0, %%xmm1\n\t"
		"jnc	2f\n\t"
		"ptest	%%xmm3, %%xmm1\n\t"
		"jc	1b\n\t"
		"mov	%1, %4\n\t"
		"movdqa	%%xmm3, %%xmm4\n\t"
		"jmp	1b\n"
		"2:\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"pmovmskb	%%xmm3, %3\n"
		"3:\n\t"
		"test	%3, %3\n\t"
		"jz	6b\n\t"
		"bsf	%0, %2\n\t"
		"sub	$16, %2\n\t"
		"neg	%2\n\t"
		"shl	%b2, %w3\n\t"
		"shr	%b2, %w3\n\t"
		"test	%3, %3\n\t"
		"jz	6b\n\t"
		"bsr	%3, %0\n\t"
		"9:\n\t"
		"add	%1, %0\n"
		"10:"
		: /* %0 */ "=&a" (ret),
		  /* %1 */ "=&r" (p),
		  /* %2 */ "=&c" (t),
		  /* %3 */ "=&r" (m),
		  /* %4 */ "=&r" (p_o)
#  ifdef __i386__
		: /* %5 */ "m" (s),
		  /* %6 */ "m" (c)
#  else
		: /* %5 */ "r" (s),
		  /* %6 */ "r" (c)
#  endif
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4"
#  endif
	);
	return ret;
}
# endif

# if HAVE_BINUTILS >= 217
static char *strrchr_SSSE3(const char *s, int c)
{
	char *ret;
	const char *p, *p_o;
	size_t t, m;

	asm (
		/*
		 * hate doing this, but otherwise the compiler thinks
		 * he absolutly needs to put everything in the wrong
		 * reg, so he better shuffeles it all around, and for
		 * this he needs another reg, ohh, lets spill one...
		 */
		"mov	%5, %1\n\t"
		"prefetcht0	(%1)\n\t"
		"movd	%k6, %%xmm2\n\t"
		"mov	%1, %2\n\t"
		"and	$-16, %1\n\t"
		"and	$0x0f, %2\n\t"
		"pxor	%%xmm1, %%xmm1\n\t"
		"pshufb	%%xmm1, %%xmm2\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"movdqa	%%xmm0, %%xmm3\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pcmpeqb	%%xmm2, %%xmm3\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"pmovmskb	%%xmm3, %3\n\t"
		"shr	%b2, %0\n\t"
		"shr	%b2, %3\n\t"
		"shl	%b2, %0\n\t"
		"shl	%b2, %3\n\t"
		"xor	%2, %2\n\t"
		"mov	%1, %4\n\t"
		"jmp	3f\n"
		"6:\n\t"
		"test	%2, %2\n\t"
		"jz	5f\n\t"
		"bsr	%2, %0\n\t"
		"mov	%4, %1\n"
		"jmp	9f\n"
		"5:\n\t"
		"xor	%0, %0\n\t"
		"jmp	10f\n\t"
		".p2align 1\n"
		"1:\n\t"
		"movdqa	16(%1), %%xmm0\n\t"
		"movdqa	%%xmm0, %%xmm3\n\t"
		"add	$16, %1\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pcmpeqb	%%xmm2, %%xmm3\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"pmovmskb	%%xmm3, %3\n"
		"3:\n\t"
		"test	%0, %0\n\t"
		"jnz	2f\n\t"
		"test	%3, %3\n\t"
		"jz	1b\n\t"
		"mov	%1, %4\n\t"
		"mov	%3, %2\n"
		"jmp	1b\n"
		"2:\n\t"
		"test	%3, %3\n\t"
		"jz	6b\n\t"
		"bsf	%0, %0\n\t"
		"sub	$16, %0\n\t"
		"neg	%0\n\t"
		"xchg	%0, %2\n\t"
		"shl	%b2, %w3\n\t"
		"shr	%b2, %w3\n\t"
		"mov	%0, %2\n\t"
		"test	%3, %3\n\t"
		"jz	6b\n\t"
		"bsr	%3, %0\n\t"
		"9:\n\t"
		"add	%1, %0\n"
		"10:"
		: /* %0 */ "=&a" (ret),
		  /* %1 */ "=&r" (p),
		  /* %2 */ "=&c" (t),
		  /* %3 */ "=&r" (m),
		  /* %4 */ "=&r" (p_o)
#  ifdef __i386__
		: /* %5 */ "m" (s),
		  /* %6 */ "m" (c)
#  else
		: /* %5 */ "r" (s),
		  /* %6 */ "r" (c)
#  endif
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3"
#  endif
	);
	return ret;
}
# endif
#endif

static char *strrchr_SSE2(const char *s, int c)
{
	char *ret;
	const char *p, *p_o;
	size_t t, m;

	asm (
		/*
		 * hate doing this, but otherwise the compiler thinks
		 * he absolutly needs to put everything in the wrong
		 * reg, so he better shuffeles it all around, and for
		 * this he needs another reg, ohh, lets spill one...
		 */
		"mov	%5, %1\n\t"
		"prefetcht0	(%1)\n\t"
		"movzbl	%b6, %k0\n\t"
		"mov	%k0, %k2\n\t"
		"shl	$8, %k2\n\t"
		"or	%k2, %k0\n\t"
		"movd	%k0, %%xmm2\n\t"
		"mov	%1, %2\n\t"
		"and	$-16, %1\n\t"
		"and	$0x0f, %2\n\t"
		"pxor	%%xmm1, %%xmm1\n\t"
		"pshuflw	$0b00000000, %%xmm2, %%xmm2\n\t"
		"pshufd	$0b00000000, %%xmm2, %%xmm2\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"movdqa	%%xmm0, %%xmm3\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pcmpeqb	%%xmm2, %%xmm3\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"pmovmskb	%%xmm3, %3\n\t"
		"shr	%b2, %0\n\t"
		"shr	%b2, %3\n\t"
		"shl	%b2, %0\n\t"
		"shl	%b2, %3\n\t"
		"xor	%2, %2\n\t"
		"mov	%1, %4\n\t"
		"jmp	3f\n"
		"6:\n\t"
		"test	%2, %2\n\t"
		"jz	5f\n"
		"bsr	%2, %0\n\t"
		"mov	%4, %1\n\t"
		"jmp	9f\n"
		"5:\n\t"
		"xor	%0, %0\n\t"
		"jmp	10f\n\t"
		".p2align 1\n"
		"1:\n\t"
		"movdqa	16(%1), %%xmm0\n\t"
		"movdqa	%%xmm0, %%xmm3\n\t"
		"add	$16, %1\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pcmpeqb	%%xmm2, %%xmm3\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"pmovmskb	%%xmm3, %3\n"
		"3:\n\t"
		"test	%0, %0\n\t"
		"jnz	2f\n\t"
		"test	%3, %3\n\t"
		"jz	1b\n\t"
		"mov	%1, %4\n\t"
		"mov	%3, %2\n"
		"jmp	1b\n"
		"2:\n\t"
		"test	%3, %3\n\t"
		"jz	6b\n\t"
		"bsf	%0, %0\n\t"
		"sub	$16, %0\n\t"
		"neg	%0\n\t"
		"xchg	%0, %2\n\t"
		"shl	%b2, %w3\n\t"
		"shr	%b2, %w3\n\t"
		"mov	%0, %2\n\t"
		"test	%3, %3\n\t"
		"jz	6b\n\t"
		"bsr	%3, %0\n\t"
		"9:\n\t"
		"add	%1, %0\n"
		"10:"
		: /* %0 */ "=&a" (ret),
		  /* %1 */ "=&r" (p),
		  /* %2 */ "=&c" (t),
		  /* %3 */ "=&r" (m),
		  /* %4 */ "=&r" (p_o)
#ifdef __i386__
		: /* %5 */ "m" (s),
		  /* %6 */ "m" (c)
#else
		: /* %5 */ "r" (s),
		  /* %6 */ "r" (c)
#endif
#ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3"
#endif
	);
	return ret;
}

#ifndef __x86_64__
static char *strrchr_SSE(const char *s, int c)
{
	char *ret;
	const char *p, *p_o;
	size_t t, m;

	asm (
		"mov	%5, %1\n\t"
		"prefetcht0 (%1)\n\t"
		"movb	%6, %b0\n\t"
		"movb	%b0, %h0\n\t"
		"mov	%1, %2\n\t"
		"and	$-8, %1\n\t"
		"and	$0x7, %2\n\t"
		"pxor	%%mm1, %%mm1\n\t"
		"pinsrw	$0b00000000, %k0, %%mm2\n\t"
		"pshufw	$0b00000000, %%mm2, %%mm2\n\t"
		"movq	(%1), %%mm0\n\t"
		"movq	%%mm0, %%mm3\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pcmpeqb	%%mm2, %%mm3\n\t"
		"pmovmskb	%%mm0, %0\n\t"
		"pmovmskb	%%mm3, %3\n\t"
		"shr	%b2, %0\n\t"
		"shr	%b2, %3\n\t"
		"shl	%b2, %0\n\t"
		"shl	%b2, %3\n\t"
		"xor	%2, %2\n\t"
		"mov	%1, %4\n\t"
		"jmp	3f\n"
		"6:\n\t"
		"test	%2, %2\n\t"
		"jz	5f\n\t"
		"bsr	%2, %0\n\t"
		"mov	%4, %1\n"
		"jmp	9f\n"
		"5:\n\t"
		"xor	%0, %0\n\t"
		"jmp	10f\n\t"
		".p2align 1\n"
		"1:\n\t"
		"movq	8(%1), %%mm0\n\t"
		"movq	%%mm0, %%mm3\n\t"
		"add	$8, %1\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pcmpeqb	%%mm2, %%mm3\n\t"
		"pmovmskb	%%mm0, %0\n\t"
		"pmovmskb	%%mm3, %3\n"
		"3:\n\t"
		"test	%0, %0\n\t"
		"jnz	2f\n\t"
		"test	%3, %3\n\t"
		"jz	1b\n\t"
		"mov	%1, %4\n\t"
		"mov	%3, %2\n"
		"jmp	1b\n"
		"2:\n\t"
		"test	%3, %3\n\t"
		"jz	6b\n\t"
		"bsf	%0, %0\n\t"
		"sub	$8, %0\n\t"
		"neg	%0\n\t"
		"xchg	%0, %2\n\t"
		"shl	%b2, %b3\n\t"
		"shr	%b2, %b3\n\t"
		"mov	%0, %2\n\t"
		"test	%3, %3\n\t"
		"jz	6b\n\t"
		"bsr	%3, %0\n\t"
		"9:\n\t"
		"add	%1, %0\n"
		"10:"
		: /* %0 */ "=&a" (ret),
		  /* %1 */ "=r" (p),
		  /* %2 */ "=c" (t),
		  /* %3 */ "=q" (m),
		  /* %4 */ "=r" (p_o)
		: /* %5 */ "m" (s),
		  /* %6 */ "m" (c)
# ifdef __MMX__
		: "mm0", "mm1", "mm2", "mm3"
# endif
	);
	return ret;
}
#endif

static char *strrchr_x86(const char *s, int c)
{
	const char *p;
	size_t r, m, mask, x;
	struct {
		const char *p;
		size_t m;
	} l_match;
	unsigned shift;

	/* only do this in scalar code the SIMD versions are fast,
	 * better avoid the overhead there */
	if(unlikely(!c))
		return strrchr_null(s, c);

	mask = (c & 0xFF) * MK_C(0x01010101);
	p  = (const char *)ALIGN_DOWN(s, SOST);
	shift = ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR;
	x  = *(const size_t *)p;
	if(!HOST_IS_BIGENDIAN)
		x >>= shift;
	r  = has_nul_byte(x);
	x ^= mask;
	m  = has_nul_byte(x);
	r <<= shift;
	m <<= shift;
	if(HOST_IS_BIGENDIAN) {
		r >>= shift;
		m >>= shift;
	}
	l_match.p = p;
	l_match.m = 0;

	while(!r)
	{
		if(m) {
			l_match.p = p;
			l_match.m = m;
		}
		p += SOST;
		x  = *(const size_t *)p;
		r  = has_nul_byte(x);
		x ^= mask;
		m  = has_nul_byte(x);
	}
	if(m) {
		r = nul_byte_index(r);
		if(!HOST_IS_BIGENDIAN) {
			r  = 1u << ((r * BITS_PER_CHAR)+BITS_PER_CHAR-1u);
			r |= r - 1u;
		} else {
			r  = 1u << (((SOSTM1-r) * BITS_PER_CHAR)+BITS_PER_CHAR-1u);
			r |= r - 1u;
			r  = ~r;
		}
		m &= r;
		if(m)
			return (char *)(uintptr_t)p + nul_byte_index_last(m);
	}
	if(l_match.m)
		return (char *)(uintptr_t)l_match.p + nul_byte_index_last(l_match.m);
	return NULL;
}

static __init_cdata const struct test_cpu_feature tfeat_strrchr[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
	{.func = (void (*)(void))strrchr_AVX2,  .features = {[4] = CFB(CFEATURE_AVX2)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 218
#if 0
	{.func = (void (*)(void))strrchr_SSE42, .features = {[1] = CFB(CFEATURE_SSE4_2)}},
#endif
	{.func = (void (*)(void))strrchr_SSE41, .features = {[1] = CFB(CFEATURE_SSE4_1)}},
# endif
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))strrchr_SSSE3, .features = {[1] = CFB(CFEATURE_SSSE3)}},
# endif
#endif
	{.func = (void (*)(void))strrchr_SSE2,  .features = {[0] = CFB(CFEATURE_SSE2)}},
#ifndef __x86_64__
	{.func = (void (*)(void))strrchr_SSE,   .features = {[0] = CFB(CFEATURE_SSE)}},
	{.func = (void (*)(void))strrchr_SSE,   .features = {[2] = CFB(CFEATURE_MMXEXT)}},
#endif
	{.func = (void (*)(void))strrchr_x86,   .features = {}, .flags = CFF_DEFAULT},
};

#ifndef USE_SIMPLE_DISPATCH
# define NO_ALIAS
#endif
DYN_JMP_DISPATCH_ALIAS(char *, strrchr, (const char *s, int c), (s, c), rindex)

/*@unused@*/
static char const rcsid_srcx[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
