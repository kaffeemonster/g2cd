/*
 * mem_spn_ff.c
 * count 0xff span length, x86 implementation
 *
 * Copyright (c) 2009-2012 Jan Seiffert
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
 */


/*
 * This is all a little bit ... sad
 *
 * The scas string instruction is quite fast, but has its problems.
 * Older Intel have a quite high factor (5*n, 7*n), others don't
 * (Via 3*n, AMD 2*n), most have a high constant price (up to 50
 * cycles). This all makes scas problematic. Still, it is _the_
 * instruction for the task.
 *
 * MMX/SSE can be faster, but it is also problematic. You have the
 * memory fetch (but scas has to do the same), the dependend ops
 * (latency stalls, on older > 1 for pcmpeq/pxor), and pmovmsk is
 * good, but still "slow" (often 3 cycles latency), + manual loop
 * overhead.
 *
 * The only thing which safes us is that with MMX/SSE we can look at
 * very large chunks (4/8 times scasl, but the factor gets worse for
 * x86_64...).
 *
 * The SSE41 version is the king of the hill, because ptest has a
 * latency of 1 cycle.
 *
 * Looking at callgrind instruction counts does not help, there scas
 * will always win because callgrind only sees one instruction (times
 * length).
 *
 * Some bulk mesurements on 128k memblocks show the SSE2 version being
 * twice as fast then scas on an Athlon-X2. It's actually a little better
 * than twice as fast, so should beat scasq on x86_64.
 */

#include "x86.h"
#include "x86_features.h"

#define SOV8	8
#define SOV16	16
#define SOV32	32

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
static size_t mem_spn_ff_AVX2(const void *s, size_t len)
{
	const unsigned char *p;
	size_t ret;
	size_t f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"mov	%7, %k2\n\t"
		"vpcmpeqb	%%ymm1, %%ymm1, %%ymm1\n\t"
		"vmovdqa	(%"PTRP"1), %%ymm0\n\t"
		"sub	%k3, %k2\n\t"
		"vpcmpeqb	%%ymm0, %%ymm1, %%ymm0\n\t"
		"vpxor	%%ymm0, %%ymm1, %%ymm0\n\t" /* invert match */
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
		"mov	%4, %0\n\t"
		"jmp	5f\n"
		"8:\n\t"
		"vmovdqa	%%ymm2, %%ymm0\n\t"
		"add	%7, %1\n"
		"10:\n\t"
		"vpcmpeqb	%%ymm0, %%ymm1, %%ymm0\n\t"
		"vpxor	%%ymm0, %%ymm1, %%ymm0\n\t" /* invert match */
		"vpmovmskb	%%ymm0, %k0\n\t"
		"jmp	4f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"prefetcht0 96(%"PTRP"1)\n\t"
		"vmovdqa	(%"PTRP"1), %%ymm0\n\t"
		"vmovdqa	%c7(%"PTRP"1), %%ymm2\n\t"
		"vptest	%%ymm1, %%ymm0\n\t"
		"jnc	10b\n\t"
		"vptest	%%ymm1, %%ymm2\n\t"
		"jnc	8b\n\t"
		"sub	%7*2, %2\n\t"
		"add	%7*2, %1\n"
		"2:\n\t"
		"cmp	%7*2, %2\n\t"
		"jae	1b\n\t"
		"cmp	%7, %2\n\t"
		"jnae	9f\n\t"
		"vmovdqa	(%"PTRP"1), %%ymm0\n\t"
		"vptest	%%ymm1, %%ymm0\n\t"
		"jnc	10b\n\t"
		"sub	%7, %2\n\t"
		"add	%7, %1\n"
		"9:\n\t"
		"cmp	$0, %2\n\t"
		"jle	6b\n\t"
		"vmovdqa	(%"PTRP"1), %%ymm0\n\t"
		"vpcmpeqb	%%ymm0, %%ymm1, %%ymm0\n\t"
		"vpxor	%%ymm0, %%ymm1, %%ymm0\n\t" /* invert match */
		"vpmovmskb	%%ymm0, %k0\n\t"
		"mov	%7, %k3\n\t"
		"sub	%k2, %k3\n"
		"3:\n\t"
		"shl	%b3, %k0\n\t"
		"shr	%b3, %k0\n"
		"jz	6b\n"
		"4:\n\t"
		"tzcnt	%k0, %k0\n\t"
		"sub	%5, %1\n\t"
		"add	%1, %0\n"
		"5:\n\t"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=&c" (f)
	:
#  ifndef __x86_64__
	  /* %4 */ "m" (len),
	  /* %5 */ "m" (s),
#  else
	/* amd64 has enough call clobbered regs not to spill */
	  /* %4 */ "r" (len),
	  /* %5 */ "r" (s),
#  endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV32)),
	  /* %7 */ "i" (SOV32),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV32))
	: "cc"
#  ifdef __AVX__
	, "ymm0", "ymm1", "ymm2"
#  elif defined(__SSE__)
	, "xmm0", "xmm1", "xmm2"
#  endif
	);
	return ret;
}
# endif

# if HAVE_BINUTILS >= 219
/*
 * They made the vptest wider, even beeing a integer instruction, but
 * they prop. saw a the advantage to have an full width test instruction,
 * even with floats....
 */
static size_t mem_spn_ff_AVX(const void *s, size_t len)
{
	const unsigned char *p;
	size_t ret;
	size_t f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"mov	%7, %k2\n\t"
		"vpcmpeqb	%%xmm1, %%xmm1, %%xmm1\n\t"
		"vmovdqa	(%"PTRP"1), %%xmm0\n\t"
		"vperm2f128	$0, %%ymm1, %%ymm1, %%ymm1\n\t"
		"sub	%k3, %k2\n\t"
		"vpcmpeqb	%%xmm0, %%xmm1, %%xmm0\n\t"
		"vpxor	%%xmm0, %%xmm1, %%xmm0\n\t" /* invert match */
		"vpmovmskb	%%xmm0, %k0\n\t"
		"shr	%b3, %k0\n\t"
		"shl	%b3, %k0\n\t"
		"sub	%4, %2\n\t"
		"jg	7f\n\t"
		"test	%k0, %k0\n\t"
		"jnz	4f\n\t"
		"neg	%2\n\t"
		"add	%7, %1\n\t"
		"cmp	%7, %2\n\t"
		"jnae	9f\n\t"
		"test	$((%c7*2)-1), %1\n\t"
		"jz	2f\n\t"
		"vmovdqa	(%"PTRP"1), %%xmm0\n\t"
		"vptest	%%xmm1, %%xmm0\n\t"
		"jnc	10f\n\t"
		"sub	%7, %2\n\t"
		"add	%7, %1\n"
		"jmp	2f\n"
		"7:\n\t"
		"mov	%k2, %k3\n\t"
		"jmp	3f\n"
		"6:\n\t"
		"mov	%4, %0\n\t"
		"jmp	5f\n"
		"8:\n\t"
		"vptest	%%xmm1, %%xmm0\n\t"
		"jnc	10f\n\t"
		"vextractf128	$1, %%ymm0, %%xmm0\n\t"
		"add	%7, %1\n"
		"10:\n\t"
		"vpcmpeqb	%%xmm0, %%xmm1, %%xmm0\n\t"
		"vpxor	%%xmm0, %%xmm1, %%xmm0\n\t" /* invert match */
		"vpmovmskb	%%xmm0, %k0\n\t"
		"jmp	4f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"prefetcht0 96(%"PTRP"1)\n\t"
		"vmovdqa	(%"PTRP"1), %%ymm0\n\t"
		"vptest	%%ymm1, %%ymm0\n\t"
		"jnc	8b\n\t"
		"sub	%7*2, %2\n\t"
		"add	%7*2, %1\n"
		"2:\n\t"
		"cmp	%7*2, %2\n\t"
		"jae	1b\n\t"
		"cmp	%7, %2\n\t"
		"jnae	9f\n\t"
		"vmovdqa	(%"PTRP"1), %%xmm0\n\t"
		"vptest	%%xmm1, %%xmm0\n\t"
		"jnc	10b\n\t"
		"sub	%7, %2\n\t"
		"add	%7, %1\n"
		"9:\n\t"
		"cmp	$0, %2\n\t"
		"jle	6b\n\t"
		"vmovdqa	(%"PTRP"1), %%xmm0\n\t"
		"vpcmpeqb	%%xmm0, %%xmm1, %%xmm0\n\t"
		"vpxor	%%xmm0, %%xmm1, %%xmm0\n\t" /* invert match */
		"vpmovmskb	%%xmm0, %k0\n\t"
		"mov	%7, %k3\n\t"
		"sub	%k2, %k3\n"
		"3:\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n"
		"jz	6b\n"
		"4:\n\t"
		"bsf	%k0, %k0\n\t"
		"sub	%5, %1\n\t"
		"add	%1, %0\n"
		"5:\n\t"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=&c" (f)
	:
#  ifndef __x86_64__
	  /* %4 */ "m" (len),
	  /* %5 */ "m" (s),
#  else
	/* amd64 has enough call clobbered regs not to spill */
	  /* %4 */ "r" (len),
	  /* %5 */ "r" (s),
#  endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV16)),
	  /* %7 */ "i" (SOV16),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV16))
	: "cc"
#  ifdef __SSE2__
	, "xmm0", "xmm1", "xmm2"
#  endif
	);
	return ret;
}
# endif

# if HAVE_BINUTILS >= 218
static size_t mem_spn_ff_SSE41(const void *s, size_t len)
{
	const unsigned char *p;
	size_t ret;
	size_t f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"mov	%7, %k2\n\t"
		"pcmpeqb	%%xmm1, %%xmm1\n\t"
		"movdqa	(%"PTRP"1), %%xmm0\n\t"
		"sub	%k3, %k2\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pxor	%%xmm1, %%xmm0\n\t" /* invert match */
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
		"mov	%4, %0\n\t"
		"jmp	5f\n"
		"8:\n\t"
		"movdqa	%%xmm2, %%xmm0\n\t"
		"add	%7, %1\n"
		"10:\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pxor	%%xmm1, %%xmm0\n\t" /* invert match */
		"pmovmskb	%%xmm0, %k0\n\t"
		"jmp	4f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"prefetcht0 96(%"PTRP"1)\n\t"
		"movdqa	(%"PTRP"1), %%xmm0\n\t"
		"movdqa	%c7(%"PTRP"1), %%xmm2\n\t"
		"ptest	%%xmm1, %%xmm0\n\t"
		"jnc	10b\n\t"
		"ptest	%%xmm1, %%xmm2\n\t"
		"jnc	8b\n\t"
		"sub	%7*2, %2\n\t"
		"add	%7*2, %1\n"
		"2:\n\t"
		"cmp	%7*2, %2\n\t"
		"jae	1b\n\t"
		"cmp	%7, %2\n\t"
		"jnae	9f\n\t"
		"movdqa	(%"PTRP"1), %%xmm0\n\t"
		"ptest	%%xmm1, %%xmm0\n\t"
		"jnc	10b\n\t"
		"sub	%7, %2\n\t"
		"add	%7, %1\n"
		"9:\n\t"
		"cmp	$0, %2\n\t"
		"jle	6b\n\t"
		"movdqa	(%"PTRP"1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pxor	%%xmm1, %%xmm0\n\t" /* invert match */
		"pmovmskb	%%xmm0, %k0\n\t"
		"mov	%7, %k3\n\t"
		"sub	%k2, %k3\n"
		"3:\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n"
		"jz	6b\n"
		"4:\n\t"
		"bsf	%k0, %k0\n\t"
		"sub	%5, %1\n\t"
		"add	%1, %0\n"
		"5:\n\t"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=&c" (f)
	:
#  ifndef __x86_64__
	  /* %4 */ "m" (len),
	  /* %5 */ "m" (s),
#  else
	/* amd64 has enough call clobbered regs not to spill */
	  /* %4 */ "r" (len),
	  /* %5 */ "r" (s),
#  endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV16)),
	  /* %7 */ "i" (SOV16),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV16))
	: "cc"
#  ifdef __SSE2__
	, "xmm0", "xmm1", "xmm2"
#  endif
	);
	return ret;
}
# endif
#endif

static size_t mem_spn_ff_SSE2(const void *s, size_t len)
{
	const unsigned char *p;
	size_t ret;
	size_t f, t;
	asm volatile ("prefetcht0 (%"PTRP"0)" : : "r" (s));

	asm (
		"mov	%7, %k2\n\t"
		"pcmpeqb	%%xmm1, %%xmm1\n\t"
		"movdqa	(%"PTRP"1), %%xmm0\n\t"
		"sub	%k3, %k2\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pxor	%%xmm1, %%xmm0\n\t" /* invert match */
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
		"mov	%4, %0\n\t"
		"jmp	5f\n"
		"8:\n\t"
		"mov	%k3, %k0\n\t"
		"add	%7, %1\n"
		"10:\n\t"
		"xor	$0xffff, %k0\n\t" /* invert match */
		"jmp	4f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"prefetcht0 96(%"PTRP"1)\n\t"
		"movdqa	(%"PTRP"1), %%xmm0\n\t"
		"movdqa	%c7(%"PTRP"1), %%xmm2\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm2\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"pmovmskb	%%xmm2, %k3\n\t"
		"cmp	$0xffff, %k0\n\t"
		"jne	10b\n\t"
		"cmp	$0xffff, %k3\n\t"
		"jne	8b\n\t"
		"sub	%7*2, %2\n\t"
		"add	%7*2, %1\n"
		"2:\n\t"
		"cmp	%7*2, %2\n\t"
		"jae	1b\n\t"
		"cmp	%7, %2\n\t"
		"jnae	9f\n\t"
		"movdqa	(%"PTRP"1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"cmp	$0xffff, %k0\n\t"
		"jne	10b\n\t"
		"sub	%7, %2\n\t"
		"add	%7, %1\n"
		"9:\n\t"
		"cmp	$0, %2\n\t"
		"jle	6b\n\t"
		"movdqa	(%"PTRP"1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pxor	%%xmm1, %%xmm0\n\t" /* invert match */
		"pmovmskb	%%xmm0, %k0\n\t"
		"mov	%7, %k3\n\t"
		"sub	%k2, %k3\n"
		"3:\n\t"
		"shl	%b3, %w0\n\t"
		"shr	%b3, %w0\n"
		"jz	6b\n"
		"4:\n\t"
		"bsf	%k0, %k0\n\t"
		"sub	%5, %1\n\t"
		"add	%1, %0\n"
		"5:\n\t"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=&c" (f)
	:
#ifndef __x86_64__
	  /* %4 */ "m" (len),
	  /* %5 */ "m" (s),
#else
	/* amd64 has enough call clobbered regs not to spill */
	  /* %4 */ "r" (len),
	  /* %5 */ "r" (s),
#endif
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV16)),
	  /* %7 */ "i" (SOV16),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV16))
	: "cc"
#ifdef __SSE2__
	, "xmm0", "xmm1", "xmm2"
#endif
	);
	return ret;
}

#ifndef __x86_64__
static size_t mem_spn_ff_SSE(const void *s, size_t len)
{
	const unsigned char *p;
	size_t ret;
	size_t f, t;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));

	asm (
		"mov	%7, %k2\n\t"
		"pcmpeqb	%%mm1, %%mm1\n\t"
		"movq	(%1), %%mm0\n\t"
		"sub	%k3, %k2\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pxor	%%mm1, %%mm0\n\t" /* invert match */
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
		"mov	%4, %0\n\t"
		"jmp	5f\n"
		"8:\n\t"
		"mov	%k3, %k0\n\t"
		"add	%7, %1\n"
		"10:\n\t"
		"xor	$0xff, %k0\n\t" /* invert match */
		"jmp	4f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"movq	(%1), %%mm0\n\t"
		"movq	%c7(%1), %%mm2\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pcmpeqb	%%mm1, %%mm2\n\t"
		"pmovmskb	%%mm0, %k0\n\t"
		"pmovmskb	%%mm2, %k3\n\t"
		"cmp	$0xff, %k0\n\t"
		"jne	10b\n"
		"cmp	$0xff, %k3\n\t"
		"jne	8b\n"
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
		"cmp	$0xff, %k0\n\t"
		"jne	10b\n"
		"sub	%7, %2\n\t"
		"add	%7, %1\n"
		"9:\n\t"
		"cmp	$0, %2\n\t"
		"jle	6b\n\t"
		"movq	(%1), %%mm0\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pxor	%%mm1, %%mm0\n\t" /* invert match */
		"pmovmskb	%%mm0, %k0\n\t"
		"mov	%7, %k3\n\t"
		"sub	%k2, %k3\n"
		"3:\n\t"
		"shl	%b3, %b0\n\t"
		"shr	%b3, %b0\n"
		"jz	6b\n"
		"4:\n\t"
		"bsf	%k0, %k0\n\t"
		"sub	%5, %1\n\t"
		"add	%1, %0\n"
		"5:\n\t"
	: /* %0 */ "=&a" (ret),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&r" (t),
	  /* %3 */ "=&c" (f)
	: /* %4 */ "m" (len),
	  /* %5 */ "m" (s),
	  /* %6 */ "3" (ALIGN_DOWN_DIFF(s, SOV8)),
	  /* %7 */ "i" (SOV16),
	  /* %8 */ "1" (ALIGN_DOWN(s, SOV8))
	: "cc"
# ifdef __SSE__
	, "mm0", "mm1", "mm2"
# endif
	);
	return ret;
}
#endif

static size_t mem_spn_ff_x86(const void *s, size_t len)
{
	const unsigned char *p = s;
#if 0
	size_t r = ALIGN_DIFF(p, NOST);
// TODO: align or not...
	len -= r;
	for(; r; p++, r--) {
		if(0xff != *p)
			goto OUT;
	}
#endif

	if(len / NOST)
	{
		size_t t;
		nreg_t u;
		asm (
			"repe scas (%"PTRP"0), %3\n\t"
			"je	1f\n\t"
			"sub	%4, %"PTRP"0\n\t"
			"lea	%c4(%2, %1, %c4), %2\n"
			"1:"
			: /* %0 */ "=D" (p),
			  /* %1 */ "=c" (t),
			  /* %2 */ "=r" (len),
			  /* %3 */ "=a" (u)
			: /* %4 */ "i" (NOST),
			  "0" (p),
			  "1" (len / NOST),
			  "2" (len % NOST),
			  "3" (~(nreg_t)0),
			  "m" (*(const nreg_t *)p)
			: "cc"
		);
	}
	if(NOST != SO32)
	{
		if(len >= SO32 && 0xFFFFFFFFu == *(const uint32_t *)p) {
			p   += SO32;
			len -= SO32;
		}
	}
	if(len >= sizeof(uint16_t) && 0xFFFFu == *(const uint16_t *)p) {
		p   += sizeof(uint16_t);
		len -= sizeof(uint16_t);
	}
	if(len && 0xff == *p) {
		p++;
		len--;
	}
#if 0
OUT:
#endif
	return (size_t)(p - (const unsigned char *)s);
}

static __init_cdata const struct test_cpu_feature tfeat_mem_spn_ff[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
	{.func = (void (*)(void))mem_spn_ff_AVX2,  .features = {[4] = CFB(CFEATURE_AVX2)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 219
	{.func = (void (*)(void))mem_spn_ff_AVX,   .features = {[1] = CFB(CFEATURE_AVX)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 218
	{.func = (void (*)(void))mem_spn_ff_SSE41, .features = {[1] = CFB(CFEATURE_SSE4_1)}},
# endif
#endif
	{.func = (void (*)(void))mem_spn_ff_SSE2,  .features = {[0] = CFB(CFEATURE_SSE2)}},
#ifndef __x86_64__
	{.func = (void (*)(void))mem_spn_ff_SSE,   .features = {[0] = CFB(CFEATURE_SSE)}},
	{.func = (void (*)(void))mem_spn_ff_SSE,   .features = {[2] = CFB(CFEATURE_MMXEXT)}},
#endif
	{.func = (void (*)(void))mem_spn_ff_x86,   .features = {}, .flags = CFF_DEFAULT},
};

DYN_JMP_DISPATCH(size_t, mem_spn_ff, (const void *s, size_t len), (s, len))

/*@unused@*/
static char const rcsid_msfx[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
