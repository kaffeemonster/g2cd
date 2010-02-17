/*
 * tstrlen.c
 * tstrlen, alpha implementation
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

size_t tstrlen(const tchar_t *s)
{
	const char *p;
	unsigned long r;
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
	p = (const char *)ALIGN_DOWN(s, SOUL);
	shift = ALIGN_DOWN_DIFF(s, SOUL);
	r = cmpbeqz(*(const unsigned long *)p);
	r = r & ((r & 0xAA) >> 1);
	if(!HOST_IS_BIGENDIAN)
		r >>= shift;
	else
		r <<= shift + SOULM1 * BITS_PER_CHAR;
	if(r)
		return alpha_nul_byte_index_b(r) / 2;

	do
	{
		p += SOUL;
		r = cmpbeqz(*(const unsigned long *)p);
		r = r & ((r & 0xAA) >> 1);
	} while(!r);
	r = alpha_nul_byte_index_e(r) / 2;
	return ((const tchar_t *)p) - s + r;
}

static char const rcsid_tsl[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
