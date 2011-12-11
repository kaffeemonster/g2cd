/*
 * mem_searchrn.c
 * search mem for a \r\n, x86 implementation
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
 * Even this beeing a mem* function, the len can be seen as a
 * "hint". We can overread and underread, but should cut the
 * result (and not pass a page boundery, but we cannot because
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

#define SOV8	8
#define SOV8M1	(SOV8-1)
#define SOV16	16
#define SOV16M1	(SOV16-1)
#define SOV32	32
#define SOV32M1	(SOV32-1)

static const struct {
	char d[SOV32];
} y GCC_ATTR_ALIGNED(SOV32) =
{{'\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r',
  '\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r',
  '\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r',
  '\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r'}};
static const struct {
	char d[SOV32];
} z GCC_ATTR_ALIGNED(SOV32) =
{{'\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n',
  '\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n',
  '\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n',
  '\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n'}};

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
static void *mem_searchrn_AVX2(void *src, size_t len);
# endif
# if HAVE_BINUTILS >= 218
static void *mem_searchrn_SSE42(void *src, size_t len);
# endif
#endif
static void *mem_searchrn_SSE2(void *src, size_t len);
#ifndef __x86_64__
static void *mem_searchrn_SSE(void *src, size_t len);
#endif
static void *mem_searchrn_x86(void *src, size_t len);

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
static void *mem_searchrn_AVX2(void *s, size_t len)
{
	char *p, *f;
	ssize_t k;
	size_t rr, t;

	asm (
		"prefetcht0	(%1)\n\t"
		"test	%7, %7\n\t" /* len NULL? */
#  ifdef __x86_64__
		"cmovz	%7, %0\n\t" /* create NULL */
#  endif
		"jz	9f\n\t" /* outa here */
		"test	%1, %1\n\t" /* src NULL? */
		"cmovz	%1, %0\n\t" /* create NULL */
		"jz	9f\n\t" /* outa here */
		"vmovdqa	%8, %%ymm0\n\t" /* load search strings */
		"vmovdqa	%9, %%ymm1\n\t"
		"mov	%1, %2\n\t" /* duplicate src */
		"and	$31, %2\n\t" /* create align difference */
		"and	$-32, %1\n\t" /* align down src */
		"vmovdqa	(%1), %%ymm3\n\t" /* load data */
		"vmovdqa	%%ymm3, %%ymm2\n\t"
		"sub	%2, %4\n\t" /* k -= align diff */
		"vpcmpeqb	%%ymm3, %%ymm1, %%ymm3\n\t"
		"vpcmpeqb	%%ymm2, %%ymm0, %%ymm2\n\t"
		"vpsrldq	$1, %%ymm3, %%ymm3\n\t" /* shift '\n' one down */
		"sub	%7, %4\n\t" /* k -= len */
		"vpmovmskb	%%ymm3, %3\n\t"
		"vpmovmskb	%%ymm2, %0\n\t"
		"ja	6f\n\t" /* k > 0 ? -> we are done */
		"shr	%b2, %0\n\t" /* mask out lower stuff */
		"shl	%b2, %0\n\t"
		"xchg	%3, %0\n\t" /* copy rr to last_rr */
		"and	%3, %0\n\t" /* put rn together with rr */
		"bsf	%0, %0\n\t" /* create index */
		"jnz	7f\n\t" /* match? -> out */
		"neg	%4\n\t" /* restore len */
		"jmp	3f\n\t" /* jump in loop */
		".p2align 2\n"
		"1:\n\t"
		"sub	$32, %4\n\t" /* buffer space left? */
		"jb	4f\n" /* are we at the buffer end? -> adjust mask */
		"3:\n\t"
		"shr	$31, %k3\n\t" /* pull high bit down */
		"vmovdqa	32(%1), %%ymm2\n\t" /* load next data */
		"vmovdqa	%%ymm2, %%ymm3\n\t" /* load next data */
		"add	$32, %1\n\t" /* p += SOV32 */
		"vpcmpeqb	%%ymm3, %%ymm1, %%ymm3\n\t"
		"vpcmpeqb	%%ymm2, %%ymm0, %%ymm2\n\t"
		"vpmovmskb	%%ymm3, %k2\n\t"
		"and	%k2, %k3\n\t" /* match between rn and last_rr? */
		"neg	%k3\n\t"
		"cmovc	%k3, %k0\n\t"
		"jc	7f\n\t"
		"vpmovmskb	%%ymm2, %k0\n\t"
		"shr	$1, %k2\n\t"
		"cdq\n\t" /* copy high bit */
		"and	%k2, %k0\n\t" /* generate match */
		"jz	1b\n\t" /* no bits overlapped? -> continue loop */
		"sub	$32, %4\n\t" /* buffer space left? */
		"jb	4f\n" /* are we at the buffer end? -> adjust mask */
		"2:\n\t"
		"bsf	%0, %0\n\t" /* find index */
		"cmovz	%0, %1\n" /* no match, set p to zero */
		"7:\n\t"
		"add	%1, %0\n\t" /* add match index to p */
		/*
		 * done!
		 * out
		 */
#  ifdef __ELF__
		".subsection 2\n\t"
#  else
		"jmp	9f\n\t"
#  endif
		".p2align 2\n"
		"4:\n\t"
		"neg	%4\n\t" /* correct k's sign ;) */
		"jmp	10f\n\t"
		".p2align 1\n"
		"6:\n\t"
		"and	%3, %0\n\t" /* see if an '\n' matches */
		"shr	%b2, %0\n\t" /* cut mask lower bits */
		"shl	%b2, %0\n"
		"10:\n\t"
		"mov	%4, %2\n\t" /* get k */
		"shl	%b2, %k0\n\t" /* cut mask upper bits */
		"shr	%b2, %k0\n\t"
		"jmp 2b\n\t"/* back to generate result */
#  ifdef __ELF__
		".previous\n"
#  endif
		"9:"
	: /*  %0 */ "=a" (p),
	  /*  %1 */ "=r" (f),
	  /*  %2 */ "=c" (rr),
	  /*  %3 */ "=d" (k),
	  /*  %4 */ "=r" (t)
	: /*  %5 */ "4" (SOV32),  /* k = SOV32 */
	  /*  %6 */ "1" (s),
#  ifndef __x86_64__
	  /*  %7 */ "0" (len),
#  else
	  /* amd64 has enough call clobbered regs not to spill */
	  /*  %7 */ "r" (len),
#  endif
	  /*  %8 */ "m" (y),
	  /*  %9 */ "m" (z)
#  ifdef __AVX__
	: "ymm0", "ymm1", "ymm2", "ymm3"
#  elif defined(__SSE__)
	: "xmm0", "xmm1", "xmm2", "xmm3"
#  endif
	);

	return p;
}
# endif

# if HAVE_BINUTILS >= 218
static void *mem_searchrn_SSE42(void *s, size_t len)
{
	char *p, *f;
	ssize_t k;
	size_t rr;
	static const size_t t = 0;

	asm (
		"prefetcht0	(%1)\n\t"
		"test	%1, %1\n\t" /* src NULL? */
		"jz	9f\n\t" /* outa here */
		"test	%3, %3\n\t" /* len NULL? */
		"jz	9f\n\t" /* outa here */
		"add	$2, %0\n\t" /* create a 2 */
		"movd	%k2, %%xmm1\n\t"
		"mov	%1, %2\n\t" /* duplicate src */
		"and	$15, %2\n\t" /* create align difference */
		"and	$-16, %1\n\t" /* align down src */
		"add	%2, %3\n\t" /* len += align diff */
		/* ByteM,Masked+,EqualO,Bytes */
		/*             6543210 */
		"pcmpestrm	$0b1101100, (%1), %%xmm1\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"shr	%b2, %0\n\t" /* mask out lower stuff */
		"shl	%b2, %0\n\t"
		"mov	%3, %2\n\t" /* get len */
		"neg	%2\n\t" /* negate */
		"add	$16, %2\n\t" /* add SOV16 */
		"cmovs	%8, %2\n" /* create a 0 if k < 0 */
		"inc	%b2\n\t" /* shift out last rr bit */
		"shl	%b2, %0\n\t"
		"shr	%b2, %w0\n\t"
		"bsf	%w0, %w2\n\t" /* create index */
		"jnz	7f\n\t" /* match -> out */
		"cmp	$16, %3\n\t" /* len left? */
		"jbe	9f\n\t" /* no -> done */
		"shr	$17, %0\n\t" /* put eax.17 in carry */
		"lea	2(%0), %0\n\t" /* create a two without changing flags */
		"jnc	3f\n\t" /* eax.17 == 0 ? -> start normal */
		".p2align 2\n"
		"4:\n\t"
		"cmpb	$0x0A, 16(%1)\n\t" /* is first byte in str a '\n'? */
		"je	7f\n\t"
		".p2align 1\n"
		"3:\n\t"
		"sub	$16, %3\n" /* len -= SOV16 */
		"add	$16, %1\n\t" /* p += SOV16 */
		/* LSB,Norm,EqualO,Bytes */
		/*             6543210 */
		"pcmpestri	$0b0001100, (%1), %%xmm1\n\t"
		"ja	3b\n\t" /* no match(C) or end(Z)? -> continue */
		"jz	2f\n\t" /* end? -> don't check match position */
		"cmp	$15, %2\n\t" /* index of last char? */
		"je	4b\n" /* last char matched? - > continue */
		"jmp	7f\n\t"
		"2:\n\t"
# ifndef __x86_64__
		"setc	%b0\n\t" /* create a 0 if no match was generated */
		"cmovnc	%0, %2\n\t" /* no match, set rr to zero */
		"cmovnc	%0, %1\n" /* no match, set p to zero */
# else
		"cmovnc	%8, %2\n\t" /* no match, set rr to zero */
		"cmovnc	%8, %1\n" /* no match, set p to zero */
# endif
		"7:\n\t"
		"movzx	%w2, %2\n\t" /* clear upper half, result is small */
		"lea	(%1, %2), %0\n" /* add match index to p */
		"9:\n\t"
		/*
		 * done!
		 * out
		 */
	: /*  %0 */ "=a" (f),
	  /*  %1 */ "=r" (p),
	  /*  %2 */ "=c" (rr),
	  /*  %3 */ "=d" (k)
	: /*  %4 */ "0" (0),
	  /*  %5 */ "1" (s),
	  /*  %6 */ "3" (len),
	  /*  %7 */ "2" (0x0A0D),
#  ifndef __x86_64__
	  /*  %8 */ "m" (t)
#  else
	  /* amd64 has enough call clobbered regs not to spill */
	  /*  %8 */ "r" (t)
#  endif
#  ifdef __SSE__
	: "xmm0", "xmm1", "xmm2"
#  endif
	);
	return f;
}
# endif
#endif

static void *mem_searchrn_SSE2(void *s, size_t len)
{
	char *p, *f;
	ssize_t k;
	size_t rr, t;

	asm (
		"prefetcht0	(%1)\n\t"
		"test	%7, %7\n\t" /* len NULL? */
#ifdef __x86_64__
		"cmovz	%7, %0\n\t" /* create NULL */
#endif
		"jz	9f\n\t" /* outa here */
		"test	%1, %1\n\t" /* src NULL? */
		"cmovz	%1, %0\n\t" /* create NULL */
		"jz	9f\n\t" /* outa here */
		"movdqa	%8, %%xmm0\n\t" /* load search strings */
		"movdqa	%9, %%xmm1\n\t"
		"mov	%1, %2\n\t" /* duplicate src */
		"and	$15, %2\n\t" /* create align difference */
		"and	$-16, %1\n\t" /* align down src */
		"movdqa	(%1), %%xmm3\n\t" /* load data */
		"movdqa	%%xmm3, %%xmm2\n\t"
		"sub	%2, %4\n\t" /* k -= align diff */
		"pcmpeqb	%%xmm1, %%xmm3\n\t"
		"pcmpeqb	%%xmm0, %%xmm2\n\t"
		"psrldq	$1, %%xmm3\n\t" /* shift '\n' one down */
		"sub	%7, %4\n\t" /* k -= len */
		"pmovmskb	%%xmm3, %3\n\t"
		"pmovmskb	%%xmm2, %0\n\t"
		"ja	6f\n\t" /* k > 0 ? -> we are done */
		"shr	%b2, %0\n\t" /* mask out lower stuff */
		"shl	%b2, %0\n\t"
		"xchg	%3, %0\n\t" /* copy rr to last_rr */
		"and	%3, %0\n\t" /* put rn together with rr */
		"bsf	%0, %0\n\t" /* create index */
		"jnz	7f\n\t" /* match? -> out */
		"neg	%4\n\t" /* restore len */
		"jmp	3f\n\t" /* jump in loop */
		".p2align 2\n"
		"1:\n\t"
		"sub	$16, %4\n\t" /* buffer space left? */
		"jb	4f\n" /* are we at the buffer end? -> adjust mask */
		"3:\n\t"
		"shr	$15, %3\n\t" /* pull high bit down */
		"movdqa	16(%1), %%xmm2\n\t" /* load next data */
		"movdqa	%%xmm2, %%xmm3\n\t" /* load next data */
		"add	$16, %1\n\t" /* p += SOV16 */
		"pcmpeqb	%%xmm1, %%xmm3\n\t"
		"pcmpeqb	%%xmm0, %%xmm2\n\t"
		"pmovmskb	%%xmm3, %2\n\t"
		"and	%2, %3\n\t" /* match between rn and last_rr? */
		"neg	%3\n\t"
		"cmovc	%3, %0\n\t"
		"jc	7f\n\t"
		"pmovmskb	%%xmm2, %0\n\t"
		"shr	$1, %2\n\t"
		"cwd\n\t" /* copy high bit */
		"and	%2, %0\n\t" /* generate match */
		"jz	1b\n\t" /* no bits overlapped? -> continue loop */
		"sub	$16, %4\n\t" /* buffer space left? */
		"jb	4f\n" /* are we at the buffer end? -> adjust mask */
		"2:\n\t"
		"bsf	%0, %0\n\t" /* find index */
		"cmovz	%0, %1\n" /* no match, set p to zero */
		"7:\n\t"
		"add	%1, %0\n\t" /* add match index to p */
		/*
		 * done!
		 * out
		 */
#ifdef __ELF__
		".subsection 2\n\t"
#else
		"jmp	9f\n\t"
#endif
		".p2align 2\n"
		"4:\n\t"
		"neg	%4\n\t" /* correct k's sign ;) */
		"jmp	10f\n\t"
		".p2align 1\n"
		"6:\n\t"
		"and	%3, %0\n\t" /* see if an '\n' matches */
		"shr	%b2, %0\n\t" /* cut mask lower bits */
		"shl	%b2, %0\n"
		"10:\n\t"
		"mov	%4, %2\n\t" /* get k */
		"shl	%b2, %w0\n\t" /* cut mask upper bits */
		"shr	%b2, %w0\n\t"
		"jmp 2b\n\t"/* back to generate result */
#ifdef __ELF__
		".previous\n"
#endif
		"9:"
	: /*  %0 */ "=a" (p),
	  /*  %1 */ "=r" (f),
	  /*  %2 */ "=c" (rr),
	  /*  %3 */ "=d" (k),
	  /*  %4 */ "=r" (t)
	: /*  %5 */ "4" (SOV16),  /* k = SOV16 */
	  /*  %6 */ "1" (s),
#ifndef __x86_64__
	  /*  %7 */ "0" (len),
#else
	  /* amd64 has enough call clobbered regs not to spill */
	  /*  %7 */ "r" (len),
#endif
	  /*  %8 */ "m" (y),
	  /*  %9 */ "m" (z)
#ifdef __SSE__
	: "xmm0", "xmm1", "xmm2", "xmm3"
#endif
	);

	return p;
}

#ifndef __x86_64__
static void *mem_searchrn_SSE(void *s, size_t len)
{
	char *p, *f;
	ssize_t k;
	size_t rr, t;

	asm (
		"prefetcht0	(%1)\n\t"
		"test	%7, %7\n\t" /* len NULL? */
		"jz	9f\n\t" /* outa here */
		"test	%1, %1\n\t" /* src NULL? */
		"cmovz	%1, %0\n\t" /* create NULL */
		"jz	9f\n\t" /* outa here */
		"movq	%8, %%mm0\n\t" /* load search strings */
		"movq	%9, %%mm1\n\t"
		"mov	%1, %2\n\t" /* duplicate src */
		"and	$7, %2\n\t" /* create align difference */
		"and	$-8, %1\n\t" /* align down src */
		"movq	(%1), %%mm3\n\t" /* load data */
		"movq	%%mm3, %%mm2\n\t"
		"sub	%2, %4\n\t" /* k -= align diff */
		"pcmpeqb	%%mm1, %%mm3\n\t"
		"pcmpeqb	%%mm0, %%mm2\n\t"
		"pmovmskb	%%mm3, %3\n\t"
		"shr	$1, %3\n\t" /* shift '\n' one down */
		"sub	%7, %4\n\t" /* k -= len */
		"pmovmskb	%%mm2, %0\n\t"
		"ja	6f\n\t" /* k > 0 ? -> we are done */
		"shr	%b2, %0\n\t" /* mask out lower stuff */
		"shl	%b2, %0\n\t"
		"xchg	%3, %0\n\t" /* copy rr to last_rr */
		"and	%3, %0\n\t" /* put rn together with rr */
		"bsf	%0, %0\n\t" /* create index */
		"jnz	7f\n\t" /* match? -> out */
		"neg	%4\n\t" /* restore len */
		"jmp	3f\n\t" /* jump in loop */
		".p2align 2\n"
		"1:\n\t"
		"sub	$8, %4\n\t" /* buffer space left? */
		"jb	4f\n" /* are we at the buffer end? -> adjust mask */
		"3:\n\t"
		"shr	$7, %3\n\t" /* pull high bit down */
		"movq	8(%1), %%mm2\n\t" /* load next data */
		"movq	%%mm2, %%mm3\n\t" /* load next data */
		"add	$8, %1\n\t" /* p += SOV8 */
		"pcmpeqb	%%mm1, %%mm3\n\t"
		"pcmpeqb	%%mm0, %%mm2\n\t"
		"pmovmskb	%%xmm3, %2\n\t"
		"and	%2, %3\n\t" /* match between rn and last_rr? */
		"neg	%3\n\t"
		"cmovc	%3, %0\n\t"
		"jc	7f\n\t"
		"pmovmskb	%%mm2, %0\n\t"
		"shr	$1, %2\n\t"
		"cwd\n\t" /* copy high bit */
		"and	%2, %0\n\t" /* generate match */
		"jz	1b\n\t" /* no bits overlapped? -> continue loop */
		"sub	$8, %4\n\t" /* buffer space left? */
		"jb	4f\n" /* are we at the buffer end? -> adjust mask */
		"2:\n\t"
		"bsf	%0, %0\n\t" /* find index */
		"cmovz	%0, %1\n" /* no match, set p to zero */
		"7:\n\t"
		"add	%1, %0\n\t" /* add match index to p */
		/*
		 * done!
		 * out
		 */
#ifdef __ELF__
		".subsection 2\n\t"
#else
		"jmp	9f\n\t"
#endif
		".p2align 2\n"
		"4:\n\t"
		"neg	%4\n\t" /* correct k's sign ;) */
		"jmp	10f\n\t"
		".p2align 1\n"
		"6:\n\t"
		"and	%3, %0\n\t" /* see if an '\n' matches */
		"shr	%b2, %0\n\t" /* cut mask lower bits */
		"shl	%b2, %0\n"
		"10:\n\t"
		"mov	%4, %2\n\t" /* get k */
		"shl	%b2, %w0\n\t" /* cut mask upper bits */
		"shr	%b2, %w0\n\t"
		"jmp 2b\n\t"/* back to generate result */
#ifdef __ELF__
		".previous\n"
#endif
		"9:"
	: /*  %0 */ "=a" (p),
	  /*  %1 */ "=r" (f),
	  /*  %2 */ "=c" (rr),
	  /*  %3 */ "=d" (k),
	  /*  %4 */ "=r" (t)
	: /*  %5 */ "4" (SOV8),  /* k = SOV8 */
	  /*  %6 */ "1" (s),
	  /*  %7 */ "0" (len),
	  /*  %8 */ "m" (y),
	  /*  %9 */ "m" (z)
#ifdef __SSE__
	: "xmm0", "xmm1", "xmm2", "xmm3"
#endif
	);
	return p;
}
#endif

void *mem_searchrn_x86(void *s, size_t len)
{
	char *p;
	size_t rr, rn, last_rr = 0;
	ssize_t f, k;
	prefetch(s);

	if(unlikely(!s || !len))
		return NULL;

	f = ALIGN_DOWN_DIFF(s, SOST);
	k = SOST - f - (ssize_t) len;
	k = k > 0 ? k : 0;

	p  = (char *)ALIGN_DOWN(s, SOST);
	rn = (*(size_t *)p);
	rr = rn ^ MK_C(0x0D0D0D0D); /* \r\r\r\r */
	rr = has_nul_byte(rr);
	rr <<= k * BITS_PER_CHAR;
	rr >>= k * BITS_PER_CHAR;
	rr >>= f * BITS_PER_CHAR;
	rr <<= f * BITS_PER_CHAR;
	if(unlikely(rr))
	{
		last_rr = rr >> SOSTM1 * BITS_PER_CHAR;
		rn ^= MK_C(0x0A0A0A0A); /* \n\n\n\n */
		rn  = has_nul_byte(rn);
		rr &= rn >> BITS_PER_CHAR;
		if(rr)
			return p + nul_byte_index(rr);
	}
	if(unlikely(k))
		return NULL;

	len -= SOST - f;
	do
	{
		p += SOST;
		rn = *(size_t *)p;
		rr = rn ^ MK_C(0x0D0D0D0D); /* \r\r\r\r */
		rr = has_nul_byte(rr);
		if(unlikely(len <= SOST))
			break;
		len -= SOST;
		if(rr || last_rr)
		{
			rn ^= MK_C(0x0A0A0A0A); /* \n\n\n\n */
			rn = has_nul_byte(rn);
			last_rr &= rn;
			if(last_rr)
				return p - 1;
			last_rr = rr >> SOSTM1 * BITS_PER_CHAR;
			rr &= rn >> BITS_PER_CHAR;
			if(rr)
				return p + nul_byte_index(rr);
		}
	} while(1);
	k = SOST - (ssize_t) len;
	k = k > 0 ? k : 0;
	rr <<= k * BITS_PER_CHAR;
	rr >>= k * BITS_PER_CHAR;
	if(rr || last_rr)
	{
		rn ^= MK_C(0x0A0A0A0A); /* \n\n\n\n */
		rn = has_nul_byte(rn);
		last_rr &= rn;
		if(last_rr)
			return p - 1;
		rr &= rn >> BITS_PER_CHAR;
		if(rr)
			return p + nul_byte_index(rr);
	}

	return NULL;
}

static __init_cdata const struct test_cpu_feature tfeat_mem_searchrn[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
	{.func = (void (*)(void))mem_searchrn_AVX2,  .features = {[4] = CFB(CFEATURE_AVX2)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 218
	{.func = (void (*)(void))mem_searchrn_SSE42, .features = {[1] = CFB(CFEATURE_SSE4_2)}},
# endif
#endif
	{.func = (void (*)(void))mem_searchrn_SSE2,  .features = {[0] = CFB(CFEATURE_SSE2)}},
#ifndef __x86_64__
	{.func = (void (*)(void))mem_searchrn_SSE,   .features = {[0] = CFB(CFEATURE_SSE)}},
	{.func = (void (*)(void))mem_searchrn_SSE,   .features = {[2] = CFB(CFEATURE_MMXEXT)}},
#endif
	{.func = (void (*)(void))mem_searchrn_x86,   .features = {}, .flags = CFF_DEFAULT},
};

DYN_JMP_DISPATCH(void *, mem_searchrn, (void *s, size_t len), (s, len))

/*@unused@*/
static char const rcsid_msrn[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
