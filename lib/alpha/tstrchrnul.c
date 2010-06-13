/*
 * tstrchrnul.c
 * tstrchrnul, alpha implementation
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

#include "alpha.h"

tchar_t *tstrchrnul(const tchar_t *s, tchar_t c)
{
	const char *p;
	unsigned long r, mask, x;
	unsigned shift;
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
	mask = (((size_t)c) & 0xFFFF) * 0x0001000100010001UL;
	p  = (const char *)ALIGN_DOWN(s, SOUL);
	shift = ALIGN_DOWN_DIFF(s, SOUL) * BITS_PER_CHAR;
	x  = *(const size_t *)p;
	r  = cmpbeqz(x);
	r  = r & ((r & 0xAA) >> 1);
	x ^= mask;
	x  = cmpbeqz(x);
	r |= x & ((x & 0xAA) >> 1);
	if(!HOST_IS_BIGENDIAN)
		r >>= shift;
	else
		r <<= shift + SOULM1 * BITS_PER_CHAR;
	if(r)
		return ((tchar_t *)(uintptr_t)s) + alpha_nul_byte_index_b(r) / 2;

	do
	{
		p += SOUL;
		x  = *(const size_t *)p;
		r  = cmpbeqz(x);
		r  = r & ((r & 0xAA) >> 1);
		x ^= mask;
		x  = cmpbeqz(x);
		r |= x & ((x & 0xAA) >> 1);
	} while(!r);
	r = alpha_nul_byte_index_e(r) / 2;
	return ((tchar_t *)(uintptr_t)p) + r;
}

static char const rcsid_tscn[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
