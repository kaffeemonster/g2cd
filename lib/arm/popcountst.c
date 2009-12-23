/*
 * popcountst.c
 * calculate popcount in size_t, arm implementation
 *
 * Copyright (c) 2004-2009 Jan Seiffert
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

#if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || \
    defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || \
    defined(__ARM_ARCH_7A__)
size_t popcountst(size_t n)
{
	n -= (n & MK_C(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_C(0x33333333L)) + (n & MK_C(0x33333333L));
	n = ((n >> 4) + n) & MK_C(0x0f0f0f0fL);
	asm("usad8	%0, %1, %2" : "=r" (n) : "r" (n), "r" (0));
	return n;
}

static char const rcsid_pca[] GCC_ATTR_USED_VAR = "$Id:$";
#else
# include "../generic/popcountst.c"
#endif
