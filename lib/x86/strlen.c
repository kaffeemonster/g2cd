/*
 * strlen.c
 * strlen, x86 implementation
 *
 * Copyright (c) 2008-2012 Jan Seiffert
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

#include "x86.h"
#include "x86_features.h"

#define SOV32	32
#define SOV16	16
#define SOV8	8

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
static size_t strlen_AVX2(const char *s)
{
	size_t len, t;
	const char *p;

	asm (
#  ifdef __i386__
		"mov	%3, %1\n\t"
		"prefetcht0	(%1)\n\t"
		"mov	%1, %2\n\t"
#  else
		"prefetcht0	(%"PTRP"3)\n\t"
		"mov	%3, %1\n\t"
		"mov	%k3, %k2\n\t"
#  endif
		"and	$-32, %1\n\t"
		"vpxor	%%ymm4, %%ymm4, %%ymm4\n\t"
		"xor	%k1, %k2\n\t"
		"jz	6f\n\t"
		"vpcmpeqb	(%"PTRP"1), %%ymm4, %%ymm1\n\t"
		"vpmovmskb	%%ymm1, %k0\n\t"
		"shr	%b2, %k0\n\t"
		"shl	%b2, %k0\n\t"
		"jnz	2f\n\t"
		".p2align 2\n"
		"1:\t"
		"lea	128(%1), %1\n\t"
		"vpcmpeqb	-96(%"PTRP"1), %%ymm4, %%ymm0\n\t"
		"vptest	%%ymm0, %%ymm4\n\t"
		"jnc	5f\n\t"
		"vpcmpeqb	-64(%"PTRP"1), %%ymm4, %%ymm1\n\t"
		"vptest	%%ymm1, %%ymm4\n\t"
		"jnc	4f\n\t"
		"vpcmpeqb	-32(%"PTRP"1), %%ymm4, %%ymm2\n\t"
		"vptest	%%ymm2, %%ymm4\n\t"
		"jnc	3f\n"
		"6:\n\t"
		"vpcmpeqb	(%"PTRP"1), %%ymm4, %%ymm3\n\t"
		"vptest	%%ymm3, %%ymm4\n\t"
		"jc	1b\n\t"
		"vpmovmskb	%%ymm3, %k0\n\t"
		"jmp	2f\n\t"
		"5:\t"
		"vpmovmskb	%%ymm0, %k0\n\t"
		"sub	$96, %1\n"
		"jmp	2f\n\t"
		"4:\t"
		"vpmovmskb	%%ymm1, %k0\n\t"
		"sub	$64, %1\n"
		"jmp	2f\n\t"
		"3:\t"
		"vpmovmskb	%%ymm2, %k0\n\t"
		"sub	$32, %1\n"
		"2:\t"
		"tzcnt	%k0, %k0\n\t"
		"add	%1, %0\n\t"
		"sub	%3, %0\n\t"
		: /* %0 */ "=&r" (len),
		  /* %1 */ "=&r" (p),
		  /* %2 */ "=&c" (t)
#  ifdef __i386__
		: /* %3 */ "m" (s)
#  else
		: /* %3 */ "r" (s)
#  endif
#  ifdef __AVX__
		: "ymm0", "ymm1", "ymm2", "ymm3", "ymm4"
#  elif defined(__SSE__)
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4"
#  endif
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
static size_t strlen_AVX(const char *s)
{
	size_t len, t;
	const char *p;
	/*
	 * even if nehalem can handle unaligned load much better
	 * (so they promised), we still align hard to get in
	 * swing with the page boundery.
	 */
	asm (
#ifdef __i386__
		"mov	%3, %1\n\t"
		"prefetcht0	(%1)\n\t"
		"mov	%1, %2\n\t"
#else
		"prefetcht0	(%"PTRP"3)\n\t"
		"mov	%3, %1\n\t"
		"mov	%k3, %k2\n\t"
#endif
		"and	$-16, %1\n\t"
		"vpxor	%%xmm0, %%xmm0, %%xmm0\n\t"
		"xor	%k1, %k2\n\t"
		"jz	7f\n\t"
		"vpcmpeqb	(%"PTRP"1), %%xmm0, %%xmm1\n\t"
		"vpmovmskb	%%xmm1, %k0\n\t"
		"shr	%b2, %k0\n\t"
		"jz	1f\n\t"
		"shl	%b2, %k0\n\t"
		/*
		 * make the bsf a tzcount
		 * 0xf3 is a rep prefix byte, which is ignored on older CPU,
		 * so it will execute bsf. Both instructions are compatible,
		 * except when arg is 0 (here not possible) and in flags.
		 * CPU with BMI will execute tzcount, which is, funniely, faster.
		 * hardcoded to not need any assembler support
		 */
		".byte	0xf3\n\t"
		"bsf	%k0, %k2\n\t"
		"jmp	3f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"add	$64, %1\n\t"
		/* LSB,Norm,EqEach,Bytes */
		/*             6543210 */
		"vpcmpistri	$0b0001000, -48(%"PTRP"1), %%xmm0\n\t"
		"jz	6f\n\t"
		"vpcmpistri	$0b0001000, -32(%"PTRP"1), %%xmm0\n\t"
		"jz	5f\n\t"
		"vpcmpistri	$0b0001000, -16(%"PTRP"1), %%xmm0\n\t"
		"jz	4f\n"
		"7:\n\t"
		"vpcmpistri	$0b0001000, (%"PTRP"1), %%xmm0\n\t"
		"jnz	1b\n\t"
		"jmp	3f\n\t"
		"6:\n"
		"sub	$16, %k2\n"
		"5:\n"
		"sub	$16, %k2\n"
		"4:\n"
		"sub	$16, %k2\n"
		"3:\n\t"
		"lea	(%1,%2),%0\n\t"
		"sub	%3, %0\n"
		: /* %0 */ "=&r" (len),
		  /* %1 */ "=&r" (p),
		  /* %2 */ "=&c" (t)
#  ifdef __i386__
		: /* %3 */ "m" (s)
#  else
		: /* %3 */ "r" (s)
#  endif
#  ifdef __SSE__
		: "xmm0", "xmm1"
#  endif
	);
	return len;
}
# endif

# if HAVE_BINUTILS >= 218
static size_t strlen_SSE42(const char *s)
{
	size_t len, t;
	const char *p;
	/*
	 * even if nehalem can handle unaligned load much better
	 * (so they promised), we still align hard to get in
	 * swing with the page boundery.
	 */
	asm (
#ifdef __i386__
		"mov	%3, %1\n\t"
		"prefetcht0	(%1)\n\t"
		"mov	%1, %2\n\t"
#else
		"prefetcht0	(%"PTRP"3)\n\t"
		"mov	%3, %1\n\t"
		"mov	%k3, %k2\n\t"
#endif
		"and	$-16, %1\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"xor	%k1, %k2\n\t"
		"jz	7f\n\t"
		"movdqa	(%"PTRP"1), %%xmm1\n\t"
		"pcmpeqb	%%xmm0, %%xmm1\n\t"
		"pmovmskb	%%xmm1, %k0\n\t"
		"shr	%b2, %k0\n\t"
		"jz	1f\n\t"
		"shl	%b2, %k0\n\t"
		/*
		 * make the bsf a tzcount
		 * 0xf3 is a rep prefix byte, which is ignored on older CPU,
		 * so it will execute bsf. Both instructions are compatible,
		 * except when arg is 0 (here not possible) and in flags.
		 * CPU with BMI will execute tzcount, which is, funniely, faster.
		 * hardcoded to not need any assembler support
		 */
		".byte	0xf3\n\t"
		"bsf	%k0, %k2\n\t"
		"jmp	3f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"add	$64, %1\n\t"
		/* LSB,Norm,EqEach,Bytes */
		/*             6543210 */
		"pcmpistri	$0b0001000, -48(%"PTRP"1), %%xmm0\n\t"
		"jz	6f\n\t"
		"pcmpistri	$0b0001000, -32(%"PTRP"1), %%xmm0\n\t"
		"jz	5f\n\t"
		"pcmpistri	$0b0001000, -16(%"PTRP"1), %%xmm0\n\t"
		"jz	4f\n"
		"7:\n\t"
		"pcmpistri	$0b0001000, (%"PTRP"1), %%xmm0\n\t"
		"jnz	1b\n\t"
		"jmp	3f\n"
		"6:\n"
		"sub	$16, %k2\n"
		"5:\n"
		"sub	$16, %k2\n"
		"4:\n"
		"sub	$16, %k2\n"
		"3:\n\t"
		"lea	(%1,%2),%0\n\t"
		"sub	%3, %0\n"
		: /* %0 */ "=&r" (len),
		  /* %1 */ "=&r" (p),
		  /* %2 */ "=&c" (t)
#  ifdef __i386__
		: /* %3 */ "m" (s)
#  else
		: /* %3 */ "r" (s)
#  endif
#  ifdef __SSE__
		: "xmm0", "xmm1"
#  endif
	);
	return len;
}

static size_t strlen_SSE41(const char *s)
{
	size_t len, t;
	const char *p;

	asm (
#ifdef __i386__
		"mov	%3, %1\n\t"
		"prefetcht0	(%1)\n\t"
		"mov	%1, %2\n\t"
#else
		"prefetcht0	(%"PTRP"3)\n\t"
		"mov	%3, %1\n\t"
		"mov	%k3, %k2\n\t"
#endif
		"and	$-16, %1\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"pxor	%%xmm1, %%xmm1\n\t"
		"pxor	%%xmm2, %%xmm2\n\t"
		"pxor	%%xmm3, %%xmm3\n\t"
		"xor	%k1, %k2\n\t"
		"jz	6f\n\t"
		"pcmpeqb	(%"PTRP"1), %%xmm2\n\t"
		"pmovmskb	%%xmm2, %k0\n\t"
		"shr	%b2, %k0\n\t"
		"shl	%b2, %k0\n\t"
		"jnz	2f\n\t"
		"pxor	%%xmm2, %%xmm2\n\t"
		".p2align 2\n"
		"1:\t"
		"add	$64, %1\n\t"
		"pcmpeqb	-48(%"PTRP"1), %%xmm0\n\t"
		"ptest	%%xmm0, %%xmm1\n\t"
		"jnc	5f\n\t"
		"pcmpeqb	-32(%"PTRP"1), %%xmm1\n\t"
		"ptest	%%xmm1, %%xmm2\n\t"
		"jnc	4f\n\t"
		"pcmpeqb	-16(%"PTRP"1), %%xmm2\n\t"
		"ptest	%%xmm2, %%xmm3\n\t"
		"jnc	3f\n"
		"6:\n\t"
		"pcmpeqb	(%"PTRP"1), %%xmm3\n\t"
		"ptest	%%xmm3, %%xmm0\n\t"
		"jc	1b\n\t"
		"pmovmskb	%%xmm3, %k0\n\t"
		"jmp	2f\n\t"
		"5:\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"sub	$48, %1\n"
		"jmp	2f\n\t"
		"4:\t"
		"pmovmskb	%%xmm1, %k0\n\t"
		"sub	$32, %1\n"
		"jmp	2f\n\t"
		"3:\t"
		"pmovmskb	%%xmm2, %k0\n\t"
		"sub	$16, %1\n"
		"2:\t"
		/*
		 * make the bsf a tzcount
		 * 0xf3 is a rep prefix byte, which is ignored on older CPU,
		 * so it will execute bsf. Both instructions are compatible,
		 * except when arg is 0 (here not possible) and in flags.
		 * CPU with BMI will execute tzcount, which is, funniely, faster.
		 * hardcoded to not need any assembler support
		 */
		".byte 0xf3\n\t"
		"bsf	%k0, %k0\n\t"
		"add	%1, %0\n\t"
		"sub	%3, %0\n\t"
		: /* %0 */ "=&r" (len),
		  /* %1 */ "=&r" (p),
		  /* %2 */ "=&c" (t)
#ifdef __i386__
		: /* %3 */ "m" (s)
#else
		: /* %3 */ "r" (s)
#endif
#ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3"
#endif
	);
	return len;
}
# endif
#endif

static size_t strlen_SSE2(const char *s)
{
	size_t len, t;
	const char *p;

	asm (
#ifdef __i386__
		"mov	%3, %1\n\t"
		"prefetcht0	(%1)\n\t"
		"mov	%1, %2\n\t"
#else
		"prefetcht0	(%"PTRP"3)\n\t"
		"mov	%3, %1\n\t"
		"mov	%k3, %k2\n\t"
#endif
		"and	$-16, %1\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"pxor	%%xmm1, %%xmm1\n\t"
		"pxor	%%xmm2, %%xmm2\n\t"
		"pxor	%%xmm3, %%xmm3\n\t"
		"xor	%k1, %k2\n\t"
		"jz	6f\n\t"
		"pcmpeqb	(%"PTRP"1), %%xmm2\n\t"
		"pmovmskb	%%xmm2, %k0\n\t"
		"shr	%b2, %k0\n\t"
		"shl	%b2, %k0\n\t"
		"jnz	2f\n\t"
		"pxor	%%xmm2, %%xmm2\n\t"
		".p2align 2\n"
		"1:\t"
		"add	$64, %1\n\t"
		"pcmpeqb	-48(%"PTRP"1), %%xmm0\n\t"
		"pmovmskb	%%xmm0, %k0\n\t"
		"test	%k0, %k0\n\t"
		"jnz	5f\n\t"
		"pcmpeqb	-32(%"PTRP"1), %%xmm1\n\t"
		"pmovmskb	%%xmm1, %k0\n\t"
		"test	%k0, %k0\n\t"
		"jnz	4f\n\t"
		"pcmpeqb	-16(%"PTRP"1), %%xmm2\n\t"
		"pmovmskb	%%xmm2, %k0\n\t"
		"test	%k0, %k0\n\t"
		"jnz	3f\n"
		"6:\n\t"
		"pcmpeqb	(%"PTRP"1), %%xmm3\n\t"
		"pmovmskb	%%xmm3, %k0\n\t"
		"test	%k0, %k0\n\t"
		"jz	1b\n\t"
		"jmp	2f\n\t"
		"5:\t"
		"sub	$16, %1\n"
		"4:\t"
		"sub	$16, %1\n"
		"3:\t"
		"sub	$16, %1\n"
		"2:\t"
		"bsf	%k0, %k0\n\t"
		"add	%1, %0\n\t"
		"sub	%3, %0\n\t"
		: /* %0 */ "=&r" (len),
		  /* %1 */ "=&r" (p),
		  /* %2 */ "=&c" (t)
#ifdef __i386__
		: /* %3 */ "m" (s)
#else
		: /* %3 */ "r" (s)
#endif
#ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3"
#endif
	);
	return len;
}

#ifdef __i386__
static size_t strlen_SSE(const char *s)
{
	size_t len, t;
	const char *p;

	asm (
		"mov	%3, %1\n\t"
		"prefetcht0	(%1)\n\t"
		"mov	%1, %2\n\t"
		"and	$-8, %1\n\t"
		"pxor	%%mm0, %%mm0\n\t"
		"pxor	%%mm1, %%mm1\n\t"
		"pxor	%%mm2, %%mm2\n\t"
		"pxor	%%mm3, %%mm3\n\t"
		"xor	%1, %2\n\t"
		"jz	6f\n\t"
		"pcmpeqb	(%1), %%mm2\n\t"
		"pmovmskb	%%mm2, %0\n\t"
		"shr	%b2, %0\n\t"
		"shl	%b2, %0\n\t"
		"jnz	2f\n\t"
		"pxor	%%mm2, %%mm2\n\t"
		".p2align 2\n"
		"1:\t"
		"add	$32, %1\n\t"
		"pcmpeqb	-24(%1), %%mm0\n\t"
		"pmovmskb	%%mm0, %0\n\t"
		"test	%0, %0\n\t"
		"jnz	5f\n\t"
		"pcmpeqb	-16(%1), %%mm1\n\t"
		"pmovmskb	%%mm1, %0\n\t"
		"test	%0, %0\n\t"
		"jnz	4f\n\t"
		"pcmpeqb	-8(%1), %%mm2\n\t"
		"pmovmskb	%%mm2, %0\n\t"
		"test	%0, %0\n\t"
		"jnz	3f\n"
		"6:\n\t"
		"pcmpeqb	(%1), %%mm3\n\t"
		"pmovmskb	%%mm3, %0\n\t"
		"test	%0, %0\n\t"
		"jz	1b\n\t"
		"jmp	2f\n\t"
		"5:\t"
		"sub	$8, %1\n"
		"4:\t"
		"sub	$8, %1\n"
		"3:\t"
		"sub	$8, %1\n"
		"2:\t"
		"bsf	%0, %0\n\t"
		"add	%1, %0\n\t"
		"sub	%3, %0\n\t"
		: /* %0 */ "=&r" (len),
		  /* %1 */ "=&r" (p),
		  /* %2 */ "=&c" (t)
		: /* %3 */ "m" (s)
#ifdef __MMX__
		: "mm0", "mm1", "mm2", "mm3"
#endif
	);

	return len;
}
#endif

static size_t strlen_x86(const char *s)
{
	const char *p;
	nreg_t t, len;
	asm (
		"mov	(%"PTRP"1), %2\n\t"
#ifdef __i386__
		"lea	-0x1010101(%2), %0\n\t"
#else
		"lea	(%2, %7), %0\n\t"
#endif
		"not	%2\n\t"
		"and	%2, %0\n\t"
#ifdef __i386__
		"mov	%5, %2\n\t"
#else
		"mov	%k5, %k2\n\t"
#endif
		"xor	%k1, %k2\n\t"
		"shl	$3, %2\n\t"
		"and	%6, %0\n\t"
		"shr	%b2, %0\n\t"
		"shl	%b2, %0\n\t"
		"test	%0, %0\n\t"
		"jnz	2f\n\t"
		".p2align 2\n"
		"1:\n\t"
		"add	%4, %1\n\t"
		"mov	(%"PTRP"1), %2\n\t"
#ifdef __i386__
		"lea	-0x1010101(%2), %0\n\t"
#else
		"lea	(%2, %7), %0\n\t"
#endif
		"not	%2\n\t"
		"and	%2, %0\n\t"
		"and	%6, %0\n\t"
		"jz	1b\n"
		"2:\n\t"
		"lea	-1(%0), %2\n\t"
		"not	%0\n\t"
		"and	%0, %2\n\t"
		"shr	$7, %2\n\t"
#ifdef __i386__
		"lea	0xff4000(%2), %0\n\t"
		"sar	$23, %0\n\t"
		"and	%2, %0\n\t"
#else
		"mov	$0x0001020304050608, %0\n\t"
		"imul	%2, %0\n\t"
		"shr	$56, %0\n\t"
#endif
		"add	%"PTRP"1, %0\n\t"
		"sub	%"PTRP"5, %0"
	: /* %0 */ "=&a" (len),
	  /* %1 */ "=&r" (p),
	  /* %2 */ "=&c" (t)
	: /* %3 */ "1" (ALIGN_DOWN(s, NOST)),
	  /* %4 */ "K" (NOST),
#ifdef __i386__
	  /*
	   * 386 should keep it on stack to prevent
	   * register spill and a frame
	   */
	  /* %5 */ "m" (s),
	  /* %6 */ "e" (0x80808080)
#else
	  /*
	   * amd64 has more register and a regcall abi
	   * so keep in reg.
	   */
	  /* %5 */ "r" (s),
	  /* we can't encode very long constants so put in reg */
	  /* %6 */ "r" ( 0x8080808080808080LL),
	  /* %7 */ "r" (-0x0101010101010101LL)
#endif
	);
	return len;
}


static __init_cdata const struct test_cpu_feature tfeat_strlen[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
	{.func = (void (*)(void))strlen_AVX2,  .features = {[4] = CFB(CFEATURE_AVX2)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 219
	{.func = (void (*)(void))strlen_AVX,   .features = {[1] = CFB(CFEATURE_AVX)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 218
	{.func = (void (*)(void))strlen_SSE42, .features = {[1] = CFB(CFEATURE_SSE4_2)}},
	{.func = (void (*)(void))strlen_SSE41, .features = {[1] = CFB(CFEATURE_SSE4_1)}},
# endif
#endif
	{.func = (void (*)(void))strlen_SSE2,  .features = {[0] = CFB(CFEATURE_SSE2)}},
#ifdef __i386__
	{.func = (void (*)(void))strlen_SSE,   .features = {[0] = CFB(CFEATURE_SSE)}},
	{.func = (void (*)(void))strlen_SSE,   .features = {[2] = CFB(CFEATURE_MMXEXT)}},
#endif
	{.func = (void (*)(void))strlen_x86,   .features = {}, .flags = CFF_DEFAULT},
};

DYN_JMP_DISPATCH(size_t, strlen, (const char *s), (s))

#if 0
/*
 * newer glibc(2.11)/binutils(2.20) now know "ifuncs",
 * functions which are re-linked to a specific impl. based on
 * a "test" function. It is meant to use the default dynamic link
 * interface (plt, got), already used by the dynamic loader only
 * with a small extention. This way one can do all sorts of foo-bar
 * but mainly the intetion was to overcome the "has cpu feat. X?"
 * at runtime "efficient".
 * The problem for us is since we already have it in an NIH way:
 * - it is only evaluated once.
 *   This makes recursion in our cpu detection painfull. Esp.
 *   since we can not really prevent it (when the compiler decides
 *   to change something for a memcpy or strlen)
 * - is it thread safe?
 *   We specificly try to run the pointer change at constructor
 *   time (before main) to get the functions in place before
 *   something funky can happen.
 * - support detection is a pain.
 *   There are several things here which needed support:
 *   1) inline asm
 *   2) redefining func names by asm
 *   3) working file scope asm
 *   4) binutils support for %gnu_indirect_function
 *   5) runtime support (can not test on cross compile)
 *   To make matters worse, there are some bugs in corner cases
 *   like static linking (which we are basically doing, some "magic"
 *   involved to update our func to plt, in linker/etc), other
 *   linker (gold, which is the new thing for LTO, clang?), and
 *   whatnot.
 * The GCC folks are working on an function attribute "ifunc", but
 * how it might look and when it will come (4.6? 4.7?) and if it
 * will "catch" the worst bugs...
 * Some tests with program wide optimisations inlined the base func
 * nicely into all caller, because it is so cheap. You basically got
 * an "call *0xcafebabe" on all callsites, directly reading the
 * func_ptr. The plt foo would be bogus in this case, this is much
 * nicer (except when lto/whopr lowers plt...). But, we still may
 * want to put the ptr into the got and/or write potect it.
 *
 * So for reference:
 */
extern void *strlen_ifunc (void) __asm__ ("strlen");
void *strlen_ifunc (void)
{
	return test_cpu_feature(t_feat, anum(t_feat));
}
__asm__ (".type strlen, %gnu_indirect_function");
#endif

/*@unused@*/
static char const rcsid_sl[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
