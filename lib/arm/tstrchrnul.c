/*
 * tstrchrnul.c
 * tstrchrnul, arm implementation
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

tchar_t *tstrchrnul(const tchar_t *s, tchar_t c)
{
	const char *p;
	size_t r, mask, x;
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
	mask = (((size_t)c) & 0xFFFF) * MK_C(0x00010001UL);
	p  = (const char *)ALIGN_DOWN(s, SOST);
	shift = ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR;
	x  = *(const size_t *)p;
	r = alu_ucmp16_eqz_msk(x) & ACMP_MSK;
	x ^= mask;
	r |= alu_ucmp16_eqz_msk(x) & ACMP_MSK;
	if(!HOST_IS_BIGENDIAN)
		r >>= shift + ACMP_SHR;
	else
		r <<= shift + ACMP_SHL;
	if(r)
		return ((tchar_t *)(uintptr_t)s) + arm_nul_byte_index_b(r) / 2;

	do
	{
		p += SOST;
		x  = *(const size_t *)p;
		r = alu_ucmp16_eqz_msk(x) & ACMP_MSK;
		x ^= mask;
		r |= alu_ucmp16_eqz_msk(x) & ACMP_MSK;
	} while(!r);
	r = arm_nul_byte_index_e(r) / 2;
	return ((tchar_t *)(uintptr_t)p) + r;
}

static char const rcsid_tscnar[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../genric/tstrchrnul.c"
#endif
/* EOF */
