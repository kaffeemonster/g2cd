/*
 * strlen.c
 * strlen, x86 implementation
 *
 * Copyright (c) 2008-2010 Jan Seiffert
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

#include "x86_features.h"

#define SOV16	16
#define SOV8	8

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 218
static size_t strlen_SSE42(const char *s)
{
	size_t len, t;
	const char *p;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));
	/*
	 * even if nehalem can handle unaligned load much better
	 * (so they promised), we still align hard to get in
	 * swing with the page boundery.
	 */
	asm (
		"pxor	%%xmm0, %%xmm0\n\t"
		"movdqa	(%1), %%xmm1\n\t"
		"pcmpeqb	%%xmm0, %%xmm1\n\t"
		"pmovmskb	%%xmm1, %0\n\t"
		"shr	%b2, %0\n\t"
		"bsf	%0, %0\n\t"
		"jnz	2f\n\t"
		"mov	$0xFF01, %k0\n\t"
		"movd	%k0, %%xmm0\n\t"
		".p2align 1\n"
		"1:\n\t"
		"add	$16, %1\n\t"
		"prefetcht0	64(%1)\n\t"
		/* LSB,Invert,Range,Bytes */
		/*             6543210 */
		"pcmpistri	$0b0010100, (%1), %%xmm0\n\t"
		"jnz	1b\n\t"
		"lea	(%1,%2),%0\n\t"
		"sub	%3, %0\n"
		"2:"
		: /* %0 */ "=&r" (len),
		  /* %1 */ "=&r" (p),
		  /* %2 */ "=&c" (t)
#  ifdef __i386__
		: /* %3 */ "m" (s),
#  else
		: /* %3 */ "r" (s),
#  endif
		  /* %4 */ "2" (ALIGN_DOWN_DIFF(s, SOV16)),
		  /* %5 */ "1" (ALIGN_DOWN(s, SOV16))
#  ifdef __SSE2__
		: "xmm0", "xmm1"
#  endif
	);
	return len;
}
# endif
#endif

static size_t strlen_SSE2(const char *s)
{
	size_t len;
	const char *p;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));
	asm (
		"pxor	%%xmm1, %%xmm1\n\t"
		"movdqa	(%1), %%xmm0\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"jnz	2f\n\t"
		".p2align 1\n"
		"1:\n\t"
		"movdqa	16(%1), %%xmm0\n\t"
		"add	$16, %1\n\t"
		"prefetcht0	64(%1)\n\t"
		"pcmpeqb	%%xmm1, %%xmm0\n\t"
		"pmovmskb	%%xmm0, %0\n\t"
		"test	%0, %0\n\t"
		"jz	1b\n\t"
		"bsf	%0, %0\n\t"
		"add	%1, %0\n\t"
		"sub	%2, %0\n\t"
		"2:"
		: /* %0 */ "=&r" (len),
		  /* %1 */ "=&r" (p)
#ifdef __i386__
		: /* %2 */ "m" (s),
#else
		: /* %2 */ "r" (s),
#endif
		  /* %3 */ "c" (ALIGN_DOWN_DIFF(s, SOV16)),
		  /* %4 */ "1" (ALIGN_DOWN(s, SOV16))
#ifdef __SSE2__
		: "xmm0", "xmm1"
#endif
	);
	return len;
}

#ifndef __x86_64__
static size_t strlen_SSE(const char *s)
{
	size_t len;
	const char *p;
	asm volatile ("prefetcht0 (%0)" : : "r" (s));
	asm (
		"pxor	%%mm1, %%mm1\n\t"
		"movq	(%1), %%mm0\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pmovmskb	%%mm0, %0\n\t"
		"shr	%b3, %0\n\t"
		"bsf	%0, %0\n\t"
		"jnz	2f\n\t"
		".p2align 1\n"
		"1:\n\t"
		"movq	8(%1), %%mm0\n\t"
		"add	$8, %1\n\t"
		"prefetcht0	64(%1)\n\t"
		"pcmpeqb	%%mm1, %%mm0\n\t"
		"pmovmskb	%%mm0, %0\n\t"
		"test	%0, %0\n\t"
		"jz	1b\n\t"
		"bsf	%0, %0\n\t"
		"add	%1, %0\n\t"
		"sub	%2, %0\n\t"
		"2:\n\t"
		: /* %0 */ "=&r" (len),
		  /* %1 */ "=r" (p)
		: /* %2 */ "m" (s),
		  /* %3 */ "c" (ALIGN_DOWN_DIFF(s, SOV8)),
		  /* %4 */ "1" (ALIGN_DOWN(s, SOV8))
#ifdef __SSE__
		: "mm0", "mm1"
#endif
	);
	return len;
}
#endif

static size_t strlen_x86(const char *s)
{
	const char *p;
	size_t t, len;
	asm (
#ifndef __x86_64__
		"push	%2\n\t"
#endif
		"mov	(%1), %2\n\t"
#ifndef __x86_64__
		"lea	-0x1010101(%2),%0\n\t"
#else
		"mov	%2, %0\n\t"
		"add	%8, %0\n\t"
#endif
		"not	%2\n\t"
		"and	%2, %0\n\t"
#ifndef __x86_64__
		"pop	%2\n\t"
#endif
		"and	%7, %0\n\t"
		"shr	%b5, %0\n\t"
		"shl	%b5, %0\n\t"
		"test	%0, %0\n\t"
		"jnz	2f\n\t"
		".p2align 1\n"
		"1:\n\t"
		"add	%4, %1\n\t"
		"mov	(%1), %2\n\t"
#ifndef __x86_64__
		"lea	-0x1010101(%2),%0\n\t"
#else
		"mov	%2, %0\n\t"
		"add	%8, %0\n\t"
#endif
		"not	%2\n\t"
		"and	%2, %0\n\t"
		"and	%7, %0\n\t"
		"jz	1b\n"
		"2:\n\t"
		"bsf	%0, %0\n\t"
		"shr	$3, %0\n\t"
		"add	%1, %0\n\t"
		"sub	%6, %0"
	: /* %0 */ "=&a" (len),
	  /* %1 */ "=&r" (p),
#ifndef __x86_64__
	  /* %2 */ "=c" (t)
#else
	  /* %2 */ "=&r" (t)
#endif
	: /* %3 */ "1" (ALIGN_DOWN(s, SOST)),
	  /* %4 */ "K" (SOST),
#ifndef __x86_64__
	  /* %5 */ "2" (ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR),
	  /*
	   * 386 should keep it on stack to prevent
	   * register spill and a frame
	   */
	  /* %6 */ "m" (s),
	  /* %7 */ "e" (0x80808080)
#else
	  /*
	   * amd64 has more register and a regcall abi
	   * so keep in reg.
	   */
	  /* %5 */ "c" (ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR),
	  /* %6 */ "r" (s),
	  /* %7 */ "r" ( 0x8080808080808080LL),
	  /* %8 */ "r" (-0x0101010101010101LL)
#endif
	);
	return len;
}


static const struct test_cpu_feature t_feat[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 218
	{.func = (void (*)(void))strlen_SSE42, .flags_needed = CFEATURE_SSE4_2, .callback = NULL},
# endif
#endif
	{.func = (void (*)(void))strlen_SSE2, .flags_needed = CFEATURE_SSE2, .callback = NULL},
#ifndef __x86_64__
	{.func = (void (*)(void))strlen_SSE, .flags_needed = CFEATURE_SSE, .callback = NULL},
	{.func = (void (*)(void))strlen_SSE, .flags_needed = CFEATURE_MMXEXT, .callback = NULL},
#endif
	{.func = (void (*)(void))strlen_x86, .flags_needed = -1, .callback = NULL},
};

#if 1
static size_t strlen_runtime_sw(const char *s);
/*
 * Func ptr
 */
static size_t (*strlen_ptr)(const char *s) = strlen_runtime_sw;

/*
 * constructor
 */
static GCC_ATTR_CONSTRUCT void strlen_select(void)
{
	strlen_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructor fails
 */
static size_t strlen_runtime_sw(const char *s)
{
	strlen_select();
	return strlen_ptr(s);
}

size_t strlen(const char *s)
{
	return strlen_ptr(s);
}
#else
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

static char const rcsid_sl[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
