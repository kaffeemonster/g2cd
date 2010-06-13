/*
 * memchr.c
 * memchr, alpha implementation
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

#include "alpha.h"

void *my_memchr(const void *s, int c, size_t n)
{
	const unsigned char *p;
	unsigned long r, mask;
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
	f = ALIGN_DOWN_DIFF((const unsigned char *)s, SOUL);
	k = SOUL - f - (long)n;
	k = k > 0 ? k  : 0;
	mask = (c & 0xFF) * 0x0101010101010101UL;

	p  = (const unsigned char *)ALIGN_DOWN(s, SOUL);
	r  = *(const unsigned long *)p;
	r ^= mask;
	r  = cmpbeqz(r);
	if(!HOST_IS_BIGENDIAN) {
		r <<= k     + SOULM1 * BITS_PER_CHAR;
		r >>= k + f + SOULM1 * BITS_PER_CHAR;
	} else {
		r >>= k;
		r <<= k + f + SOULM1 * BITS_PER_CHAR;
	}
	if(r)
		return (char *)(uintptr_t)s + alpha_nul_byte_index_b(r);
	else if(k)
		return NULL;

	n -= SOST - f;
	do
	{
		p += SOST;
		r  = *(const unsigned long *)p;
		r ^= mask;
		r  = cmpbeqz(r);
		if(n <= SOUL)
			break;
		n -= SOUL;
	} while(!r);
	k = SOUL - (long)n;
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
		return NULL;
	return (char *)(uintptr_t)p + r;
}

static char const rcsid_mca[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
