/*
 * mem_searchrn.c
 * search mem for a \r\n, ia64 implementation
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

void *mem_searchrn(void *s, size_t len)
{
	char *p;
	unsigned long long rr, rn, last_rr = 0;
	ssize_t f, k;
	prefetch(s);

	if(unlikely(!s || !len))
		return NULL;
	/*
	 * Sometimes you need a new perspective, like the altivec
	 * way of handling things.
	 * Lower address bits? Totaly overestimated.
	 *
	 * We don't precheck for alignment, 8 or 4 is very unlikely
	 * instead we "align hard", do one load "under the address",
	 * mask the excess info out and afterwards we are fine to go.
	 *
	 * Even this beeing a mem* function, the len can be seen as a
	 * "hint". We can overread and underread, but should cut the
	 * result (and not pass a page boundery, but we cannot because
	 * we are aligned).
	 */
	f = ALIGN_DOWN_DIFF(s, SOULL);
	k = SOULL - f - (ssize_t) len;
	k = k > 0 ? k : 0;

	p  = (char *)ALIGN_DOWN(s, SOULL);
	rn = (*(unsigned long long *)p);
	rr = rn ^ 0x0D0D0D0D0D0D0D0DULL; /* \r\r\r\r */
	rr = pcmp1eq(rr, 0);
	if(!HOST_IS_BIGENDIAN) {
		rr <<= k * BITS_PER_CHAR;
		rr >>= k * BITS_PER_CHAR;
		rr >>= f * BITS_PER_CHAR;
		rr <<= f * BITS_PER_CHAR;
	} else {
		rr >>= k * BITS_PER_CHAR;
		rr <<= k * BITS_PER_CHAR;
		rr <<= f * BITS_PER_CHAR;
		rr >>= f * BITS_PER_CHAR;
	}
	if(unlikely(rr))
	{
		if(!HOST_IS_BIGENDIAN)
			last_rr = rr >> (SOULLM1 * BITS_PER_CHAR);
		else
			last_rr = rr << (SOULLM1 * BITS_PER_CHAR);
		rn ^= 0x0A0A0A0A0A0A0A0AULL; /* \n\n\n\n */
		rn  = pcmp1eq(rn, 0);
		if(!HOST_IS_BIGENDIAN)
			rr &= rn >> BITS_PER_CHAR;
		else
			rr &= rn << BITS_PER_CHAR;
		if(rr)
			return p + czx1(~rr);
	}
	if(unlikely(k))
		return NULL;

	len -= SOULL - f;
	do
	{
		p += SOULL;
		rn = *(unsigned long long *)p;
		rr = rn ^ 0x0D0D0D0D0D0D0D0DULL; /* \r\r\r\r */
		rr = pcmp1eq(rr, 0);
		if(unlikely(len <= SOULL))
			break;
		len -= SOULL;
		if(rr || last_rr)
		{
			rn ^= 0x0A0A0A0A0A0A0A0AULL; /* \n\n\n\n */
			rn  = pcmp1eq(rn, 0);
			last_rr &= rn;
			if(last_rr)
				return p - 1;
			if(!HOST_IS_BIGENDIAN) {
				last_rr = rr >> (SOULLM1 * BITS_PER_CHAR);
				rr &= rn >> BITS_PER_CHAR;
			} else {
				last_rr = rr << (SOULLM1 * BITS_PER_CHAR);
				rr &= rn << BITS_PER_CHAR;
			}
			if(rr)
				return p + czx1(~rr);
		}
	} while(1);
	k = SOULL - (ssize_t) len;
	k = k > 0 ? k : 0;
	if(!HOST_IS_BIGENDIAN) {
		rr <<= k * BITS_PER_CHAR;
		rr >>= k * BITS_PER_CHAR;
	} else {
		rr >>= k * BITS_PER_CHAR;
		rr <<= k * BITS_PER_CHAR;
	}
	if(rr || last_rr)
	{
		rn ^= 0x0A0A0A0A0A0A0A0AULL; /* \n\n\n\n */
		rn  = pcmp1eq(rn, 0);
		last_rr &= rn;
		if(last_rr)
			return p - 1;
		if(!HOST_IS_BIGENDIAN)
			rr &= rn >> BITS_PER_CHAR;
		else
			rr &= rn << BITS_PER_CHAR;
		if(rr)
			return p + czx1(~rr);
	}

	return NULL;
}

static char const rcsid_msrnia[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
