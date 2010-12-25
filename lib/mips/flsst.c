/*
 * flsst.c
 * find last set in size_t, mips implementation
 *
 * Copyright (c) 2007-2010 Jan Seiffert
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

#if __mips >= 32
# include "../my_bitopsm.h"

static inline size_t flsst_32(size_t find)
{
	size_t found;
	/* mips knows clz */
	__asm__("clz\t%1, %0\n" : "=r" (found) : "r" (find));
	return SIZE_T_BITS - found;
}

# if __mips == 64 || defined(__mips64)
static inline size_t flsst_64(size_t find)
{
	size_t found;
	/* mips knows clz */
	__asm__("dclz\t%1, %0\n" : "=r" (found) : "r" (find));
	return SIZE_T_BITS - found;
}
# else
size_t flsst_64(size_t find);
# endif

extern size_t illigal_size_t_size(size_t);

size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL flsst(size_t find)
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

static char const rcsid_flm[] GCC_ATTR_USED_VAR = "$Id:$";
#else
# include "../generic/flsst.c"
#endif
