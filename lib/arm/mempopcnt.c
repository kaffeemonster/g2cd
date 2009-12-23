/*
 * mempopcnt.c
 * popcount a mem region, arm implementation
 *
 * Copyright (c) 2009 Jan Seiffert
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
 * $Id:$
 */

#if defined(__ARM_NEON__)
# include <arm_neon.h>
# include "my_neon.h"

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

#elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || \
      defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || \
      defined(__ARM_ARCH_7A__)

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

static inline size_t popcntb(size_t n)
{
	n -= (n & MK_C(0xaaaaaaaaL)) >> 1;
	n = ((n >> 2) & MK_C(0x33333333L)) + (n & MK_C(0x33333333L));
	n = ((n >> 4) + n) & MK_C(0x0f0f0f0fL);
	return n;
}

size_t mempopcnt(const void *s, size_t len)
{
	const size_t *p;
	size_t r;
	size_t sum = 0;
	unsigned shift = ALIGN_DOWN_DIFF(s, SOST);
	prefetch(s);

	p = (const size_t *)ALIGN_DOWN(s, SOST);
	r = *p;
	if(!HOST_IS_BIGENDIAN)
		r >>= shift * BITS_PER_CHAR;
	else
		r <<= shift * BITS_PER_CHAR;
	if(len >= SOST || len + shift >= SOST)
	{
		/*
		 * Sometimes you need a new perspective, like the altivec
		 * way of handling things.
		 * Lower address bits? Totaly overestimated.
		 *
		 * We don't precheck for alignment.
		 * Instead we "align hard", do one load "under the address",
		 * mask the excess info out and afterwards we are fine to go.
		 */
		p++;
		len -= SOST - shift;
		sum += popcountst_int1(r);

		while(len >= SOST * 4)
		{
			size_t sumb = 0;

			r    = len / (SOST * 4);
			r    = r > 7 ? 7 : r;
			len -= r * SOST * 4;
			for(; r; r--, p += 4) {
				sumb += popcntb(p[0]);
				sumb += popcntb(p[1]);
				sumb += popcntb(p[2]);
				sumb += popcntb(p[3]);
			}
			asm("usada8	%0, %1, %2, %3" : "=r" (sum) : "r" (sumb), "r" (0), "r" (sum));
		}
		if(len >= SOST * 2) {
			sum += popcountst_int2(p[0], p[1]);
			p += 2;
			len -= SOST * 2;
		}
		if(len >= SOST) {
			sum += popcountst_int1(p[0]);
			p++;
			len -= SOST;
		}
		if(len)
			r =*p;
	}
	if(len) {
		if(!HOST_IS_BIGENDIAN)
			r <<= (SOST - len) * BITS_PER_CHAR;
		else
			r >>= (SOST - len) * BITS_PER_CHAR;
		sum += popcountst_int1(r);
	}
	return sum;
}

static char const rcsid_mpa[] GCC_ATTR_USED_VAR = "$Id:$";
#else
# include "../generic/mempopcnt.c"
#endif

