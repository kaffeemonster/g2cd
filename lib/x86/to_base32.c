/*
 * to_base32.c
 * convert binary string to base 32, x86 implementation
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

#define ARCH_NAME_SUFFIX _generic
#define ONLY_REMAINDER
#include "../generic/to_base32.c"

#include "x86_features.h"

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 218
# endif
# if HAVE_BINUTILS >= 217
# endif
#endif
#ifndef __x86_64__
tchar_t *to_base32_SSE(tchar_t *dst, const unsigned char *src, unsigned len);
#endif

static const uint8_t unity_mask[16] GCC_ATTR_ALIGNED(16) =
		{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

static const unsigned char vals[][16] GCC_ATTR_ALIGNED(16) =
{
	/* 1    2    3    4    5    6    7    8    9   10   11   12   13   14   15   16 */
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
	{0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF},
	{0x00,0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF},
	{0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF},
	{0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F},
	{0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41},
	{0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A},
	{0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29},
};

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
# endif
#endif

#ifndef __x86_64__
tchar_t *to_base32_SSE(tchar_t *dst, const unsigned char *src, unsigned len)
{
	asm (
			"cmp	$8, %2\n\t"
			"jb	2f\n\t"
			"movq	-48+%3, %%mm7\n\t"
			"movq	-32+%3, %%mm6\n\t"
			"movq	-16+%3, %%mm5\n\t"
			"movq	32+%3, %%mm3\n\t"
			"movq	48+%3, %%mm4\n\t"
			".p2align 2\n"
			"1:\n\t"
			"movq	(%1), %%mm0\n\t"       /* fetch input data */
			"sub	$5, %2\n\t"
			"add	$5, %1\n\t"
			/* swab endianess */
			"movq	%%mm0, %%mm1\n\t"
			"psrlw	$8, %%mm0\n\t"
			"psllw	$8, %%mm1\n\t"
			"por	%%mm1, %%mm0\n\t"
			"pshufw	$0x1b, %%mm0, %%mm0\n\t"
			/* partionate */
			"movq	%%mm0, %%mm1\n\t"      /* copy */
			"movq	%%mm7, %%mm2\n\t"
			"psrlq	$0xc, %%mm0\n\t"    /* shift copy */
			"pand	%%mm7, %%mm1\n\t"      /* eliminate */
			"pandn	%%mm0, %%mm2\n\t"
			"por	%%mm2, %%mm1\n\t"      /* join */
			"movq	%%mm6, %%mm2\n\t"
			"movq	%%mm1, %%mm0\n\t"      /* copy */
			"pand	%%mm6, %%mm1\n\t"      /* eliminate */
			"psrld	$0x6, %%mm0\n\t"    /* shift copy */
			"pandn	%%mm0, %%mm2\n\t"
			"por	%%mm2, %%mm1\n\t"      /* join */
			"movq	%%mm5, %%mm2\n\t"
			"movq	%%mm1, %%mm0\n\t"      /* copy */
			"pand	%%mm5, %%mm1\n\t"      /* eliminate */
			"psrlw	$0x3, %%mm0\n\t"    /* shift copy */
			"pandn	%%mm0, %%mm2\n\t"
			"movq	%3, %%mm0\n\t"
			"por	%%mm2, %%mm1\n\t"      /* join */
			"movq	16+%3, %%mm2\n\t"
			"psrlw	$0x3, %%mm1\n\t"    /* bring it down */
			"pand	%%mm0, %%mm1\n\t"      /* eliminate */
			/* convert */
			"paddb	%%mm2, %%mm1\n\t"
			"pxor	%%mm2, %%mm2\n\t"
			"movq	%%mm1, %%mm0\n\t"
			"pcmpgtb	%%mm3, %%mm0\n\t"
			"pand	%%mm4, %%mm0\n\t"
			"psubb	%%mm0, %%mm1\n\t"
			/* write out */
			"cmp	$0x7, %2\n\t"
			"movq	%%mm1, %%mm0\n\t"
			"punpckhbw	%%mm2, %%mm1\n\t"
			"punpcklbw	%%mm2, %%mm0\n\t"
			"pshufw	$0x1b,%%mm1, %%mm1\n\t"
			"pshufw	$0x1b,%%mm0, %%mm0\n\t"
			"movq	%%mm1,    (%0)\n\t"
			"movq	%%mm0, 0x8(%0)\n\t"
			"lea	16(%0), %0\n\t"
			"ja	1b\n"
			"2:"
		: /* %0 */ "=r" (dst),
		  /* %1 */ "=r" (src),
		  /* %2 */ "=r" (len)
		: /* %3 */ "m" (vals[4][0]),
		  /*    */ "0" (dst),
		  /*    */ "1" (src),
		  /*    */ "2" (len)
#ifdef __MMX__
		: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
#endif
	);
	return to_base32_generic(dst, src, len);
}
#endif

static const struct test_cpu_feature t_feat[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 218
# endif
# if HAVE_BINUTILS >= 217
# endif
#endif
#ifndef __x86_64__
	{.func = (void (*)(void))to_base32_SSE, .flags_needed = CFEATURE_SSE},
#endif
	{.func = (void (*)(void))to_base32_generic, .flags_needed = -1 },
};

static tchar_t *to_base32_runtime_sw(tchar_t *dst, const unsigned char *src, unsigned len);
/*
 * Func ptr
 */
static tchar_t *(*to_base32_ptr)(tchar_t *dst, const unsigned char *src, unsigned len) = to_base32_runtime_sw;

/*
 * constructor
 */
static void to_base32_select(void) GCC_ATTR_CONSTRUCT;
static void to_base32_select(void)
{
	to_base32_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer failes
 */
static tchar_t *to_base32_runtime_sw(tchar_t *dst, const unsigned char *src, unsigned len)
{
	to_base32_select();
	return to_base32(dst, src, len);
}

/*
 * trampoline
 */
tchar_t *to_base32(tchar_t *dst, const unsigned char *src, unsigned len)
{
	return to_base32_ptr(dst, src, len);
}

static char const rcsid_tb32x[] GCC_ATTR_USED_VAR = "$Id:$";
