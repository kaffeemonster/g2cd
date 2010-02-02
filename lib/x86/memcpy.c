/*
 * memcpy.c
 * memcpy - x86 version
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

#include "../other.h"
#include "x86_features.h"

static noinline GCC_ATTR_FASTCALL void *memcpy_small(void *restrict dst, const void *restrict src, size_t len)
{
	char *restrict dst_c;
	const char *restrict src_c;

#ifdef __i386__
	asm (
			"push	%0\n\t"
			"push	%2\n\t"
			"lea	3(%0), %2\n\t"
			"xchg	%0, %%edi\n\t"
			"xchg	%1, %%esi\n\t"
			"and	$-4, %2\n\t"
			"sub	%%edi, %2\n\t"
			"sub	%2, (%%esp)\n\t"
			"rep	movsb\n\t"
			"mov	(%%esp), %2\n\t"
			"and	$3, (%%esp)\n\t"
			"shr	$2, %2\n\t"
			"rep	movsd\n\t"
			"pop	%2\n\t"
			"rep	movsb\n\t"
			"mov	%0, %%edi\n\t"
			"mov	%1, %%esi\n\t"
			"pop	%0\n\t"
		: "=r" (dst_c), "=r" (src_c), "=c" (len), "=m" (*(char *)dst)
		: "0" (dst), "1" (src), "2" (len), "m" (*(const char *)src)
	);
	return dst_c;
#else
	void *ret;
	size_t t;
	asm (
			"mov	%0, %3\n\t"
			"lea	7(%0,1), %4\n\t"
			"and	$-8, %4\n\t"
			"sub	%0, %4\n\t"
			"sub	%4, %2\n\t"
			"rep	movsb\n\t"
			"mov	%2, %4\n\t"
			"and	$7, %2\n\t"
			"shr	$3, %4\n\t"
			"rep	movsq\n\t"
			"mov	%4, %2\n\t"
			"rep	movsb\n\t"
		: "=D" (dst_c), "=S" (src_c), "=r" (len), "=r" (ret), "=c" (t), "=m" (*(char *)dst)
		: "0" (dst), "1" (src), "2" (len), "m" (*(const char *)src)
	);
	return ret;
#endif
}

#define HAVE_MMX
#define HAVE_SSE
#ifndef __x86_64__
static GCC_ATTR_FASTCALL void *memcpy_medium_MMX(void *restrict dst, const void *restrict src, size_t len)
{
	size_t i;
	char *restrict old_dst = dst;

	i = ALIGN_DIFF((char *)dst, 8);
	len -= i;
	asm (
		"rep	movsb"
		: "=D" (dst), "=S" (src), "=c" (i), "=m" (*(char *)dst)
		: "0" (dst), "1" (src), "2" (i), "m" (*(const char *)src)
	);
	asm (
		"and	$63, %2\n\t"
		"shr	$6, %3\n\t"
		"jz	2f\n\t"
		".p2align 3\n"
		"1:\n\t"
		"movq	  (%1), %%mm0\n\t"
		"movq	 8(%1), %%mm1\n\t"
		"movq	16(%1), %%mm2\n\t"
		"movq	24(%1), %%mm3\n\t"
		"movq	32(%1), %%mm4\n\t"
		"movq	40(%1), %%mm5\n\t"
		"movq	48(%1), %%mm6\n\t"
		"movq	56(%1), %%mm7\n\t"
		"add	$64, %1\n\t"
		"dec	%3\n\t"
		"movq	%%mm0,   (%0)\n\t"
		"movq	%%mm1,  8(%0)\n\t"
		"movq	%%mm2, 16(%0)\n\t"
		"movq	%%mm3, 24(%0)\n\t"
		"movq	%%mm4, 32(%0)\n\t"
		"movq	%%mm5, 40(%0)\n\t"
		"movq	%%mm6, 48(%0)\n\t"
		"movq	%%mm7, 56(%0)\n\t"
		"lea	64(%0), %0\n\t"
		"jnz	1b\n"
		"2:\n\t"
		"mov	%2, %3\n\t"
		"and	$7, %2\n\t"
		"shr	$3, %3\n\t"
		"jz	4f\n\t"
		".p2align 2\n"
		"3:\n\t"
		"movq	(%1), %%mm0\n\t"
		"add	$8, %1\n\t"
		"dec	%3\n\t"
		"movq	%%mm0, (%0)\n\t"
		"lea	8(%0), %0\n\t"
		"jnz	3b\n"
		"4:\n\t"
		: "=D" (dst), "=S" (src), "=c" (len), "=r" (i), "=m" (*(char *)dst)
		: "0" (dst), "1" (src), "2" (len), "3" (len), "m" (*(const char *)src)
	);
	asm (
		"mov	%2, %3\n\t"
		"and	$3, %3\n\t"
		"shr	$2, %2\n\t"
		"rep	movsd\n\t"
		"mov	%3, %2\n\t"
		"rep	movsb"
		: "=D" (dst), "=S" (src), "=c" (len), "=r" (i), "=m" (*(char *)dst)
		: "0" (dst), "1" (src), "2" (len), "m" (*(const char *)src)
	);

	return old_dst;
}

# undef ARCH_NAME_SUFFIX
# undef WANT_BIG
# define ARCH_NAME_SUFFIX _medium_SSE
# include "memcpy_tmpl.c"
# undef ARCH_NAME_SUFFIX
# define WANT_BIG
# define ARCH_NAME_SUFFIX _big_SSE
# include "memcpy_tmpl.c"

# define HAVE_3DNOW
# undef ARCH_NAME_SUFFIX
# undef WANT_BIG
# define ARCH_NAME_SUFFIX _medium_SSE_3DNOW
# include "memcpy_tmpl.c"
# undef ARCH_NAME_SUFFIX
# define WANT_BIG
# define ARCH_NAME_SUFFIX _big_SSE_3DNOW
# include "memcpy_tmpl.c"
# undef HAVE_3DNOW
#endif

#define HAVE_SSE2
#undef ARCH_NAME_SUFFIX
#undef WANT_BIG
#define ARCH_NAME_SUFFIX _medium_SSE2
#include "memcpy_tmpl.c"
#undef ARCH_NAME_SUFFIX
#define WANT_BIG
#define ARCH_NAME_SUFFIX _big_SSE2
#include "memcpy_tmpl.c"

#define HAVE_3DNOW
#undef ARCH_NAME_SUFFIX
#undef WANT_BIG
#define ARCH_NAME_SUFFIX _medium_SSE2_3DNOW
#include "memcpy_tmpl.c"
#undef ARCH_NAME_SUFFIX
#define WANT_BIG
#define ARCH_NAME_SUFFIX _big_SSE2_3DNOW
#include "memcpy_tmpl.c"
#undef HAVE_3DNOW

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
#  define HAVE_SSE3
#  undef ARCH_NAME_SUFFIX
#  undef WANT_BIG
#  define ARCH_NAME_SUFFIX _medium_SSE3
#  include "memcpy_tmpl.c"
#  undef ARCH_NAME_SUFFIX
#  define WANT_BIG
#  define ARCH_NAME_SUFFIX _big_SSE3
#  include "memcpy_tmpl.c"

#  define HAVE_3DNOW
#  undef ARCH_NAME_SUFFIX
#  undef WANT_BIG
#  define ARCH_NAME_SUFFIX _medium_SSE3_3DNOW
#  include "memcpy_tmpl.c"
#  undef ARCH_NAME_SUFFIX
#  define WANT_BIG
#  define ARCH_NAME_SUFFIX _big_SSE3_3DNOW
#  include "memcpy_tmpl.c"
#  undef HAVE_3DNOW
# endif
#endif

static const struct test_cpu_feature t_feat_med[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))memcpy_medium_SSE3_3DNOW, .flags_needed = CFEATURE_SSE3, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memcpy_medium_SSE3, .flags_needed = CFEATURE_SSE3},
# endif
#endif
	{.func = (void (*)(void))memcpy_medium_SSE2_3DNOW, .flags_needed = CFEATURE_SSE2, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memcpy_medium_SSE2, .flags_needed = CFEATURE_SSE2},
#ifndef __x86_64__
	{.func = (void (*)(void))memcpy_medium_SSE_3DNOW, .flags_needed = CFEATURE_SSE, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memcpy_medium_SSE, .flags_needed = CFEATURE_SSE},
	{.func = (void (*)(void))memcpy_medium_MMX, .flags_needed = CFEATURE_MMX},
#endif
	{.func = (void (*)(void))memcpy_small, .flags_needed = -1 },
};

static const struct test_cpu_feature t_feat_big[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))memcpy_big_SSE3_3DNOW, .flags_needed = CFEATURE_SSE3, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memcpy_big_SSE3, .flags_needed = CFEATURE_SSE3},
# endif
#endif
	{.func = (void (*)(void))memcpy_big_SSE2_3DNOW, .flags_needed = CFEATURE_SSE2, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memcpy_big_SSE2, .flags_needed = CFEATURE_SSE2},
#ifndef __x86_64__
	{.func = (void (*)(void))memcpy_big_SSE_3DNOW, .flags_needed = CFEATURE_SSE, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memcpy_big_SSE, .flags_needed = CFEATURE_SSE},
	{.func = (void (*)(void))memcpy_medium_MMX, .flags_needed = CFEATURE_MMX},
#endif
	{.func = (void (*)(void))memcpy_small, .flags_needed = -1 },
};

static GCC_ATTR_FASTCALL void *memcpy_runtime_sw(void *restrict dst, const void *restrict src, size_t len);
/*
 * Func ptr
 */
static GCC_ATTR_FASTCALL void *(*memcpy_medium_ptr)(void *restrict dst, const void *restrict src, size_t len) = memcpy_runtime_sw;
static GCC_ATTR_FASTCALL void *(*memcpy_big_ptr)(void *restrict dst, const void *restrict src, size_t len) = memcpy_runtime_sw;

/*
 * constructor
 */
static void memcpy_select(void) GCC_ATTR_CONSTRUCT;
static void memcpy_select(void)
{
	memcpy_medium_ptr = test_cpu_feature(t_feat_med, anum(t_feat_med));
	memcpy_big_ptr = test_cpu_feature(t_feat_big, anum(t_feat_big));
}

static GCC_ATTR_FASTCALL void *memcpy_runtime_sw(void *restrict dst, const void *restrict src, size_t len)
{
	memcpy_select();
	if(likely(len < 256 * 1024))
		return memcpy_medium_ptr(dst, src, len);
	return memcpy_big_ptr(dst, src, len);
}

/*
  1) less than 64-byte use a MOV or MOVQ or MOVDQA
  2) 64+ bytes, use REP MOVSD
  3) 1KB plus use MOVQ or MOVDQA
  4) 2MB plus use MOVNTDQ or other non-temporal stores.
*/

noinline GCC_ATTR_FASTCALL void *memcpy_big(void *restrict dst, const void *restrict src, size_t len)
{
	/* trick gcc to generate lean stack frame and do a tailcail */
	if(likely(len < 512))
		return memcpy_small(dst, src, len);
	if(likely(len < 256 * 1024))
		return memcpy_medium_ptr(dst, src, len);
	return memcpy_big_ptr(dst, src, len);
}

void *memcpy(void *restrict dst, const void *restrict src, size_t len)
{
	if(likely(len < 16))
		return cpy_rest_o(dst, src, len);

	/* trick gcc to generate lean stack frame and do a tailcail */
	return memcpy_big(dst, src, len);
}

static char const rcsid_mcx[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
