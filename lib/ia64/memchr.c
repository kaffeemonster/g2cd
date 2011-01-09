/*
 * memchr.c
 * memchr, ia64 implementation
 *
 * Copyright (c) 2011 Jan Seiffert
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

void *my_memchr(const void *s, int c, size_t n)
{
	const unsigned char *p;
	unsigned long long r, mask;
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
	 */
	f = ALIGN_DOWN_DIFF(s, SOULL);
	k = SOULL - f - (long)n;
	k = k > 0 ? k  : 0;
	mask = (((size_t)c) & 0xFF) * 0x0101010101010101ULL;

	p  = (const unsigned char *)ALIGN_DOWN(s, SOULL);
	r  = *(const unsigned long long *)p;
	r ^= mask;
	if(!HOST_IS_BIGENDIAN) {
		r |= (~0ULL) >> ((SOULL - f) * BITS_PER_CHAR);
		r |= (~0ULL) << ((SOULL - k) * BITS_PER_CHAR);
	} else {
		r |= (~0ULL) << ((SOULL - f) * BITS_PER_CHAR);
		r |= (~0ULL) >> ((SOULL - k) * BITS_PER_CHAR);
	}
	r = czx1(r);
	if(r < SOULL)
		return (char *)(uintptr_t)p + r;
	else if(k)
		return NULL;

	n -= SOULL - f;
	do
	{
		p += SOULL;
		r  = *(const unsigned long long *)p;
		r ^= mask;
		if(n <= SOULL)
			break;
		r = czx1(r);
		if(r < SOULL)
			return (char *)(uintptr_t)p + r;
		n -= SOULL;
	} while(r == SOULL);
	k = SOULL - (long)n;
	k = k > 0 ? k : 0;
	if(!HOST_IS_BIGENDIAN)
		r |= (~0ULL) << ((SOULL - k) * BITS_PER_CHAR);
	else
		r |= (~0ULL) >> ((SOULL - k) * BITS_PER_CHAR);
	r = czx1(r);
	if(SOULL == r)
		return NULL;
	return (char *)(uintptr_t)p + r;
}

static char const rcsid_mcia64[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
