/*
 * memchr.c
 * memchr, arm implementation
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

#if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || \
      defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || \
      defined(__ARM_ARCH_7A__)

# include "my_neon.h"

void *my_memchr(const void *s, int c, size_t n)
{
	const unsigned char *p;
	size_t r, mask;
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
	 */
	f = ALIGN_DOWN_DIFF((const unsigned char *)s, SOST);
	k = SOST - f - (ssize_t)n;
	k = k > 0 ? k  : 0;
	mask = (c & 0xFF) * MK_C(0x01010101UL);

	p  = (const unsigned char *)ALIGN_DOWN(s, SOST);
	r  = *(const size_t *)p;
	r ^= mask;
	r  = alu_ucmp_eqz_msk(r) & ACMP_MSK;
	if(!HOST_IS_BIGENDIAN) {
		r <<= k     + ACMP_SHL;
		r >>= k + f + ACMP_SHL + ACMP_SHR;
	} else {
		r >>= k + ACMP_SHR;
		r <<= k + f + ACMP_SHR + ACMP_SHL;
	}
	if(r)
		return (char *)(uintptr_t)s + arm_nul_byte_index_b(r);
	else if(k)
		return NULL;

	n -= SOST - f;
	do
	{
		p += SOST;
		r  = *(const size_t *)p;
		r ^= mask;
		r  = alu_ucmp_eqz_msk(r) & ACMP_MSK;
		if(n <= SOST)
			break;
		n -= SOST;
	} while(!r);
	k = SOST - (ssize_t)n;
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
		return NULL;
	return (char *)(uintptr_t)p + r;
}

static char const rcsid_mcar[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/memchr.c"
#endif
/* EOF */
