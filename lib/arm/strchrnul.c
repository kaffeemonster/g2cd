/*
 * strchrnul.c
 * strchrnul for non-GNU platforms, arm implementation
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
char *strchrnul(const char *s, int c)
{
	const char *p;
	size_t r, mask, x;
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
	r |= alu_ucmp_eqz_msk(x) & ACMP_MSK;
	if(!HOST_IS_BIGENDIAN)
		r >>= shift + ACMP_SHR;
	else
		r <<= shift + ACMP_SHL;
	if(r)
		return (char *)(uintptr_t)s + arm_nul_byte_index_b(r);

	do
	{
		p += SOST;
		x  = *(const unsigned long *)p;
		r  = alu_ucmp_eqz_msk(x) & ACMP_MSK;
		x ^= mask;
		r |= alu_ucmp_eqz_msk(x) & ACMP_MSK;
	} while(!r);
	r = arm_nul_byte_index_e(r);
	return (char *)(uintptr_t)p + r;
}

static char const rcsid_scnar[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/strchrnul.c"
#endif
/* EOF */
