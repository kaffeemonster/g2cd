/*
 * mem_searchrn.c
 * search mem for a \r\n, alpha implementation
 *
 * Copyright (c) 2010-2012 Jan Seiffert
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

#include "alpha.h"

void *mem_searchrn(void *s, size_t len)
{
	char *p;
	unsigned long rr, rn, last_rr = 0;
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
	f = ALIGN_DOWN_DIFF(s, SOUL);
	k = SOUL - f - (ssize_t) len;
	k = k > 0 ? k : 0;

	p  = (char *)ALIGN_DOWN(s, SOUL);
	rn = *(unsigned long *)p;
	rr = cmpbeqm(rn, 0x0D0D0D0D0D0D0D0DUL); /* \r\r\r\r */
	if(!HOST_IS_BIGENDIAN) {
		rr <<= k + SOULM1 * BITS_PER_CHAR;
		rr >>= k + SOULM1 * BITS_PER_CHAR;
		rr >>= f;
		rr <<= f;
	} else {
		rr >>= k;
		rr <<= k;
		rr <<= f + SOULM1 * BITS_PER_CHAR;
		rr >>= f + SOULM1 * BITS_PER_CHAR;
	}
	if(unlikely(rr))
	{
		if(!HOST_IS_BIGENDIAN)
			last_rr = rr >> SOULM1;
		else
			last_rr = rr << SOULM1;
		rn  = cmpbeqm(rn, 0x0A0A0A0A0A0A0A0AUL); /* \n\n\n\n */
		if(!HOST_IS_BIGENDIAN)
			rr &= rn >> 1;
		else
			rr &= rn << 1;
		if(rr)
			return p + alpha_nul_byte_index_e(rr);
	}
	if(unlikely(k))
		return NULL;

	len -= SOUL - f;
	do
	{
		p += SOUL;
		rn = *(unsigned long *)p;
		rr = cmpbeqm(rn, 0x0D0D0D0D0D0D0D0DUL); /* \r\r\r\r */
		if(unlikely(len <= SOUL))
			break;
		len -= SOUL;
		if(rr || last_rr)
		{
			rn = cmpbeqm(rn, 0x0A0A0A0A0A0A0A0AUL); /* \n\n\n\n */
			last_rr &= rn;
			if(last_rr)
				return p - 1;
			if(!HOST_IS_BIGENDIAN) {
				last_rr = rr >> SOULM1;
				rr &= rn >> 1;
			} else {
				last_rr = rr << SOULM1;
				rr &= rn << 1;
			}
			if(rr)
				return p + alpha_nul_byte_index_e(rr);
		}
	} while(1);
	k = SOUL - (ssize_t) len;
	k = k > 0 ? k : 0;
	if(!HOST_IS_BIGENDIAN) {
		rr <<= k + SOULM1 * BITS_PER_CHAR;
		rr >>= k + SOULM1 * BITS_PER_CHAR;
	} else {
		rr >>= k;
		rr <<= k;
	}
	if(rr || last_rr)
	{
		rn = cmpbeqm(rn, 0x0A0A0A0A0A0A0A0AUL); /* \n\n\n\n */
		last_rr &= rn;
		if(last_rr)
			return p - 1;
		if(!HOST_IS_BIGENDIAN)
			rr &= rn >> 1;
		else
			rr &= rn << 1;
		if(rr)
			return p + alpha_nul_byte_index_e(rr);
	}

	return NULL;
}

static char const rcsid_msrn[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
