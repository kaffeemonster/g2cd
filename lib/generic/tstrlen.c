/*
 * tstrlen.c
 * tstrlen, generic implementation
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

size_t tstrlen(const tchar_t *s)
{
	const char *p;
	size_t r;
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
	p = (const char *)ALIGN_DOWN(s, SOST);
	shift = ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR;
	r = *(const size_t *)p;
	if(!HOST_IS_BIGENDIAN)
		r >>= shift;
	r = has_nul_word(r);
	if(HOST_IS_BIGENDIAN)
		r <<= shift;
	if(r)
		return nul_word_index(r);

	do
	{
		p += SOST;
		r = has_nul_word(*(const size_t *)p);
	} while(!r);
	r = nul_word_index(r);
	return ((const tchar_t *)p) - s + r;
}

static char const rcsid_tsl[] GCC_ATTR_USED_VAR = "$Id: $";
