/*
 * to_base16.c
 * convert binary string to hex, arm impl.
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
#if defined(ARM_NEON_SANE)
# define ARCH_NAME_SUFFIX _generic
# define ONLY_REMAINDER
static unsigned char *to_base16_generic(unsigned char *dst, const unsigned char *src, unsigned len);

unsigned char *to_base16(unsigned char *dst, const unsigned char *src, unsigned len)
{
	uint8x16_t v_0f, v_30, v_3a, v_27;

	v_0f = (uint8x16_t){0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f};
	v_30 = (uint8x16_t){0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30};
	v_3a = (uint8x16_t){0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a};
	v_27 = (uint8x16_t){0x27,0x27,0x27,0x27,0x27,0x27,0x27,0x27,0x27,0x27,0x27,0x27,0x27,0x27,0x27,0x27};

	for(; len >= SOVUCQ; len -= SOVUCQ, src += SOVUCQ, dst += 2 * SOVUCQ)
	{
		register uint8x16_t in_l asm("q8");
		register uint8x16_t in_h asm("q9");

		in_l  = vld1q_u8(src);
		in_h  = vshrq_n_u8(vbicq_u8(in_l, v_0f), 4);
		in_l  = vandq_u8(in_l, v_0f);
		in_h  = vaddq_u8(in_h, v_30);
		in_l  = vaddq_u8(in_l, v_30);
		in_h  = vaddq_u8(in_h, vandq_u8(vcgeq_u8(in_h, v_3a), v_27));
		in_l  = vaddq_u8(in_l, vandq_u8(vcgeq_u8(in_l, v_3a), v_27));

		asm (
			"vst2.8 {%e1,%e2}, %0\n\t"
			: "=Q" (*dst)
			: "w" (in_l),
			  "w" (in_h)
		);
		asm (
			"vst2.8 {%f1,%f2}, %0\n\t"
			: "=Q" (*(dst+SOVUCQ))
			: "w" (in_l),
			  "w" (in_h)
		);
	}
	for(; len >= SOVUC; len -= SOVUC, src += SOVUC, dst += 2 * SOVUC)
	{
		uint8x8_t in_l;
		uint8x16_t in;

		in_l  = vld1_u8(src);
		in  = vcombine_u8(vand_u8(in_l, vget_low_u8(v_0f)), vshr_n_u8(vbic_u8(in_l, vget_low_u8(v_0f)), 4));
		in  = vaddq_u8(in, v_30);
		in  = vaddq_u8(in, vandq_u8(vcgeq_u8(in, v_3a), v_27));

		asm (
			"vst2.8 {%e1,%f1}, %0\n\t"
			: "=Q" (*dst)
			: "w" (in)
		);
	}

	if(unlikely(len))
		return to_base16_generic(dst, src, len);
	return dst;
}

static char const rcsid_tb16arn[] GCC_ATTR_USED_VAR = "$Id:$";

#elif defined(ARM_DSP_SANE)
# define ARCH_NAME_SUFFIX _generic
# define ONLY_REMAINDER
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

static char const rcsid_tb16ard[] GCC_ATTR_USED_VAR = "$Id:$";
#endif
#include "../generic/to_base16.c"
/* EOF */
