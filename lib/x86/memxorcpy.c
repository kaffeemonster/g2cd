/*
 * memxorcpy.c
 * xor two memory region efficient and cpy to dest, x86 implementation
 *
 * Copyright (c) 2004-2009 Jan Seiffert
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
 * $Id:$
 */

#include "../other.h"
#include "x86_features.h"

#undef HAVE_3DNOW
#undef HAVE_MMX
#undef HAVE_SSE
#undef HAVE_SSE2

#define ARCH_NAME_SUFFIX _x86
static void *DFUNC_NAME(memxorcpy, ARCH_NAME_SUFFIX)(void *dst, const void *src1, const void *src2, size_t len);
#include "memxorcpy_tmpl.c"

#define HAVE_MMX
#ifndef __x86_64__
# undef ARCH_NAME_SUFFIX
# define ARCH_NAME_SUFFIX _MMX
static void *DFUNC_NAME(memxorcpy, ARCH_NAME_SUFFIX)(void *dst, const void *src1, const void *src2, size_t len);
# include "memxorcpy_tmpl.c"

# define HAVE_3DNOW
# undef ARCH_NAME_SUFFIX
# define ARCH_NAME_SUFFIX _MMX_3DNOW
static void *DFUNC_NAME(memxorcpy, ARCH_NAME_SUFFIX)(void *dst, const void *src1, const void *src2, size_t len);
# include "memxorcpy_tmpl.c"
#undef HAVE_3DNOW
#endif

#define HAVE_SSE
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE
static void *DFUNC_NAME(memxorcpy, ARCH_NAME_SUFFIX)(void *dst, const void *src1, const void *src2, size_t len);
#include "memxorcpy_tmpl.c"

#define HAVE_3DNOW
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE_3DNOW
static void *DFUNC_NAME(memxorcpy, ARCH_NAME_SUFFIX)(void *dst, const void *src1, const void *src2, size_t len);
#include "memxorcpy_tmpl.c"
#undef HAVE_3DNOW

#define HAVE_SSE2
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE2
static void *DFUNC_NAME(memxorcpy, ARCH_NAME_SUFFIX)(void *dst, const void *src1, const void *src2, size_t len);
#include "memxorcpy_tmpl.c"

#define HAVE_3DNOW
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE2_3DNOW
static void *DFUNC_NAME(memxorcpy, ARCH_NAME_SUFFIX)(void *dst, const void *src1, const void *src2, size_t len);
#include "memxorcpy_tmpl.c"
#undef HAVE_3DNOW

#define HAVE_SSE3
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE3
static void *DFUNC_NAME(memxorcpy, ARCH_NAME_SUFFIX)(void *dst, const void *src1, const void *src2, size_t len);
#include "memxorcpy_tmpl.c"

#define HAVE_3DNOW
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE3_3DNOW
static void *DFUNC_NAME(memxorcpy, ARCH_NAME_SUFFIX)(void *dst, const void *src1, const void *src2, size_t len);
#include "memxorcpy_tmpl.c"
#undef HAVE_3DNOW

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 219
#  define HAVE_AVX
#  undef ARCH_NAME_SUFFIX
#  define ARCH_NAME_SUFFIX _AVX
static void *DFUNC_NAME(memxorcpy, ARCH_NAME_SUFFIX)(void *dst, const void *src1, const void *src2, size_t len);
#  include "memxorcpy_tmpl.c"
# endif
#endif

/*
 * needed features
 */
static const struct test_cpu_feature t_feat[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 219
	{.func = (void (*)(void))memxorcpy_AVX, .flags_needed = CFEATURE_AVX, .callback = test_cpu_feature_avx_callback},
# endif
#endif
	{.func = (void (*)(void))memxorcpy_SSE3_3DNOW, .flags_needed = CFEATURE_SSE3, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memxorcpy_SSE3, .flags_needed = CFEATURE_SSE3, .callback = NULL},
	{.func = (void (*)(void))memxorcpy_SSE2_3DNOW, .flags_needed = CFEATURE_SSE2, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memxorcpy_SSE2, .flags_needed = CFEATURE_SSE2, .callback = NULL},
	{.func = (void (*)(void))memxorcpy_SSE_3DNOW, .flags_needed = CFEATURE_SSE, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memxorcpy_SSE, .flags_needed = CFEATURE_SSE, .callback = NULL},
#ifndef __x86_64__
	{.func = (void (*)(void))memxorcpy_MMX_3DNOW, .flags_needed = CFEATURE_3DNOW, .callback = NULL},
	{.func = (void (*)(void))memxorcpy_MMX, .flags_needed = CFEATURE_MMX, .callback = NULL},
#endif
	{.func = (void (*)(void))memxorcpy_x86, .flags_needed = -1, .callback = NULL},
};

static void *memxorcpy_runtime_sw(void *dst, const void *src1, const void *src2, size_t len);
/*
 * Func ptr
 */
static void *(*memxorcpy_ptr)(void *dst, const void *src1, const void *src2, size_t len) = memxorcpy_runtime_sw;

/*
 * constructor
 */
static void memxorcpy_select(void) GCC_ATTR_CONSTRUCT;
static void memxorcpy_select(void)
{
	memxorcpy_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructor fails
 */
static void *memxorcpy_runtime_sw(void *dst, const void *src1, const void *src2, size_t len)
{
	memxorcpy_select();
	return memxorcpy_ptr(dst, src1, src2, len);
}

/*
 * trampoline
 */
void *memxorcpy(void *dst, const void *src1, const void *src2, size_t len)
{
	return memxorcpy_ptr(dst, src1, src2, len);
}

static char const rcsid_mxxc[] GCC_ATTR_USED_VAR = "$Id:$";
