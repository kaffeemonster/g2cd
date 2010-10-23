/*
 * popcountst.c
 * calculate popcount in size_t, x86 implementation
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
 * $Id:$
 */

#include "x86_features.h"

static size_t popcountst_generic(size_t n) GCC_ATTR_CONST GCC_ATTR_FASTCALL;
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 218
static size_t popcountst_SSE4(size_t n) GCC_ATTR_CONST GCC_ATTR_FASTCALL;
static size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL popcountst_SSE4(size_t n)
{
	size_t tmp;
	__asm__ ("popcnt	%1, %0" : "=r" (tmp) : "g" (n): "cc");
	return tmp;
}
# endif
#endif

#define ARCH_NAME_SUFFIX _generic
#include "../generic/popcountst.c"

static __init_cdata const struct test_cpu_feature t_feat[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 218
	{.func = (void (*)(void))popcountst_SSE4, .flags_needed = CFEATURE_POPCNT},
	{.func = (void (*)(void))popcountst_SSE4, .flags_needed = CFEATURE_SSE4A},
	{.func = (void (*)(void))popcountst_SSE4, .flags_needed = CFEATURE_SSE4_2},
# endif
#endif
	{.func = (void (*)(void))popcountst_generic, .flags_needed = -1 },
};

static size_t popcountst_runtime_sw(size_t n) GCC_ATTR_CONST GCC_ATTR_FASTCALL;
/*
 * Func ptr
 */
static size_t (*popcountst_ptr)(size_t n) GCC_ATTR_CONST GCC_ATTR_FASTCALL = popcountst_runtime_sw;

/*
 * constructor
 */
static void popcountst_select(void) GCC_ATTR_CONSTRUCT;
static __init void popcountst_select(void)
{
	popcountst_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer failes
 */
static __init size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL popcountst_runtime_sw(size_t n)
{
	popcountst_select();
	return popcountst(n);
}

/*
 * trampoline
 */
size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL popcountst(size_t n)
{
	return popcountst_ptr(n);
}

static char const rcsid_pcx[] GCC_ATTR_USED_VAR = "$Id:$";
