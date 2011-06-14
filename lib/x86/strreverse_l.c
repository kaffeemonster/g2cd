/*
 * strreverse_l.c
 * strreverse_l, x86 implementation
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

#define ARCH_NAME_SUFFIX _generic
#define ONLY_REMAINDER
#include "../generic/strreverse_l.c"

#include "x86_features.h"

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
static void strreverse_l_AVX2(char *begin, char *end);
# endif
# if HAVE_BINUTILS >= 217
static void strreverse_l_SSSE3(char *begin, char *end);
static void strreverse_l_SSE3(char *begin, char *end);
# endif
#endif
static void strreverse_l_SSE2(char *begin, char *end);
#ifndef __x86_64__
static void strreverse_l_SSE(char *begin, char *end);
#endif

static const struct {
	unsigned char d[16];
} vals GCC_ATTR_ALIGNED(16) =
{
	/* 1    2    3    4    5    6    7    8    9   10   11   12   13   14   15   16 */
	{0x0f,0x0e,0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00},
};

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
static void strreverse_l_AVX2(char *begin, char *end)
{
	char *t;

	asm (
#  ifndef __x86_64__
			"lea	-15(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jae	4f\n"
			"vmovdqa	%3, %%xmm4\n\t"
#  endif
			"lea	-31(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jae	2f\n\t"
#  ifndef __x86_64__
			"vmovdqa	%3, %%xmm4\n\t"
#  endif
			"lea	-63(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jae	3f\n\t"
			"vperm2i128	$0, %%ymm4, %%ymm4, %%ymm4\n\t"
			".p2align 2\n"
			"1:\n\t"
			"vlddqu	-31(%1), %%ymm0\n\t"       /* fetch input data */
			"vlddqu	(%0), %%ymm2\n\t"
			/* swab endianess */
			"vpshufb	%%ymm4, %%ymm0, %%ymm0\n\t"
			"vpshufb	%%ymm4, %%ymm2, %%ymm2\n\t"
			"vperm2i128	$1, %%ymm0, %%ymm0, %%ymm0\n\t"
			"vperm2i128	$1, %%ymm2, %%ymm2, %%ymm2\n\t"
			"vmovdqu	%%ymm0, (%0)\n\t"
			"add	$32, %0\n\t"
			"vmovdqu	%%ymm2, -31(%1)\n\t"
			"sub	$32, %1\n\t"
			"lea	-63(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jb	1b\n"
			"lea	-31(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jae	5f\n"
			"3:\n\t"
			"vlddqu	-15(%1), %%xmm0\n\t"       /* fetch input data */
			"vlddqu	(%0), %%xmm2\n\t"
			/* swab endianess */
			"vpshufb	%%ymm4, %%ymm0, %%ymm0\n\t"
			"vpshufb	%%ymm4, %%ymm2, %%ymm2\n\t"
			"vmovdqu	%%xmm0, (%0)\n\t"
			"add	$16, %0\n\t"
			"vmovdqu	%%xmm2, -15(%1)\n\t"
			"sub	$16, %1\n"
			"5:\n\t"
#  ifndef __x86_64__
			"lea	-15(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jae	4f\n"
#  endif
			"2:\n\t"
#  ifndef __x86_64__
			"vmovq	-7(%1), %%xmm0\n\t"       /* fetch input data */
			"vmovq	(%0), %%xmm2\n\t"
			/* swab mask */
			"vpshufd	$0x4E, %%xmm4, %%xmm4\n\t"
			/* swab endianess */
			"vpshufb	%%xmm4, %%xmm0, %%xmm0\n\t"
			"vpshufb	%%xmm4, %%xmm2, %%xmm2\n\t"
			"vmovq	%%xmm0, (%0)\n\t"
			"add	$8, %0\n\t"
			"vmovq	%%xmm2, -7(%1)\n\t"
			"sub	$8, %1\n\t"
			"4:"
#  endif
		: /* %0 */ "=r" (begin),
		  /* %1 */ "=r" (end),
		  /* %2 */ "=r" (t)
		: /* %3 */ "m" (vals),
		  /*    */ "0" (begin),
		  /*    */ "1" (end)
#  ifdef __AVX__
		: "ymm0", "ymm1", "ymm2", "ymm3", "ymm4"
#  elif defined(__SSE__)
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4"
#  endif
	);
	strreverse_l_generic(begin, end);
}
# endif

# if HAVE_BINUTILS >= 218
static void strreverse_l_SSSE3(char *begin, char *end)
{
	char *t;

	asm (
#  ifndef __x86_64__
			"lea	-15(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jae	4f\n"
			"movdqa	%3, %%xmm4\n\t"
#  endif
			"lea	-31(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jae	2f\n\t"
#  ifdef __x86_64__
			"movdqa	%3, %%xmm4\n\t"
#  endif
			".p2align 2\n"
			"1:\n\t"
			"lddqu	-15(%1), %%xmm0\n\t"       /* fetch input data */
			"lddqu	(%0), %%xmm2\n\t"
			/* swab endianess */
			"pshufb	%%xmm4, %%xmm0\n\t"
			"pshufb	%%xmm4, %%xmm2\n\t"
			"movdqu	%%xmm0, (%0)\n\t"
			"add	$16, %0\n\t"
			"movdqu	%%xmm2, -15(%1)\n\t"
			"sub	$16, %1\n\t"
			"lea	-31(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jb	1b\n"
#  ifndef __x86_64__
			"lea	-15(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jae	4f\n"
#  endif
			"2:\n\t"
#  ifndef __x86_64__
			"movq	-7(%1), %%xmm0\n\t"       /* fetch input data */
			"movq	(%0), %%xmm2\n\t"
			/* swab mask */
			"pshufd	$0x4E, %%xmm4, %%xmm4\n\t"
			/* swab endianess */
			"pshufb	%%xmm4, %%xmm0\n\t"
			"pshufb	%%xmm4, %%xmm2\n\t"
			"movq	%%xmm0, (%0)\n\t"
			"add	$8, %0\n\t"
			"movq	%%xmm2, -7(%1)\n\t"
			"sub	$8, %1\n\t"
			"4:"
#  endif
		: /* %0 */ "=r" (begin),
		  /* %1 */ "=r" (end),
		  /* %2 */ "=r" (t)
		: /* %3 */ "m" (vals),
		  /*    */ "0" (begin),
		  /*    */ "1" (end)
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4"
#  endif
	);
	strreverse_l_generic(begin, end);
}
# endif

# if HAVE_BINUTILS >= 217
static void strreverse_l_SSE3(char *begin, char *end)
{
	char *t;

	asm (
			"lea	-31(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jae	2f\n\t"
			".p2align 2\n"
			"1:\n\t"
			"lddqu	-15(%1), %%xmm0\n\t"       /* fetch input data */
			"lddqu	(%0), %%xmm2\n\t"
			/* swab endianess */
			"movdqa	%%xmm0, %%xmm1\n\t"
			"movdqa	%%xmm2, %%xmm3\n\t"
			"psrlw	$8, %%xmm0\n\t"
			"psrlw	$8, %%xmm2\n\t"
			"psllw	$8, %%xmm1\n\t"
			"psllw	$8, %%xmm3\n\t"
			"por	%%xmm1, %%xmm0\n\t"
			"por	%%xmm3, %%xmm2\n\t"
			"pshuflw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshuflw	$0x1b, %%xmm2, %%xmm2\n\t"
			"pshufhw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufhw	$0x1b, %%xmm2, %%xmm2\n\t"
			"pshufd	$0x4e, %%xmm0, %%xmm0\n\t"
			"pshufd	$0x4e, %%xmm2, %%xmm2\n\t"
			"movdqu	%%xmm0, (%0)\n\t"
			"add	$16, %0\n\t"
			"movdqu	%%xmm2, -15(%1)\n\t"
			"sub	$16, %1\n\t"
			"lea	-31(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jb	1b\n"
			"2:\n\t"
#ifndef __x86_64__
			"lea	-15(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jae	4f\n"
			"3:\n\t"
			"movq	-7(%1), %%xmm0\n\t"       /* fetch input data */
			"movq	(%0), %%xmm2\n\t"
			/* swab endianess */
			"movdqa	%%xmm0, %%xmm1\n\t"
			"movdqa	%%xmm2, %%xmm3\n\t"
			"psrlw	$8, %%xmm0\n\t"
			"psrlw	$8, %%xmm2\n\t"
			"psllw	$8, %%xmm1\n\t"
			"psllw	$8, %%xmm3\n\t"
			"por	%%xmm1, %%xmm0\n\t"
			"por	%%xmm3, %%xmm2\n\t"
			"pshuflw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshuflw	$0x1b, %%xmm2, %%xmm2\n\t"
			"movq	%%xmm0, (%0)\n\t"
			"add	$8, %0\n\t"
			"movq	%%xmm2, -7(%1)\n\t"
			"sub	$8, %1\n\t"
			"4:"
#endif
		: /* %0 */ "=r" (begin),
		  /* %1 */ "=r" (end),
		  /* %2 */ "=r" (t)
		: /*    */ "0" (begin),
		  /*    */ "1" (end)
#ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3"
#endif
	);
	strreverse_l_generic(begin, end);
}
# endif
#endif

static void strreverse_l_SSE2(char *begin, char *end)
{
	char *t;

	asm (
			"lea	-31(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jae	2f\n\t"
			".p2align 2\n"
			"1:\n\t"
			"movdqu	-15(%1), %%xmm0\n\t"       /* fetch input data */
			"movdqu	(%0), %%xmm2\n\t"
			/* swab endianess */
			"movdqa	%%xmm0, %%xmm1\n\t"
			"movdqa	%%xmm2, %%xmm3\n\t"
			"psrlw	$8, %%xmm0\n\t"
			"psrlw	$8, %%xmm2\n\t"
			"psllw	$8, %%xmm1\n\t"
			"psllw	$8, %%xmm3\n\t"
			"por	%%xmm1, %%xmm0\n\t"
			"por	%%xmm3, %%xmm2\n\t"
			"pshuflw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshuflw	$0x1b, %%xmm2, %%xmm2\n\t"
			"pshufhw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufhw	$0x1b, %%xmm2, %%xmm2\n\t"
			"pshufd	$0x4e, %%xmm0, %%xmm0\n\t"
			"pshufd	$0x4e, %%xmm2, %%xmm2\n\t"
			"movdqu	%%xmm0, (%0)\n\t"
			"add	$16, %0\n\t"
			"movdqu	%%xmm2, -15(%1)\n\t"
			"sub	$16, %1\n\t"
			"lea	-31(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jb	1b\n"
			"2:\n\t"
#ifndef __x86_64__
			"lea	-15(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jae	4f\n"
			"3:\n\t"
			"movq	-7(%1), %%xmm0\n\t"       /* fetch input data */
			"movq	(%0), %%xmm2\n\t"
			/* swab endianess */
			"movdqa	%%xmm0, %%xmm1\n\t"
			"movdqa	%%xmm2, %%xmm3\n\t"
			"psrlw	$8, %%xmm0\n\t"
			"psrlw	$8, %%xmm2\n\t"
			"psllw	$8, %%xmm1\n\t"
			"psllw	$8, %%xmm3\n\t"
			"por	%%xmm1, %%xmm0\n\t"
			"por	%%xmm3, %%xmm2\n\t"
			"pshuflw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshuflw	$0x1b, %%xmm2, %%xmm2\n\t"
			"movq	%%xmm0, (%0)\n\t"
			"add	$8, %0\n\t"
			"movq	%%xmm2, -7(%1)\n\t"
			"sub	$8, %1\n\t"
			"4:"
#endif
		: /* %0 */ "=r" (begin),
		  /* %1 */ "=r" (end),
		  /* %2 */ "=r" (t)
		: /*    */ "0" (begin),
		  /*    */ "1" (end)
#ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3"
#endif
	);
	strreverse_l_generic(begin, end);
}

#ifndef __x86_64__
static void strreverse_l_SSE(char *begin, char *end)
{
	char *t;

	asm (
			"lea	-15(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jae	2f\n\t"
			".p2align 2\n"
			"1:\n\t"
			"movq	-7(%1), %%mm0\n\t"       /* fetch input data */
			"movq	(%0), %%mm2\n\t"
			/* swab endianess */
			"movq	%%mm0, %%mm1\n\t"
			"movq	%%mm2, %%mm3\n\t"
			"psrlw	$8, %%mm0\n\t"
			"psrlw	$8, %%mm2\n\t"
			"psllw	$8, %%mm1\n\t"
			"psllw	$8, %%mm3\n\t"
			"por	%%mm1, %%mm0\n\t"
			"por	%%mm3, %%mm2\n\t"
			"pshufw	$0x1b, %%mm0, %%mm0\n\t"
			"pshufw	$0x1b, %%mm2, %%mm2\n\t"
			"movq	%%mm0, (%0)\n\t"
			"add	$8, %0\n\t"
			"movq	%%mm2, -7(%1)\n\t"
			"sub	$8, %1\n\t"
			"lea	-15(%1), %2\n\t"
			"cmp	%2, %0\n\t"
			"jb	1b\n"
			"2:"
		: /* %0 */ "=r" (begin),
		  /* %1 */ "=r" (end),
		  /* %2 */ "=r" (t)
		: /*    */ "0" (begin),
		  /*    */ "1" (end)
# ifdef __MMX__
		: "mm0", "mm1", "mm2", "mm3"
# endif
	);
	strreverse_l_generic(begin, end);
}
#endif

static __init_cdata const struct test_cpu_feature tfeat_strreverse_l[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
	{.func = (void (*)(void))strreverse_l_AVX2,    .features = {[4] = CFB(CFEATURE_AVX2)} .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))strreverse_l_SSSE3,   .features = {[1] = CFB(CFEATURE_SSSE3)}},
	{.func = (void (*)(void))strreverse_l_SSE3,    .features = {[1] = CFB(CFEATURE_SSE3)}},
# endif
#endif
	{.func = (void (*)(void))strreverse_l_SSE2,    .features = {[0] = CFB(CFEATURE_SSE2)}},
#ifndef __x86_64__
	{.func = (void (*)(void))strreverse_l_SSE,     .features = {[0] = CFB(CFEATURE_SSE)}},
	{.func = (void (*)(void))strreverse_l_SSE,     .features = {[2] = CFB(CFEATURE_MMXEXT)}},
#endif
	{.func = (void (*)(void))strreverse_l_generic, .features = {}, .flags = CFF_DEFAULT},
};

DYN_JMP_DISPATCH_NR(strreverse_l, (char *begin, char *end), (begin, end))

/*@unused@*/
static char const rcsid_srl[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
