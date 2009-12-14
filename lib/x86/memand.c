/*
 * memand.c
 * and two memory region efficient, x86 implementation
 *
 * Copyright (c) 2004-2008 Jan Seiffert
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
static void *DFUNC_NAME(memand, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memand_tmpl.c"

#define HAVE_MMX
#ifndef __x86_64__
# undef ARCH_NAME_SUFFIX
# define ARCH_NAME_SUFFIX _MMX
static void *DFUNC_NAME(memand, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
# include "memand_tmpl.c"

# define HAVE_3DNOW
# undef ARCH_NAME_SUFFIX
# define ARCH_NAME_SUFFIX _MMX_3DNOW
static void *DFUNC_NAME(memand, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
# include "memand_tmpl.c"
# undef HAVE_3DNOW
#endif

#define HAVE_SSE
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE
static void *DFUNC_NAME(memand, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memand_tmpl.c"

#define HAVE_3DNOW
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE_3DNOW
static void *DFUNC_NAME(memand, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memand_tmpl.c"
#undef HAVE_3DNOW

#define HAVE_SSE2
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE2
static void *DFUNC_NAME(memand, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memand_tmpl.c"

#define HAVE_3DNOW
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE2_3DNOW
static void *DFUNC_NAME(memand, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memand_tmpl.c"
#undef HAVE_3DNOW

#define HAVE_SSE3
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE3
static void *DFUNC_NAME(memand, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memand_tmpl.c"

#define HAVE_3DNOW
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE3_3DNOW
static void *DFUNC_NAME(memand, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memand_tmpl.c"
#undef HAVE_3DNOW

#ifdef HAVE_BINTUILS
# if HAVE_BINUTILS >= 219
#  define HAVE_AVX
#  undef ARCH_NAME_SUFFIX
#  define ARCH_NAME_SUFFIX _AVX
static void *DFUNC_NAME(memand, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#  include "memand_tmpl.c"
# endif
#endif

/*
 * needed features
 */
static const struct test_cpu_feature t_feat[] =
{
#ifdef HAVE_BINTULS
# if HAVE_BINUTILS >= 219
	{.func = (void (*)(void))memand_AVX, .flags_needed = CFEATURE_AVX, .callback = test_cpu_feature_avx_callback},
# endif
#endif
	{.func = (void (*)(void))memand_SSE3_3DNOW, .flags_needed = CFEATURE_SSE3, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memand_SSE3, .flags_needed = CFEATURE_SSE3, .callback = NULL},
	{.func = (void (*)(void))memand_SSE2_3DNOW, .flags_needed = CFEATURE_SSE2, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memand_SSE2, .flags_needed = CFEATURE_SSE2, .callback = NULL},
	{.func = (void (*)(void))memand_SSE_3DNOW, .flags_needed = CFEATURE_SSE, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memand_SSE, .flags_needed = CFEATURE_SSE, .callback = NULL},
#ifndef __x86_64__
	{.func = (void (*)(void))memand_MMX_3DNOW, .flags_needed = CFEATURE_3DNOW, .callback = NULL},
	{.func = (void (*)(void))memand_MMX, .flags_needed = CFEATURE_MMX, .callback = NULL},
#endif
	{.func = (void (*)(void))memand_x86, .flags_needed = -1, .callback = NULL},
};

static void *memand_runtime_sw(void *dst, const void *src, size_t len);
/*
 * Func ptr
 */
static void *(*memand_ptr)(void *dst, const void *src, size_t len) = memand_runtime_sw;

/*
 * constructor
 */
static void memand_select(void) GCC_ATTR_CONSTRUCT;
static void memand_select(void)
{
	memand_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer failes
 */
static void *memand_runtime_sw(void *dst, const void *src, size_t len)
{
	memand_select();
	return memand_ptr(dst, src, len);
}

/*
 * trampoline
 */
void *memand(void *dst, const void *src, size_t len)
{
	return memand_ptr(dst, src, len);
}

static char const rcsid_max[] GCC_ATTR_USED_VAR = "$Id:$";
