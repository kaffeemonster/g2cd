/*
 * popcountst.c
 * calculate popcount in size_t, ppc64 implementation
 *
 * Copyright (c) 2006-2010 Jan Seiffert
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

#ifndef __powerpc64__
# include "../generic/popcountst.c"
#else
size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL popcountst(size_t n)
{
	/* according to POWER5 spec */
	size_t tmp;
	__asm__ __volatile__("popcntb	%0, %1" : "=r" (tmp) : "r" (n) : "cc");
	return (tmp * 0x0101010101010101ULL) >> 56;
}
#endif

static char const rcsid_pc[] GCC_ATTR_USED_VAR = "$Id:$";
