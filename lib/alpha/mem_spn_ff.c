/*
 * mem_spn_ff.c
 * count 0xff span length, alpha implementation
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

/*
 * Alpha has a hard time with byte access (at least older
 * models, and "generic" compiles compatible with old ones),
 * so help Alpha by doing the end mask stuff instead of the
 * generic "test the trailer byte wise".
 */
size_t mem_spn_ff(const void *s, size_t len)
{
	const unsigned char *p;
	unsigned long c, ft;
	ssize_t f, k;
	prefetch(s);

	if(unlikely(!len))
		return 0;

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
	k = SOUL - f - (ssize_t)len;

	p  = (const unsigned char *)ALIGN_DOWN(s, SOUL);
	c = *(const unsigned long *)p;
	if(!HOST_IS_BIGENDIAN)
		c |= (~0ul) >> ((SOUL - f) * BITS_PER_CHAR);
	else
		c |= (~0ul) << ((SOUL - f) * BITS_PER_CHAR);
	if(unlikely(0 > k)) {
		k += f + len;
		goto K_SHIFT;
	}

	k = -k;
	if(likely(0xFFFFFFFFFFFFFFFFul == c))
	{
		for(p += SOUL, k -= SOUL; k >= (ssize_t)SOUL;
		    p += SOUL, k -= SOUL) {
			c = *(const unsigned long *)p;
			if(0xFFFFFFFFFFFFFFFFul != c)
				goto OUT;
		}
		if(0 >= k)
			return len;
		c = *(const unsigned long *)p;
		k = SOUL - k;
K_SHIFT:
		if(!HOST_IS_BIGENDIAN)
			c |= (~0ul) << ((SOUL - k) * BITS_PER_CHAR);
		else
			c |= (~0ul) >> ((SOUL - k) * BITS_PER_CHAR);
		if(0xFFFFFFFFFFFFFFFFul == c)
			return len;
	}

OUT:
	ft = cmpbeqz(c ^ 0xFFFFFFFFFFFFFFFFul);
	return (size_t)(p - (const unsigned char *)s) + alpha_nul_byte_index_e(ft);
}

static char const rcsid_msfa[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
