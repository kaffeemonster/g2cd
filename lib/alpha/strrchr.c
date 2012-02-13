/*
 * strrchr.c
 * strrchr, alpha implementation
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
/*
 * We could first do a strlen to find the end of the string,
 * then search backwards. But since we have to walk the string
 * to find the end anyway we could pick up the matches on the
 * way while we are at it.
 */
char *strrchr(const char *s, int c)
{
	const char *p;
	unsigned long r, m, mask, x;
	struct {
		const char *p;
		unsigned long m;
	} l_match;
	unsigned shift;

	if(unlikely(!c))
		return (char *)(uintptr_t)s + strlen(s);
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
	mask = (c & 0xFF) * 0x0101010101010101UL;
	p  = (const char *)ALIGN_DOWN(s, SOUL);
	shift = ALIGN_DOWN_DIFF(s, SOUL);
	x  = *(const unsigned long *)p;
	r  = cmpbeqz(x);
	m  = cmpbeqm(x, mask);
	if(!HOST_IS_BIGENDIAN) {
		r >>= shift;
		m >>= shift;
		r <<= shift;
		m <<= shift;
	} else {
		r <<= shift + SOULM1 * BITS_PER_CHAR;
		m <<= shift + SOULM1 * BITS_PER_CHAR;
		r >>= shift + SOULM1 * BITS_PER_CHAR;
		m >>= shift + SOULM1 * BITS_PER_CHAR;
	}
	l_match.p = p;
	l_match.m = 0;

	while(!r)
	{
		if(m) {
			l_match.p = p;
			l_match.m = m;
		}
		p += SOUL;
		x  = *(const unsigned long *)p;
		r  = cmpbeqz(x);
		m  = cmpbeqm(x, mask);
	}
	if(m) {
		r = alpha_nul_byte_index_e(r);
		if(!HOST_IS_BIGENDIAN) {
			r  = 1u << ((r * BITS_PER_CHAR)+BITS_PER_CHAR-1u);
			r |= r - 1u;
		} else {
			r  = 1u << (((SOSTM1-r) * BITS_PER_CHAR)+BITS_PER_CHAR-1u);
			r |= r - 1u;
			r  = ~r;
		}
		m &= r;
		if(m)
			return (char *)(uintptr_t)p + alpha_nul_byte_index_e_last(m);
	}
	if(l_match.m)
		return (char *)(uintptr_t)l_match.p + alpha_nul_byte_index_e_last(l_match.m);
	return NULL;
}

static char const rcsid_srca[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
