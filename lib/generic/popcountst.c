/*
 * popcountst.c
 * calculate popcount in size_t, generic implementation
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

#ifndef ARCH_NAME_SUFFIX
# define F_NAME(z, x, y) z GCC_ATTR_CONST GCC_ATTR_FASTCALL x
#else
# define F_NAME(z, x, y) static z GCC_ATTR_CONST GCC_ATTR_FASTCALL x##y
#endif

F_NAME(size_t, popcountst, _generic) (size_t n)
{
	n -= (n & MK_C(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_C(0x33333333L)) + (n & MK_C(0x33333333L));
	n = ((n >> 4) + n) & MK_C(0x0f0f0f0fL);
	n = ((n >> 8) + n);
	n = ((n >> 16) + n);
	if(SIZE_T_BITS >= 64)
		n = ((n >> 32) + n);
	n &= 0xff;
	return n;
}
#undef F_NAME

static char const rcsid_pcg[] GCC_ATTR_USED_VAR = "$Id:$";
