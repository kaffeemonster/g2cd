/*
 * strrchr.c
 * strrchr, arm implementation
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
/*
 * We could first do a strlen to find the end of the string,
 * then search backwards. But since we have to walk the string
 * to find the end anyway we could pick up the matches on the
 * way while we are at it.
 */
char *strrchr(const char *s, int c)
{
	const char *p;
	size_t r, m, mask, x;
	struct {
		const char *p;
		size_t m;
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
	mask = (c & 0xFF) * MK_C(0x01010101UL);
	p  = (const char *)ALIGN_DOWN(s, SOST);
	shift = ALIGN_DOWN_DIFF(s, SOST);
	x  = *(const size_t *)p;
	r  = alu_ucmp_eqz_msk(x) & ACMP_MSK;
	x ^= mask;
	m  = alu_ucmp_eqz_msk(x) & ACMP_MSK;
	if(!HOST_IS_BIGENDIAN) {
		r >>= shift + ACMP_SHR;
		m >>= shift + ACMP_SHR;
		r <<= shift + ACMP_SHR;
		m <<= shift + ACMP_SHR;
	} else {
		r <<= shift + ACMP_SHL;
		m <<= shift + ACMP_SHL;
		r >>= shift + ACMP_SHL;
		m >>= shift + ACMP_SHL;
	}
	l_match.p = p;
	l_match.m = m;

	while(!r)
	{
		if(m) {
			l_match.p = p;
			l_match.m = m;
		}
		p += SOST;
		x  = *(const unsigned long *)p;
		r  = alu_ucmp_eqz_msk(x) & ACMP_MSK;
		x ^= mask;
		m  = alu_ucmp_eqz_msk(x) & ACMP_MSK;
	}
	if(unlikely(!(m || l_match.m)))
		return NULL;
	if(m) {
		r = arm_nul_byte_index_e(r);
		m = arm_nul_byte_index_e(m);
		if(m < r)
			return (char *)(uintptr_t)p + m;
	}
	m = arm_nul_byte_index_e(l_match.m);
	return (char *)(uintptr_t)l_match.p + m;
}

static char const rcsid_srcar[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/strrchr.c"
#endif
/* EOF */