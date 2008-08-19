/*
 * popcountst.c
 * calculate popcount in size_t, x86 implementation
 *
 * Copyright (c) 2008 Jan Seiffert
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

#include "x86_features.h"

static size_t popcountst_SSE4(size_t n) GCC_ATTR_CONST;
static size_t popcountst_generic(size_t n) GCC_ATTR_CONST;

size_t (*popcountst)(size_t n) GCC_ATTR_CONST = popcountst_generic;

static void popcount_select(void) GCC_ATTR_CONSTRUCT;
static void popcount_select(void)
{
	struct test_cpu_feature f[] =
	{
		{.func = (void (*)(void))popcountst_SSE4, .flags_needed = CFEATURE_POPCNT},
		{.func = (void (*)(void))popcountst_SSE4, .flags_needed = CFEATURE_SSE4A},
		{.func = (void (*)(void))popcountst_generic, .flags_needed = -1 },
	};
	test_cpu_feature(&popcountst, f, anum(f));
}

static size_t popcountst_SSE4(size_t n)
{
	size_t tmp;
	__asm__ ("popcnt\t%1, %0\n" : "=r" (tmp) : "g" (n): "cc");
	return tmp;
}

#define ARCH_NAME_SUFFIX _generic
#include "../generic/popcountst.c"

static char const rcsid_pcx[] GCC_ATTR_USED_VAR = "$Id:$";
