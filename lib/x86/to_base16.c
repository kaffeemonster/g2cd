/*
 * to_base16.c
 * convert binary string to hex, x86 impl.
 *
 * Copyright (c) 2010 Jan Seiffert
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

#define ONLY_REMAINDER
#define ARCH_NAME_SUFFIX _generic
#include "../generic/to_base16.c"
#include "x86_features.h"

static const uint32_t vals[][4] GCC_ATTR_ALIGNED(16) =
{
	{0x33323130, 0x37363534, 0x62613938, 0x66656463},
	{0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f},
	{0x30303030, 0x30303030, 0x30303030, 0x30303030},
	{0x39393939, 0x39393939, 0x39393939, 0x39393939},
	{0x27272727, 0x27272727, 0x27272727, 0x27272727},
};

static noinline unsigned char *to_base16_MMX(unsigned char *dst, const unsigned char *src, unsigned len)
{
#ifndef __x86_64__
	asm(
		"cmp	$8, %2\n\t"
		"jb	2f\n"
		"movq	   %8, %%mm2\n\t"
		"movq	16+%8, %%mm7\n\t"
		"movq	32+%8, %%mm6\n\t"
		"movq	48+%8, %%mm5\n\t"
		".p2align	2\n\t"
		"1:\n\t"
		"sub	$8, %2\n\t"
		"movq	%%mm2, %%mm4\n\t"
		"movq	(%1), %%mm0\n\t"
		"add	$8, %1\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"pand	%%mm4, %%mm0\n\t"
		"pandn	%%mm1, %%mm4\n\t"
		"psrlq	$4, %%mm4\n\t"
		"paddb	%%mm7, %%mm0\n\t"
		"paddb	%%mm7, %%mm4\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"movq	%%mm4, %%mm3\n\t"
		"pcmpgtb	%%mm6, %%mm1\n\t"
		"pcmpgtb	%%mm6, %%mm3\n\t"
		"pand	%%mm5, %%mm1\n\t"
		"pand	%%mm5, %%mm3\n\t"
		"paddb	%%mm1, %%mm0\n\t"
		"paddb	%%mm3, %%mm4\n\t"
		"movq	%%mm4, %%mm1\n\t"
		"punpcklbw	%%mm0, %%mm4\n\t"
		"punpckhbw	%%mm0, %%mm1\n\t"
		"movq	%%mm4,   (%0)\n\t"
		"movq	%%mm1,  8(%0)\n\t"
		"add	$16, %0\n\t"
		"cmp	$8, %2\n\t"
		"jae	1b\n"
		"2:"
		: /* %0 */ "=r" (dst),
		  /* %1 */ "=r" (src),
		  /* %2 */ "=r" (len),
		  /* %3 */ "=m" (*dst)
		: /* %4 */ "0" (dst),
		  /* %5 */ "1" (src),
		  /* %6 */ "2" (len),
		  /* %7 */ "m" (*src),
		  /* %8 */ "m" (vals[1][0])
# ifdef __MMX__
		: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
# endif
	);
#endif
	if(unlikely(len))
		return to_base16_generic(dst, src, len);
	return dst;
}

static unsigned char *to_base16_SSE2(unsigned char *dst, const unsigned char *src, unsigned len)
{
	asm(
		"cmp	$16, %2\n\t"
		"jb	2f\n"
		"movdqa	   %8, %%xmm2\n\t"
		"movdqa	16+%8, %%xmm7\n\t"
		"movdqa	32+%8, %%xmm6\n\t"
		"movdqa	48+%8, %%xmm5\n\t"
		".p2align	2\n\t"
		"1:\n\t"
		"sub	$16, %2\n\t"
		"movdqa	%%xmm2, %%xmm4\n\t"
		"movdqu	(%1), %%xmm0\n\t"
		"add	$16, %1\n\t"
		"movdqa	%%xmm0, %%xmm1\n\t"
		"pand	%%xmm4, %%xmm0\n\t"
		"pandn	%%xmm1, %%xmm4\n\t"
		"psrlq	$4, %%xmm4\n\t"
		"paddb	%%xmm7, %%xmm0\n\t"
		"paddb	%%xmm7, %%xmm4\n\t"
		"movdqa	%%xmm0, %%xmm1\n\t"
		"movdqa	%%xmm4, %%xmm3\n\t"
		"pcmpgtb	%%xmm6, %%xmm1\n\t"
		"pcmpgtb	%%xmm6, %%xmm3\n\t"
		"pand	%%xmm5, %%xmm1\n\t"
		"pand	%%xmm5, %%xmm3\n\t"
		"paddb	%%xmm1, %%xmm0\n\t"
		"paddb	%%xmm3, %%xmm4\n\t"
		"movdqa	%%xmm4, %%xmm1\n\t"
		"punpcklbw	%%xmm0, %%xmm4\n\t"
		"punpckhbw	%%xmm0, %%xmm1\n\t"
		"movdqu	%%xmm4,    (%0)\n\t"
		"movdqu	%%xmm1,  16(%0)\n\t"
		"add	$32, %0\n\t"
		"cmp	$16, %2\n\t"
		"jae	1b\n"
		"2:"
		: /* %0 */ "=r" (dst),
		  /* %1 */ "=r" (src),
		  /* %2 */ "=r" (len),
		  /* %3 */ "=m" (*dst)
		: /* %4 */ "0" (dst),
		  /* %5 */ "1" (src),
		  /* %6 */ "2" (len),
		  /* %7 */ "m" (*src),
		  /* %8 */ "m" (vals[1][0])
#ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
	);
	if(unlikely(len))
		return to_base16_MMX(dst, src, len);
	return dst;
}

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
static unsigned char *to_base16_SSSE3(unsigned char *dst, const unsigned char *src, unsigned len)
{
	asm(
		"cmp	$16, %2\n\t"
		"jb	2f\n"
		"movdqa	   %8, %%xmm6\n\t"
		"movdqa	16+%8, %%xmm7\n\t"
		".p2align	2\n\t"
		"1:\n\t"
		"sub	$16, %2\n\t"
		"movdqu	(%1), %%xmm0\n\t"
		"movdqa	%%xmm7, %%xmm1\n\t"
		"movdqa	%%xmm6, %%xmm2\n\t"
		"movdqa	%%xmm6, %%xmm3\n\t"
		"add	$16, %1\n\t"
		"pandn	%%xmm0, %%xmm1\n\t"
		"pand	%%xmm7, %%xmm0\n\t"
		"psrlq	$4, %%xmm1\n\t"
		"pshufb	%%xmm0, %%xmm2\n\t"
		"pshufb	%%xmm1, %%xmm3\n\t"
		"movdqa	%%xmm3, %%xmm1\n\t"
		"punpcklbw	%%xmm2, %%xmm3\n\t"
		"punpckhbw	%%xmm2, %%xmm1\n\t"
		"movdqu	%%xmm3,    (%0)\n\t"
		"movdqu	%%xmm1,  16(%0)\n\t"
		"add	$32, %0\n\t"
		"cmp	$16, %2\n\t"
		"jae	1b\n"
		"2:"
		: /* %0 */ "=r" (dst),
		  /* %1 */ "=r" (src),
		  /* %2 */ "=r" (len),
		  /* %3 */ "=m" (*dst)
		: /* %4 */ "0" (dst),
		  /* %5 */ "1" (src),
		  /* %6 */ "2" (len),
		  /* %7 */ "m" (*src),
		  /* %8 */ "m" (vals[0][0])
#ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
	);
	if(unlikely(len))
		return to_base16_MMX(dst, src, len);
	return dst;
}
# endif
#endif

static const struct test_cpu_feature t_feat[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))to_base16_SSSE3, .flags_needed = CFEATURE_SSSE3},
# endif
#endif
	{.func = (void (*)(void))to_base16_SSE2, .flags_needed = CFEATURE_SSE2},
#ifndef __x86_64__
	{.func = (void (*)(void))to_base16_MMX, .flags_needed = CFEATURE_MMX},
#endif
	{.func = (void (*)(void))to_base16_generic, .flags_needed = -1 },
};

static unsigned char *to_base16_runtime_sw(unsigned char *dst, const unsigned char *src, unsigned len);
/*
 * Func ptr
 */
static unsigned char *(*to_base16_ptr)(unsigned char *s, const unsigned char *src, unsigned len) = to_base16_runtime_sw;

/*
 * constructor
 */
static void to_base16_select(void) GCC_ATTR_CONSTRUCT;
static void to_base16_select(void)
{
	to_base16_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer failes
 */
static unsigned char *to_base16_runtime_sw(unsigned char *dst, const unsigned char *src, unsigned len)
{
	to_base16_select();
	return to_base16(dst, src, len);
}

/*
 * trampoline
 */
unsigned char *to_base16(unsigned char *dst, const unsigned char *src, unsigned len)
{
	return to_base16_ptr(dst, src, len);
}

static char const rcsid_tb16x[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
