/*
 * flsst.c
 * find last set in size_t, alpha implementation
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

#ifdef __alpha_cix__
# include "../my_bitopsm.h"
# include "alpha.h"

size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL flsst(size_t find)
{
	return SIZE_T_BITS - ctlz(find); /* alpha knows clz */
}
#else
size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL flsst(size_t find)
{
	size_t v = find;
	size_t r;
	size_t shift;

	/*
	 * alpha likes this, because it does not have a
	 * flags register, instead the compare instr. generate
	 * 0 or 1 in any of the gp-register -> exactly like written.
	 * Also no jumps, no loads, no constants which do not fit
	 * into the instructions -> raw instruction execution (~35)
	 */
	r =     (v > 0xFFFFFFFFUL) << 8; v >>= r;
	shift = (v > 0xFFFF      ) << 4; v >>= shift;
	shift = (v > 0xFF        ) << 3; v >>= shift; r |= shift;
	shift = (v > 0xF         ) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3         ) << 1; v >>= shift; r |= shift;
	                                              r |= (v >> 1);
	return r;
}
#endif
static char const rcsid_fla[] GCC_ATTR_USED_VAR = "$Id:$";
