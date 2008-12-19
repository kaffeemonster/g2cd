/*
 * popcountst.c
 * calculate popcount in size_t, ppc64 implementation
 *
 * Copyright (c) 2006 Jan Seiffert
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

static size_t popcountst_ppc64(size_t n) GCC_ATTR_CONST;
static size_t popcountst_generic(size_t n) GCC_ATTR_CONST;

/*
 * We always go over a function pointer to comfort x86 millions
 * SIMD evolutions. We could use this to provide different ppc
 * implementations (altivec/non-altivec, alignment constrained),
 * if only telling different ppc processors apart would be easily
 * possible...
 */
#ifndef __powerpc64__
size_t (*popcountst)(size_t n) GCC_ATTR_CONST = popcountst_generic;
static size_t popcountst_ppc64(size_t n)
{
	/* Ooops! */
#ifdef __GNUC__
	n = n;
	__builtin_trap();
#else
	size_t i = 4;
	return i / 0;
#endif
}
#else
size_t (*popcountst)(size_t n) GCC_ATTR_CONST = popcountst_ppc64;
static size_t popcountst_ppc64(size_t n)
{
	/* according to POWER5 spec */
	size_t tmp;
	__asm__ __volatile__(
		"popcntb\t%0, %1\n\t"
		"mulld\t%0, %0, %2\n\t" /*(RB) = 0x0101_0101_0101_0101*/
		"srdi\t%0, %0, 56\n"
		: "=r" (tmp)
		: "r" (n), "r" (0x0101010101010101)
		: "cc"
	);
	return tmp;
}
#endif

#define ARCH_NAME_SUFFIX _generic
#include "../generic/popcountst.c"

static char const rcsid_pc[] GCC_ATTR_USED_VAR = "$Id:$";
