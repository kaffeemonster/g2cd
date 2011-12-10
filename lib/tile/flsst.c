/*
 * flsst.c
 * find last set in size_t, tile implementation
 *
 * Copyright (c) 2011 Jan Seiffert
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

# include "../my_bitopsm.h"

size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL flsst(size_t find)
{
	return SIZE_T_BITS - __builtin_clzl(find); /* Tile knows clz */
}

static char const rcsid_flti[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
