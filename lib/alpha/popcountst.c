/*
 * popcountst.c
 * calculate popcount in size_t, alpha implementation
 *
 * Copyright (c) 2006 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2
 * of the License, or any later version.
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

inline size_t popcountst(size_t n)
{
	size_t tmp;
	__asm__ ("ctpop\t%1, %0\n" : "=r" (tmp) : "r" (n));
	return tmp;
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id:$";
