/*
 * strlen.c
 * strlen, arm implementation
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
size_t strlen(const char *s)
{
	const char *p;
	unsigned long r;
	unsigned shift;
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
	p = (const char *)ALIGN_DOWN(s, SOST);
	shift = ALIGN_DOWN_DIFF(s, SOST);
	r = alu_ucmp_eqz_msk(*(const size_t *)p) & ACMP_MSK;
	if(!HOST_IS_BIGENDIAN)
		r >>= shift + ACMP_SHR;
	else
		r <<= shift + ACMP_SHL;
	if(r)
		return arm_nul_byte_index_b(r);

	do
	{
		p += SOST;
		r = alu_ucmp_eqz_msk(*(const size_t *)p) & ACMP_MSK;
	} while(!r);
	r = arm_nul_byte_index_e(r);
	return p - s + r;
}

static char const rcsid_slar[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/strlen.c"
#endif
/* EOF */
