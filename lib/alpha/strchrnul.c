/*
 * strchrnul.c
 * strchrnul for non-GNU platforms, alpha implementation
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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

#include "alpha.h"

char *strchrnul(const char *s, int c)
{
	const char *p;
	unsigned long r, mask, x;
	unsigned shift;

	if(unlikely(!c))
		return (char *)(uintptr_t)s + strlen(s);
	prefetch(s);

	/*
	 * Sometimes you need a new perspective, like the altivec
	 * way of handling things.
	 * Lower address bits? Totaly overestimated.
	 *
	 * We don't precheck for alignment.
	 * Instead we "align hard", do one load "under the address",
	 * mask the excess info out and afterwards we are fine to go.
	 */
	mask = (c & 0xFF) * 0x0101010101010101UL;
	p  = (const char *)ALIGN_DOWN(s, SOUL);
	shift = ALIGN_DOWN_DIFF(s, SOUL);
	x  = *(const unsigned long *)p;
	r  = cmpbeqz(x);
	x ^= mask;
	r |= cmpbeqz(x);
	if(!HOST_IS_BIGENDIAN)
		r >>= shift;
	else
		r <<= shift + SOULM1 * BITS_PER_CHAR;
	if(r)
		return (char *)(uintptr_t)s + alpha_nul_byte_index_b(r);

	do
	{
		p += SOUL;
		x  = *(const unsigned long *)p;
		r  = cmpbeqz(x);
		x ^= mask;
		r |= cmpbeqz(x);
	} while(!r);
	r = alpha_nul_byte_index_e(r);
	return (char *)(uintptr_t)p + r;
}

static char const rcsid_scna[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
