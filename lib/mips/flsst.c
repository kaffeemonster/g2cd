/*
 * flsst.c
 * find last set in size_t, mips implementation
 *
 * Copyright (c) 2007-2008 Jan Seiffert
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

#include "../my_bitopsm.h"

static inline size_t flsst_32(size_t find)
{
	size_t found;
	/* mips knows clz */
	__asm__("clz\t%1, %0\n" : "=r" (found) : "r" (find));
	return SIZE_T_BITS - found;
}

static inline size_t flsst_64(size_t find);
#if __mips == 64
static inline size_t flsst_64(size_t find)
{
	size_t found;
	/* mips knows clz */
	__asm__("dclz\t%1, %0\n" : "=r" (found) : "r" (find));
	return SIZE_T_BITS - found;
}
#endif

extern size_t illigal_size_t_size(size_t);

size_t flsst(size_t find)
{
	switch(sizeof(size_t))
	{
	case 4:
		return flsst_32(find);
	case 8:
		return flsst_64(find);
	}

	return illigal_size_t_size(find);
}

static char const rcsid_fl[] GCC_ATTR_USED_VAR = "$Id:$";
