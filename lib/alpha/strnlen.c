/*
 * strnlen.c
 * strnlen for non-GNU platforms, alpha implementation
 *
 * Copyright (c) 2009-2011 Jan Seiffert
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

/*
 * Alpha is not very happy with the conditional branches
 * the str*N*.. introduces into the inner loop. So unroll
 * the inner loop a little.
 *
 * This way we beat glibc (on EV68, long == 316316 char,
 * short == 261 char):
 *	>= EV56 glibc strnlen
 *		10000 * long     t: 3655 ms
 *		10000000 * short t: 1639 ms
 *	< EV67 strnlen
 *		10000 * long     t: 1939 ms
 *		10000000 * short t: 978 ms
 *	>= EV67 strnlen
 *		10000 * long     t: 1922 ms
 *		10000000 * short t: 761 ms
 */

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

	maxlen -= SOUL - f;
	if(maxlen >= 4*SOUL)
	{
		do
		{
			maxlen -= 4 * SOUL;
			r = ((const unsigned long *)p)[1];
			if((r = cmpbeqz(r))) {
				p += SOUL;
				goto OUT;
			}
			r = ((const unsigned long *)p)[2];
			if((r = cmpbeqz(r))) {
				p += 2*SOUL;
				goto OUT;
			}
			r = ((const unsigned long *)p)[3];
			if((r = cmpbeqz(r))) {
				p += 3*SOUL;
				goto OUT;
			}
			r = ((const unsigned long *)p)[4];
			p += 4*SOUL;
			if((r = cmpbeqz(r)))
				goto OUT;
		} while(maxlen >= 4*SOUL);
	}
	if(unlikely(!maxlen))
		return p + SOUL - s;
	do
	{
		p += SOUL;
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
OUT:
	if(likely(r))
		r = alpha_nul_byte_index_b(r);
	else
		r = maxlen;
	return p - s + r;
}

static char const rcsid_snla[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
