/*
 * flsst.c
 * find last set in size_t, i386 implementation
 *
 * Copyright (c) 2004,2005,2006 Jan Seiffert
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

size_t flsst(size_t find)
{
	size_t found;
	__asm__ __volatile__(
		"xor\t%0, %0\n\t"
		"bsr\t%1, %0\n\t"
		"jz\t1f\n\t"
		"inc\t%0\n"
		"1:\n"
		: "=r" (found)
		: "mr" (find)
		: "cc"
	);
	return found;
}

static char const rcsid_fl[] GCC_ATTR_USED_VAR = "$Id:$";
