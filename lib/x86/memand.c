/*
 * memand.c
 * and two memory region efficient, x86 implementation
 *
 * Copyright (c) 2004-2010 Jan Seiffert
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

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
#  define HAVE_SSE3
#  undef ARCH_NAME_SUFFIX
#  define ARCH_NAME_SUFFIX _SSE3
static void *DFUNC_NAME(memand, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#  include "memand_tmpl.c"

#  define HAVE_3DNOW
#  undef ARCH_NAME_SUFFIX
#  define ARCH_NAME_SUFFIX _SSE3_3DNOW
static void *DFUNC_NAME(memand, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#  include "memand_tmpl.c"
#  undef HAVE_3DNOW
# endif
#endif

#ifdef HAVE_BINUTILS
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
static __init_cdata const struct test_cpu_feature t_feat[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 219
	{.func = (void (*)(void))memand_AVX, .flags_needed = CFEATURE_AVX, .callback = test_cpu_feature_avx_callback},
# endif
#endif
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))memand_SSE3_3DNOW, .flags_needed = CFEATURE_SSE3, .callback = test_cpu_feature_3dnowprf_callback},
	{.func = (void (*)(void))memand_SSE3_3DNOW, .flags_needed = CFEATURE_SSE3, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memand_SSE3, .flags_needed = CFEATURE_SSE3, .callback = NULL},
# endif
#endif
	{.func = (void (*)(void))memand_SSE2_3DNOW, .flags_needed = CFEATURE_SSE2, .callback = test_cpu_feature_3dnowprf_callback},
	{.func = (void (*)(void))memand_SSE2_3DNOW, .flags_needed = CFEATURE_SSE2, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memand_SSE2, .flags_needed = CFEATURE_SSE2, .callback = NULL},
	{.func = (void (*)(void))memand_SSE_3DNOW, .flags_needed = CFEATURE_SSE, .callback = test_cpu_feature_3dnowprf_callback},
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
static __init void memand_select(void)
{
	memand_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer failes
 */
static __init void *memand_runtime_sw(void *dst, const void *src, size_t len)
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
