/*
 * strnlen.c
 * strnlen for non-GNU platforms, arm implementation
 *
 * Copyright (c) 2010-2011 Jan Seiffert
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

#if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || \
      defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || \
      defined(__ARM_ARCH_7A__)

# include "my_neon.h"

size_t strnlen(const char *s, size_t maxlen)
{
	const char *p;
	size_t r;
	ssize_t f, k;
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
	f = ALIGN_DOWN_DIFF(s, SOST);
	k = SOST - f - (ssize_t)maxlen;
	k = k > 0 ? k  : 0;

	p = (const char *)ALIGN_DOWN(s, SOST);
	r = alu_ucmp_eqz_msk(*(const size_t *)p) & ACMP_MSK;
	if(!HOST_IS_BIGENDIAN) {
		r <<= k     + ACMP_SHL;
		r >>= k + f + ACMP_SHL + ACMP_SHR;
	} else {
		r >>= k     + ACMP_SHR;
		r <<= k + f + ACMP_SHR + ACMP_SHL;
	}
	if(r)
		return arm_nul_byte_index_b(r);
	else if(k)
		return maxlen;

	maxlen -= SOST - f;
	if(unlikely(!maxlen))
		return p + SOST - s;
	do
	{
		p += SOST;
		r = alu_ucmp_eqz_msk(*(const size_t *)p) & ACMP_MSK;
		if(maxlen <= SOST)
			break;
		maxlen -= SOST;
	} while(!r);
	k = SOST - (ssize_t)maxlen;
	k = k > 0 ? k : 0;
	if(!HOST_IS_BIGENDIAN) {
		r <<= k + ACMP_SHL;
		r >>= k + ACMP_SHL + ACMP_SHR;
	} else {
		r >>= k + ACMP_SHR;
		r <<= k + ACMP_SHR + ACMP_SHL;
	}
	if(likely(r))
		r = arm_nul_byte_index_b(r);
	else
		r = maxlen;
	return p - s + r;
}

static char const rcsid_snlar[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/strnlen.c"
#endif
/* EOF */
