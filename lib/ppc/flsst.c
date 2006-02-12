/*
 * flsst.c
 * find last set in size_t, ppc implementation
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
 * $Id: $
 */

inline size_t flsst(size_t find)
{
	size_t found;
/* ppc does not know fls but clz */
	__asm__("cntlzw\t%0, %1\n" : "=r" (found) : "r" (find));
	return 32 - found;
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id:$";
