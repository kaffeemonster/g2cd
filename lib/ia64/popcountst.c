/*
 * popcountst.c
 * calculate popcount in size_t, IA64 implementation
 *
 * Copyright (c) 2005-2010 Jan Seiffert
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

size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL popcountst(size_t n)
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

static char const rcsid_pc[] GCC_ATTR_USED_VAR = "$Id:$";
