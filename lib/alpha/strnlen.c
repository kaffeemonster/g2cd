/*
 * strnlen.c
 * strnlen for non-GNU platforms, alpha implementation
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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

size_t strnlen(const char *s, size_t maxlen)
{
	const char *p;
	unsigned long r;
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
	f = ALIGN_DOWN_DIFF(s, SOUL);
	k = SOUL - f - (long)maxlen;
	k = k > 0 ? k  : 0;

	p = (const char *)ALIGN_DOWN(s, SOUL);
	r = cmpbeqz(*(const unsigned long *)p);
	if(!HOST_IS_BIGENDIAN) {
		r <<= k     + SOULM1 * BITS_PER_CHAR;
		r >>= k + f + SOULM1 * BITS_PER_CHAR;
	} else {
		r >>= k;
		r <<= k + f + SOULM1 * BITS_PER_CHAR;
	}
	if(r)
		return alpha_nul_byte_index_b(r);
	else if(k)
		return maxlen;

	maxlen -= SOST - f;
	do
	{
		p += SOST;
		r = cmpbeqz(*(const unsigned long *)p);
		if(maxlen <= SOUL)
			break;
		maxlen -= SOUL;
	} while(!r);
	k = SOUL - (long)maxlen;
	k = k > 0 ? k : 0;
	if(!HOST_IS_BIGENDIAN) {
		r <<= k + SOULM1 * BITS_PER_CHAR;
		r >>= k + SOULM1 * BITS_PER_CHAR;
	} else {
		r >>= k;
		r <<= k + SOULM1 * BITS_PER_CHAR;
	}
	if(likely(r))
		r = alpha_nul_byte_index_b(r);
	else
		r = maxlen;
	return p - s + r;
}

static char const rcsid_snla[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
