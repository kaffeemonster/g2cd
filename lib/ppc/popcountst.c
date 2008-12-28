/*
 * popcountst.c
 * calculate popcount in size_t, ppc64 implementation
 *
 * Copyright (c) 2006-2008 Jan Seiffert
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

#ifndef __powerpc64__
# include "../generic/popcountst.c"
#else
size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL popcountst(size_t n)
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

static char const rcsid_pc[] GCC_ATTR_USED_VAR = "$Id:$";
