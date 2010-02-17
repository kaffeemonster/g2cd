/*
 * strchrnul.c
 * strchrnul for non-GNU platforms, arm implementation
 *
 * Copyright (c) 2010 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 * $Id: $
 */


#if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || \
      defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || \
      defined(__ARM_ARCH_7A__)

# include "my_neon.h"

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
