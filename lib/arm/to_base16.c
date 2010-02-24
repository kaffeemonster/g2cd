/*
 * to_base16.c
 * convert binary string to hex, arm impl.
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

# define ARCH_NAME_SUFFIX _generic
# define ONLY_REMAINDER
# include "my_neon.h"
static unsigned char *to_base16_generic(unsigned char *dst, const unsigned char *src, unsigned len);
# define SOUL (sizeof(unsigned long))

unsigned char *to_base16(unsigned char *dst, const unsigned char *src, unsigned len)
{
	static const unsigned long vals[] =
	{
		MK_C(0x0f0f0f0fUL),
		MK_C(0x30303030UL),
		MK_C(0x3A3A3A3AUL),
		MK_C(0x27272727UL),
	};

	for(; len >= SOUL; len -= SOUL, src += SOUL, dst += 2 * SOUL)
	{
		unsigned long in_l, in_h;
		unsigned long t1, t2, t3, t4;

		in_l  = get_unaligned((const unsigned long *)src);
		in_h  = (in_l & (~vals[0])) >> 4;
		in_l &= vals[0];
		in_h += vals[1];
		in_l += vals[1];
		in_h += alu_ucmp_gte_sel(in_h, vals[2], vals[3], 0);
		in_l += alu_ucmp_gte_sel(in_l, vals[2], vals[3], 0);

		if(!HOST_IS_BIGENDIAN)
		{
			/*
			 * on the one hand the compiler tends to genreate
			 * crazy instruction sequences with
			 * ux* himself, but when he should solve this
			 * "save the bytes intermidiated", he fails and
			 * creates a truckload of shifts and consumes registers,
			 * spill spill spill.
			 * Unfortunatly there are no real unpack/pack instructions
			 * This ux_/pkh stuff is near. Still, also with the legacy
			 * "unaligned stores are verboten!" situation, this is no
			 * fun/helpfull.
			 * To not get to deep into trouble, only help the compiler
			 * to not generate the worst code.
			 * Some straight half word stores are a good start...
			 */
			asm("uxtb16	%0, %1" : "=r" (t1) : "r" (in_h));
			asm("uxtb16	%0, %1" : "=r" (t3) : "r" (in_l));
			asm("uxtb16	%0, %1, ror #8" : "=r" (t2) : "r" (in_h));
			asm("uxtb16	%0, %1, ror #8" : "=r" (t4) : "r" (in_l));
#if 0
			/*
			 * when it is more common knowledge that arm >= v6 can do unaligned
			 * access.. we could reshuffle and write out with word stores
			 */
			asm("pkhbt	%0, %1, %2, lsl #16" : "=r" (in_h) : "r" (t1), "r" (t2));
			asm("pkhtb	%0, %1, %2, asr #16" : "=r" (in_l) : "r" (t2), "r" (t1));
			t1 = in_h; t2 = in_l;
			asm("pkhbt	%0, %1, %2, lsl #16" : "=r" (in_h) : "r" (t2), "r" (t3));
			asm("pkhtb	%0, %1, %2, asr #16" : "=r" (in_h) : "r" (t3), "r" (t2));
			t3 = in_h; t4 = in_l;
#endif
		}
		else
		{
			asm("uxtb16	%0, %1, ror #24" : "=r" (t3) : "r" (in_h));
			asm("uxtb16	%0, %1, ror #24" : "=r" (t1) : "r" (in_l));
			asm("uxtb16	%0, %1, ror #16" : "=r" (t4) : "r" (in_h));
			asm("uxtb16	%0, %1, ror #16" : "=r" (t2) : "r" (in_l));
		}
		t1 |= t3 << BITS_PER_CHAR;
		t2 |= t4 << BITS_PER_CHAR;
		dst[0] = (t1 & 0x000000ff);
		dst[1] = (t1 & 0x0000ff00) >>  8;
		dst[2] = (t2 & 0x000000ff);
		dst[3] = (t2 & 0x0000ff00) >>  8;
		dst[4] = (t1 & 0x00ff0000) >> 16;
		dst[5] = (t1 & 0xff000000) >> 24;
		dst[5] = (t2 & 0x00ff0000) >> 16;
		dst[6] = (t2 & 0xff000000) >> 24;
	}

	if(unlikely(len))
		return to_base16_generic(dst, src, len);
	return dst;
}

static char const rcsid_tb16ar[] GCC_ATTR_USED_VAR = "$Id:$";
#endif
#include "../generic/to_base16.c"
/* EOF */
