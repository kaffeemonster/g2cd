/*
 * flsst.c
 * find last set in size_t, ppc implementation
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
 * $Id: $
 */

#include "../my_bitopsm.h"

extern size_t _illigal_size_t_size(size_t);
size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL flsst(size_t find)
{
	size_t found;
/* ppc does not know fls but clz */
	switch(sizeof(find))
	{
	case 4:	__asm__("cntlzw	%0, %1" : "=r" (found) : "r" (find));
		break;
	case 8:	__asm__("cntlzd	%0, %1" : "=r" (found) : "r" (find));
		break;
	default:
		return _illigal_size_t_size(find);
	}
	return SIZE_T_BITS - found;
}

static char const rcsid_fl[] GCC_ATTR_USED_VAR = "$Id:$";
