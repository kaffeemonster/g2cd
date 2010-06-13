/*
 * strnlen.c
 * strnlen for non-GNU platforms, generic implementation
 *
 * Copyright (c) 2005-2010 Jan Seiffert
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

size_t strnlen(const char *s, size_t maxlen)
{
	const char *p;
	size_t r;
	ssize_t f, k;
	prefetch(s);

	/*
	 * Sometimes you need a new perspective, like the altivec
	 * way of handling things.
	 * Lower address bits? Totaly overestimated.
	 *
	 * We don't precheck for alignment.
	 * Instead we "align hard", do one load "under the address",
	 * mask the excess info out and afterwards we are fine to go.
	 *
	 * Even this beeing strNlen, this n can be seen as a "hint"
	 * We can overread and underread, but should cut the result.
	 */
	f = ALIGN_DOWN_DIFF(s, SOST);
	k = SOST - f - (ssize_t)maxlen;
	k = k > 0 ? k  : 0;

	p = (const char *)ALIGN_DOWN(s, SOST);
	r = *(const size_t *)p;
	if(!HOST_IS_BIGENDIAN)
		r >>= f * BITS_PER_CHAR;
	r = has_nul_byte(r);
	if(!HOST_IS_BIGENDIAN) {
		r <<= (k + f) * BITS_PER_CHAR;
		r >>= (k + f) * BITS_PER_CHAR;
	} else {
		r >>=  k      * BITS_PER_CHAR;
		r <<= (k + f) * BITS_PER_CHAR;
	}
	if(r)
		return nul_byte_index(r);
	else if(k)
		return maxlen;

	maxlen -= SOST - f;
	do
	{
		p += SOST;
		r = has_nul_byte(*(const size_t *)p);
		if(maxlen <= SOST)
			break;
		maxlen -= SOST;
	} while(!r);
	k = SOST - (ssize_t)maxlen;
	k = k > 0 ? k : 0;
	if(!HOST_IS_BIGENDIAN) {
		r <<= k * BITS_PER_CHAR;
		r >>= k * BITS_PER_CHAR;
	} else {
		r >>= k * BITS_PER_CHAR;
		r <<= k * BITS_PER_CHAR;
	}
	if(likely(r))
		r = nul_byte_index(r);
	else
		r = maxlen;
	return p - s + r;
}

static char const rcsid_snl[] GCC_ATTR_USED_VAR = "$Id: $";
