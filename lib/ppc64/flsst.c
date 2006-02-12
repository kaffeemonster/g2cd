/*
 * flsst.c
 * find last set in size_t, ppc64 implementation
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

extern size_t _funny_st_size(size_t);
inline size_t flsst(size_t find)
{
	size_t found;
	/* ppc does not know fls but clz */
	switch(sizeof(found))
	{
	case 4:
		__asm__("cntlzw\t%0, %1\n" : "=r" (found) : "r" (find));
		return 32 - found;
	case 8:
		__asm__("cntlzd\t%0, %1\n" : "=r" (found) : "r" (find));
		return 64 - found;
	default:
		return _funny_st_size(find);
	}
}
	
static char const rcsid[] GCC_ATTR_USED_VAR = "$Id:$";
