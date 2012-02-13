/*
 * strrchr.c
 * strrchr, arm implementation
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


#include "my_neon.h"
#if defined(ARM_DSP_SANE)
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
	l_match.m = 0;

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
	if(m) {
		r = arm_nul_byte_index_e(r);
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
			return (char *)(uintptr_t)p + arm_nul_byte_index_e_last(m);
	}
	if(l_match.m)
		return (char *)(uintptr_t)l_match.p + arm_nul_byte_index_e_last(l_match.m);
	return NULL;
}

static char const rcsid_srcar[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/strrchr.c"
#endif
/* EOF */
