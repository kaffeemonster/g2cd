/*
 * tstrchrnul.c
 * tstrchrnul, generic implementation
 *
 * Copyright (c) 2009 Jan Seiffert
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

tchar_t *tstrchrnul(const tchar_t *s, tchar_t c)
{
	const char *p;
	size_t r, mask, x;
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
	mask = (((size_t)c) & 0xFFFF) * MK_C(0x00010001);
	p  = (const char *)ALIGN_DOWN(s, SOST);
	shift = ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR;
	x  = *(const size_t *)p;
	if(!HOST_IS_BIGENDIAN)
		x >>= shift;
	r  = has_nul_word(x);
	x ^= mask;
	r |= has_nul_word(x);
	r <<= shift;
	if(HOST_IS_BIGENDIAN)
		r >>= shift;

	while(!r)
	{
		p += SOST;
		x  = *(const size_t *)p;
		r  = has_nul_word(x);
		x ^= mask;
		r |= has_nul_word(x);
	}
	r = nul_word_index(r);
	return ((tchar_t *)(uintptr_t)p) + r;
}

static char const rcsid_tscn[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
