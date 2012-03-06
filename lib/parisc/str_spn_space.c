/*
 * str_spn_space.c
 * count white space span length, parisc implementation
 *
 * Copyright (c) 2012 Jan Seiffert
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

#include "parisc.h"

static noinline size_t str_spn_space_slow(const char *str)
{
	const char *p;
	unsigned long x, r;
	unsigned shift;
	int n;

	p  = (const char *)ALIGN_DOWN(str, SOUL);
	shift = ALIGN_DOWN_DIFF(str, SOUL);
	x  = *(const unsigned long *)p;
	if(shift)
	{
		if(!HOST_IS_BIGENDIAN)
			x |= MK_C(0x20202020UL) >> ((SOUL - shift) * BITS_PER_CHAR);
		else
			x |= MK_C(0x20202020UL) << ((SOUL - shift) * BITS_PER_CHAR);
	}
	goto start_loop;

	do
	{
		p += SOUL;
		x  = *(const unsigned long *)p;
start_loop:
		r  = pcmp1eq(x, MK_C(0x20202020UL));
		r |= pcmp1gt(x, MK_C(0x08080808UL)) ^ pcmp1gt(x, MK_C(0x0d0d0d0dUL));
	} while(MK_C(0x01010101UL) == r);
	n = pa_find_z(r);
	return p + n - str;
}

static char const rcsid_strspnspa[] GCC_ATTR_USED_VAR = "$Id: $";
