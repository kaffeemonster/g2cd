/*
 * mempopcnt.c
 * popcount a mem region, IA64 implementation
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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
 * $Id: $
 */


static inline size_t popcountst_int1(size_t n)
{
	size_t tmp;
#  ifdef __INTEL_COMPILER
	tmp = _m64_popcnt(n);
#  else
#   if _GNUC_PREREQ(3,4)
	tmp = __builtin_popcountll(n);
#   else
	__asm__ ("popcnt\t%0=%1\n" : "=r" (tmp) : "r" (n));
#   endif /* _GNUC_PREREQ(3,4) */
#  endif /* __INTEL_COMPILER */
	return tmp;
}

static inline size_t popcountst_int2(size_t n, size_t m)
{
	size_t tmp;
#  ifdef __INTEL_COMPILER
	tmp = _m64_popcnt(n) + _m64_popcnt(m);
#  else
#   if _GNUC_PREREQ(3,4)
	tmp = __builtin_popcountll(n) + __builtin_popcountll(m);
#   else
	tmp = popcountst_int1(n) + popcountst_int1(m);
#   endif /* _GNUC_PREREQ(3,4) */
#  endif /* __INTEL_COMPILER */
	return tmp;
}

static inline size_t popcountst_int4(size_t n, size_t m, size_t o, size_t p)
{
	size_t tmp;
#  ifdef __INTEL_COMPILER
	tmp = _m64_popcnt(n) + _m64_popcnt(m) + _m64_popcnt(o) + _m64_popcnt(p);
#  else
#   if _GNUC_PREREQ(3,4)
	tmp = __builtin_popcountll(n) + __builtin_popcountll(m) + __builtin_popcountll(o) + __builtin_popcountll(p);
#   else
	tmp = popcountst_int1(n) + popcountst_int1(m) + popcountst_int1(o) + popcountst_int1(p);
#   endif /* _GNUC_PREREQ(3,4) */
#  endif /* __INTEL_COMPILER */
	return tmp;
}

#define NO_GEN_POPER
#include "../generic/mempopcnt.c"

static char const rcsid_mpia[] GCC_ATTR_USED_VAR = "$Id: $";
