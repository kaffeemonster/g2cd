/*
 * tstrchrnul.c
 * tstrchrnul, alpha implementation
 *
 * Copyright (c) 2010 Jan Seiffert
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
	r  = cmpbge(x, 0x0101010101010101UL);
	r  = r & ((r & 0xAA) >> 1);
	x ^= mask;
	x  = cmpbge(x, 0x0101010101010101UL);
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
		r  = cmpbge(x, 0x0101010101010101UL);
		r  = r & ((r & 0xAA) >> 1);
		x ^= mask;
		x  = cmpbge(x, 0x0101010101010101UL);
		r |= x & ((x & 0xAA) >> 1);
	} while(!r);
	r = alpha_nul_byte_index_e(r) / 2;
	return ((tchar_t *)(uintptr_t)p) + r;
}

static char const rcsid_tscn[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
