/*
 * strnlen.c
 * strnlen for non-GNU platforms, ia64 implementation
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

#include "ia64.h"

size_t strnlen(const char *s, size_t maxlen)
{
	const char *p;
	unsigned long long r, t;
	long f, k;
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
	f = ALIGN_DOWN_DIFF(s, SOULL);
	k = SOULL - f - (long)maxlen;
	k = k > 0 ? k  : 0;

	p = (const char *)ALIGN_DOWN(s, SOULL);
	r = *(const unsigned long long *)p;
	if(!HOST_IS_BIGENDIAN) {
		r |= (~0ULL) >> ((SOULL - f) * BITS_PER_CHAR);
		r |= (~0ULL) << ((SOULL - k) * BITS_PER_CHAR);
	} else {
		r |= (~0ULL) << ((SOULL - f) * BITS_PER_CHAR);
		r |= (~0ULL) >> ((SOULL - k) * BITS_PER_CHAR);
	}
	r = czx1(r);
	if(r < SOULL)
		return r;
	else if(k)
		return maxlen;

	maxlen -= SOULL - f;
	do
	{
		p += SOULL;
		t = *(const unsigned long *)p;
		if(maxlen <= SOULL)
			break;
		r = czx1(t);
		if(r < SOULL)
			return p - s + r;
		maxlen -= SOULL;
	} while(r == SOULL);
	k = SOULL - (long)maxlen;
	k = k > 0 ? k : 0;
	if(!HOST_IS_BIGENDIAN)
		t |= (~0ULL) << ((SOULL - k) * BITS_PER_CHAR);
	else
		t |= (~0ULL) >> ((SOULL - k) * BITS_PER_CHAR);
	r = czx1(t);
	r = r < SOULL ? r : maxlen;
	return p - s + r;
}

static char const rcsid_snlia64[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
