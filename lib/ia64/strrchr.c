/*
 * strrchr.c
 * strrchr, ia64 implementation
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
/*
 * We could first do a strlen to find the end of the string,
 * then search backwards. But since we have to walk the string
 * to find the end anyway we could pick up the matches on the
 * way while we are at it.
 */
char *strrchr(const char *s, int c)
{
	const char *p, *l_match;
	unsigned long long r1, r2, l_r, mask, x1, x2;
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
	mask = (((size_t)c) & 0xFF) * 0x0101010101010101ULL;
	p  = (const char *)ALIGN_DOWN(s, SOULL);
	shift = ALIGN_DOWN_DIFF(s, SOULL);
	x1 = *(const unsigned long long *)p;
	x2 = x1 ^ mask;
	if(!HOST_IS_BIGENDIAN) {
		x1 |= (~0ULL) >> ((SOULL - shift) * BITS_PER_CHAR);
		x2 |= (~0ULL) >> ((SOULL - shift) * BITS_PER_CHAR);
	} else {
		x1 |= (~0ULL) << ((SOULL - shift) * BITS_PER_CHAR);
		x2 |= (~0ULL) << ((SOULL - shift) * BITS_PER_CHAR);
	}
	r1 = czx1(x1);
	r2 = czx1(x2);
	if(r1 < SOULL) {
		if(SOULL == r2 || r1 <= r2)
			return NULL;
		return (char *)(uintptr_t)p + r2;
	}
	l_match = p;
	l_r = r2;

	do
	{
		if(r2 < SOULL) {
			l_match = p;
			l_r = r2;
		}
		p += SOULL;
		x1 = *(const unsigned long long *)p;
		x2 = x1 ^ mask;
		r1 = czx1(x1);
		r2 = czx1(x2);
	} while(r1 == SOULL);
	if(r2 < SOULL && r2 < r1)
		return (char *)(uintptr_t)p + r2;
	if(l_r < SOULL)
		return (char *)(uintptr_t)l_match + l_r;
	return NULL;
}

static char const rcsid_srcia64[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
