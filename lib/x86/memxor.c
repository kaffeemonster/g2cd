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

#include "../../other.h"
#include "x86_features.h"

#undef HAVE_MMX
#undef HAVE_SSE
#undef HAVE_SSE2

#define ARCH_NAME_SUFFIX _x86
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memxor_tmpl.c"

#define HAVE_MMX
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _MMX
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memxor_tmpl.c"

#define HAVE_SSE
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memxor_tmpl.c"

#define HAVE_SSE2
#undef ARCH_NAME_SUFFIX
#define ARCH_NAME_SUFFIX _SSE2
static void *DFUNC_NAME(memxor, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len);
#include "memxor_tmpl.c"

void *(*memxor)(void *dst, const void *src, size_t len) = memxor_x86;

static void memxor_select(void) GCC_ATTR_CONSTRUCT;
static void memxor_select(void)
{
	struct test_cpu_feature f[] =
	{
		{.func = (void (*)(void))memxor_SSE2, .flags_needed = CFEATURE_SSE2},
		{.func = (void (*)(void))memxor_SSE, .flags_needed = CFEATURE_SSE},
		{.func = (void (*)(void))memxor_MMX, .flags_needed = CFEATURE_MMX},
		{.func = (void (*)(void))memxor_x86, .flags_needed = -1},
	};
	test_cpu_feature((void (**)(void))&memxor, f, anum(f));
}


static char const rcsid_mxx[] GCC_ATTR_USED_VAR = "$Id:$";
