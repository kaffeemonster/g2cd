/*
 * guid.h
 * little stuff to generate a guid
 *
 * Copyright (c) 2010 Jan Seiffert
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

#ifndef LIB_GUID_H
# define LIB_GUID_H

# define GUID_SIZE 16

# include "other.h"

/*
 * Lets start with some unportable hacks...
 */
union guid_fast
{
	uint8_t g[GUID_SIZE];
	uint32_t d[GUID_SIZE/4];
	int64_t x[GUID_SIZE/8];
};

# define LIB_GUID_EXTRN(x) x GCC_ATTR_VIS("hidden")

LIB_GUID_EXTRN(void guid_generate(unsigned char out[GUID_SIZE]));
LIB_GUID_EXTRN(void guid_tick(void));
LIB_GUID_EXTRN(void guid_init(void));
LIB_GUID_EXTRN(uint32_t guid_hash(const union guid_fast *g, uint32_t seed));
#endif
