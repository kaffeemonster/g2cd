/*
 * mempopcnt.c
 * popcount a mem region, arm implementation
 *
 * Copyright (c) 2009-2012 Jan Seiffert
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
 * $Id:$
 */

#include "my_neon.h"
#if defined(ARM_NEON_SANE)
# include <arm_neon.h>

size_t mempopcnt(const void *s, size_t len)
{
	uint8x16_t v_0;
	uint8x16_t c;
	uint32x4_t v_sum;
	uint32x2_t v_tsum;
	unsigned char *p;
	size_t r;
	unsigned shift;

	prefetch(s);

// TODO: do this in 64 bit? the mem model seems more that way...
	v_0   = (uint8x16_t){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	v_sum = (uint32x4_t)v_0;
	p = (unsigned char *)ALIGN_DOWN(s, SOVUCQ);
	shift = ALIGN_DOWN_DIFF(s, SOVUCQ);
	c = *(const uint8x16_t *)p;
	if(HOST_IS_BIGENDIAN)
		c = neon_simple_alignq(v_0, c, SOVUCQ - shift);
	else
		c = neon_simple_alignq(c, v_0, shift);
	if(len >= SOVUCQ || len + shift >= SOVUCQ)
	{
		p    += SOVUCQ;
		len  -= SOVUCQ - shift;
		v_sum = vpadalq_u16(v_sum, vpaddlq_u8(vcntq_u8(c)));

		while(len >= SOVUCQ * 2)
		{
			uint8x16_t v_sumb = v_0;

			r    = len / (SOVUCQ * 2);
			r    = r > 15 ? 15 : r;
			len -= r * SOVUCQ * 2;
			/*
			 * NEON has a vector popcnt instruction, so no compression.
			 * We trust the speed given in the handbook (adding more
			 * instructions would not make it faster), 1-2 cycles.
			 */
			for(; r; r--, p += SOVUCQ * 2) {
				c      = *(const uint8x16_t *)p;
				v_sumb = vaddq_u8(v_sumb, vcntq_u8(c));
				c      = *((const uint8x16_t *)(p + SOVUCQ));
				v_sumb = vaddq_u8(v_sumb, vcntq_u8(c));
			}
			v_sum = vpadalq_u16(v_sum, vpaddlq_u8(v_sumb));
		}
		if(len >= SOVUCQ) {
			c     = *(const uint8x16_t *)p;
			p    += SOVUCQ;
			v_sum = vpadalq_u16(v_sum, vpaddlq_u8(vcntq_u8(c)));
			len  -= SOVUCQ;
		}

		if(len)
			c = *(const uint8x16_t *)p;
	}
	if(len) {
		if(HOST_IS_BIGENDIAN)
			c      = neon_simple_alignq(c, v_0, SOVUCQ - len);
		else
			c      = neon_simple_alignq(v_0, c, len);
		v_sum  = vpadalq_u16(v_sum, vpaddlq_u8(vcntq_u8(c)));
	}

	v_tsum = vpadd_u32(vget_high_u32(v_sum), vget_low_u32(v_sum));
	v_tsum = vpadd_u32(v_tsum, v_tsum);
	return vget_lane_u32(v_tsum, 0);
}

static char const rcsid_mpa[] GCC_ATTR_USED_VAR = "$Id:$";
#else
# if defined(ARM_DSP_SANE)
static inline size_t popcountst_int1(size_t n)
{
	n -= (n & MK_C(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_C(0x33333333L)) + (n & MK_C(0x33333333L));
	n = ((n >> 4) + n) & MK_C(0x0f0f0f0fL);
	asm("usad8	%0, %1, %2" : "=r" (n) : "r" (n), "r" (0));
	return n;
}

static inline size_t popcountst_int2(size_t n, size_t m)
{
	n -= (n & MK_C(0xaaaaaaaaL)) >> 1;
	m -= (m & MK_C(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_C(0x33333333L)) + (n & MK_C(0x33333333L));
	m = ((m >> 2) & MK_C(0x33333333L)) + (m & MK_C(0x33333333L));
	n += m;
	n = ((n >> 4) & MK_C(0x0f0f0f0fL)) + (n & MK_C(0x0f0f0f0fL));
	asm("usad8	%0, %1, %2" : "=r" (n) : "r" (n), "r" (0));
	return n;
}

static inline size_t popcountst_int4(size_t n, size_t m, size_t o, size_t p)
{
	return popcountst_int2(n, m) + popcountst_int2(o, p);
}

static inline size_t popcountst_b(size_t n)
{
	n -= (n & MK_C(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_C(0x33333333L)) + (n & MK_C(0x33333333L));
	n = ((n >> 4) + n) & MK_C(0x0f0f0f0fL);
	return n;
}

static inline size_t sideways_add(size_t sum, size_t x)
{
	asm("usada8	%0, %1, %2, %3" : "=r" (sum) : "r" (x), "r" (0), "r" (sum));
	return sum;
}

static char const rcsid_mpa[] GCC_ATTR_USED_VAR = "$Id:$";
#  define NO_GEN_POPER
#  define NO_SIDEWAYS_ADD
# endif
# include "../generic/mempopcnt.c"
#endif
/* EOF */
