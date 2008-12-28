/*
 * strlen.c
 * strlen, generic implementation
 *
 * Copyright (c) 2008 Jan Seiffert
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

size_t strlen(const char *s)
{
	const char *p;
	size_t r;
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
	p = (const char *)ALIGN_DOWN(s, SOST);
	r = has_nul_byte(*(const size_t *)p);
	if(!HOST_IS_BIGENDIAN)
		r >>= ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR;
	else
		r <<= ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR;
	if(r)
		return nul_byte_index(r);

	do
	{
		p += SOST;
		r = has_nul_byte(*(const size_t *)p);
	} while(!r);
	r = nul_byte_index(r);
	return p - s + r;
}

static char const rcsid_sl[] GCC_ATTR_USED_VAR = "$Id: $";
