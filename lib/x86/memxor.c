/*
 * memxor.c
 * xor two memory region efficient, x86 implementation
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
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memxor_tmpl.c"

#define HAVE_MMX
#ifndef __x86_64__
# undef ARCH_NAME_SUFFIX
# define ARCH_NAME_SUFFIX _MMX
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
# include "memxor_tmpl.c"

# define HAVE_3DNOW
# undef ARCH_NAME_SUFFIX
# define ARCH_NAME_SUFFIX _MMX_3DNOW
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
# include "memxor_tmpl.c"
#undef HAVE_3DNOW
#endif

#define HAVE_SSE
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memxor_tmpl.c"

#define HAVE_3DNOW
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE_3DNOW
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memxor_tmpl.c"
#undef HAVE_3DNOW

#define HAVE_SSE2
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE2
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memxor_tmpl.c"

#define HAVE_3DNOW
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE2_3DNOW
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memxor_tmpl.c"
#undef HAVE_3DNOW

#define HAVE_SSE3
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE3
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memxor_tmpl.c"

#define HAVE_3DNOW
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE3_3DNOW
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memxor_tmpl.c"
#undef HAVE_3DNOW

#if HAVE_BINUTILS >= 219
# define HAVE_AVX
# undef ARCH_NAME_SUFFIX
# define ARCH_NAME_SUFFIX _AVX
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
# include "memxor_tmpl.c"
#endif

/*
 * needed features
 */
static const struct test_cpu_feature t_feat[] =
{
#if HAVE_BINUTILS >= 219
	{.func = (void (*)(void))memxor_AVX, .flags_needed = CFEATURE_AVX, .callback = test_cpu_feature_avx_callback},
#endif
	{.func = (void (*)(void))memxor_SSE3_3DNOW, .flags_needed = CFEATURE_SSE3, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memxor_SSE3, .flags_needed = CFEATURE_SSE3, .callback = NULL},
	{.func = (void (*)(void))memxor_SSE2_3DNOW, .flags_needed = CFEATURE_SSE2, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memxor_SSE2, .flags_needed = CFEATURE_SSE2, .callback = NULL},
	{.func = (void (*)(void))memxor_SSE_3DNOW, .flags_needed = CFEATURE_SSE, .callback = test_cpu_feature_3dnow_callback},
	{.func = (void (*)(void))memxor_SSE, .flags_needed = CFEATURE_SSE, .callback = NULL},
#ifndef __x86_64__
	{.func = (void (*)(void))memxor_MMX_3DNOW, .flags_needed = CFEATURE_3DNOW, .callback = NULL},
	{.func = (void (*)(void))memxor_MMX, .flags_needed = CFEATURE_MMX, .callback = NULL},
#endif
	{.func = (void (*)(void))memxor_x86, .flags_needed = -1, .callback = NULL},
};

static void *memxor_runtime_sw(void *dst, const void *src, size_t len);
/*
 * Func ptr
 */
static void *(*memxor_ptr)(void *dst, const void *src, size_t len) = memxor_runtime_sw;

/*
 * constructor
 */
static void memxor_select(void) GCC_ATTR_CONSTRUCT;
static void memxor_select(void)
{
	memxor_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer failes
 */
static void *memxor_runtime_sw(void *dst, const void *src, size_t len)
{
	memxor_select();
	return memxor_ptr(dst, src, len);
}

/*
 * trampoline
 */
void *memxor(void *dst, const void *src, size_t len)
{
	return memxor_ptr(dst, src, len);
}

static char const rcsid_mxx[] GCC_ATTR_USED_VAR = "$Id:$";
