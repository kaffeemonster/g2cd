/*
 * mempopcnt.c
 * popcount a mem region, sparc64 implementation
 *
 * Copyright (c) 2009 Jan Seiffert
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

static inline size_t popcountst_int1(size_t n)
{
	size_t tmp;
	__asm__ ("popc\t%1, %0\n" : "=r" (tmp) : "r" (n));
	return tmp;
}

static inline size_t popcountst_int2(size_t n, size_t m)
{
	return popcountst_int1(n) +
	       popcountst_int1(m);
}

static inline size_t popcountst_int4(size_t n, size_t m, size_t o, size_t p)
{
	return popcountst_int1(n) +
	       popcountst_int1(m) +
	       popcountst_int1(o) +
	       popcountst_int1(p);
}

#define NO_GEN_POPER
#include "../generic/mempopcnt.c"

static char const rcsid_mps[] GCC_ATTR_USED_VAR = "$Id: $";
