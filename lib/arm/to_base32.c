/*
 * to_base32.c
 * convert binary string to base32, arm impl.
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

/*
 * Thanks go out to my brother for the idea how to group
 * those 5bit quantities.
 */


#include "my_neon.h"
#if defined(ARM_NEON_SANE)
static unsigned char *do_80bit(unsigned char *dst, const unsigned char *src)
{
	uint64x2_t a64, b64;
	uint64x2_t v_ffffffff;
	uint32x4_t a32, b32;
	uint32x4_t v_ffff;
	uint16x8_t a16, b16;
	uint16x8_t v_ff;
	uint8x16_t a1, b1;
	uint8x16_t v_61, v_49, v_7b, v_1f;

	v_61 = (uint8x16_t){0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61};
	v_49 = (uint8x16_t){0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49};
	v_7b = (uint8x16_t){0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b};
	v_1f = (uint8x16_t){0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f};
	v_ff = (uint16x8_t){0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
	v_ffff = (uint32x4_t){0xffff,0xffff,0xffff,0xffff};
	v_ffffffff = (uint64x2_t){0xffffffff,0xffffffff};

	a1  = vrev64q_u8(vld1q_u8(src));
	asm("vswp %e0, %f0" : "=w" (a1) : "0" (a1));
	b1  = vextq_u8(a1, a1, 5);                                        /* shift copy */
	a64 = (uint64x2_t)vcombine_u8(vget_high_u8(a1), vget_low_u8(b1)); /* eliminate & join */
	b64 = vshrq_n_u64(a64, 12);                                       /* shift copy */
	a32 = (uint32x4_t)vbslq_u64(v_ffffffff, b64, a64);                /* eliminate & join */
	b32 = vshrq_n_u32(a32, 6);                                        /* shift copy */
	a16 = (uint16x8_t)vbslq_u32(v_ffff, b32, a32);                    /* eliminate & join */
	b16 = vshrq_n_u16(a16, 3);                                        /* shift copy */
	a16 = vbslq_u16(v_ff, b16, a16);                                  /* eliminate && join */
	a1  = (uint8x16_t)vshrq_n_u16(a16, 3);                            /* bring bits down */
	a1  = vandq_u8(a1, v_1f);                                         /* eliminate */

	/* convert */
	a1 = vaddq_u8(a1, v_61);
	a1 = vsubq_u8(a1, vandq_u8(v_49, vcgeq_u8(a1, v_7b)));
	/* write out */
	vst1q_u8(dst, a1);
	return dst + SOVUCQ;
}

static unsigned char *do_40bit_int(unsigned char *dst, uint64x1_t a64)
{
	uint64x1_t b64;
	uint64x1_t v_ffffffff;
	uint32x2_t a32, b32;
	uint32x2_t v_ffff;
	uint16x4_t a16, b16;
	uint16x4_t v_ff;
	uint8x8_t a1;
	uint8x8_t v_61, v_49, v_7b, v_1f;

	v_61 = (uint8x8_t){0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61};
	v_49 = (uint8x8_t){0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49};
	v_7b = (uint8x8_t){0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b};
	v_1f = (uint8x8_t){0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f};
	v_ff = (uint16x4_t){0xff,0xff,0xff,0xff};
	v_ffff = (uint32x2_t){0xffff,0xffff};
	v_ffffffff = (uint64x1_t){0xffffffff};

	b64 = vshr_n_u64(a64, 12);                        /* shift copy */
	a32 = (uint32x2_t)vbsl_u64(v_ffffffff, b64, a64); /* eliminate & join */
	b32 = vshr_n_u32(a32, 6);                         /* shift copy */
	a16 = (uint16x4_t)vbsl_u32(v_ffff, b32, a32);     /* eliminate & join */
	b16 = vshr_n_u16(a16, 3);                         /* shift copy */
	a16 = vbsl_u16(v_ff, b16, a16);                   /* eliminate && join */
	a1  = (uint8x8_t)vshr_n_u16(a16, 3);              /* bring bits down */
	a1  = vand_u8(a1, v_1f);                          /* eliminate */

	/* convert */
	a1 = vadd_u8(a1, v_61);
	a1 = vsub_u8(a1, vand_u8(v_49, vcge_u8(a1, v_7b)));
	/* write out */
	vst1_u8(dst, a1);
	return dst + SOVUC;
}

static unsigned char *do_40bit(unsigned char *dst, const unsigned char *src)
{
	uint64x1_t a64, b64;
	uint64x1_t v_ffffffff;
	uint32x2_t a32, b32;
	uint32x2_t v_ffff;
	uint16x4_t a16, b16;
	uint16x4_t v_ff;
	uint8x8_t a1;
	uint8x8_t v_61, v_49, v_7b, v_1f;

	v_61 = (uint8x8_t){0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61};
	v_49 = (uint8x8_t){0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49};
	v_7b = (uint8x8_t){0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b,0x7b};
	v_1f = (uint8x8_t){0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f};
	v_ff = (uint16x4_t){0xff,0xff,0xff,0xff};
	v_ffff = (uint32x2_t){0xffff,0xffff};
	v_ffffffff = (uint64x1_t){0xffffffff};

	a64 = (uint64x1_t)vrev64_u8(vld1_u8(src));
	b64 = vshr_n_u64(a64, 12);                        /* shift copy */
	a32 = (uint32x2_t)vbsl_u64(v_ffffffff, b64, a64); /* eliminate & join */
	b32 = vshr_n_u32(a32, 6);                         /* shift copy */
	a16 = (uint16x4_t)vbsl_u32(v_ffff, b32, a32);     /* eliminate & join */
	b16 = vshr_n_u16(a16, 3);                         /* shift copy */
	a16 = vbsl_u16(v_ff, b16, a16);                   /* eliminate && join */
	a1  = (uint8x8_t)vshr_n_u16(a16, 3);              /* bring bits down */
	a1  = vand_u8(a1, v_1f);                          /* eliminate */

	/* convert */
	a1 = vadd_u8(a1, v_61);
	a1 = vsub_u8(a1, vand_u8(v_49, vcge_u8(a1, v_7b)));
	/* write out */
	vst1_u8(dst, a1);
	return dst + 8;
}

static inline uint32_t rol32(uint32_t word, unsigned int shift)
{
	return (word << shift) | (word >> (32 - shift));
}

unsigned char *to_base32(unsigned char *dst, const unsigned char *src, unsigned len)
{
	while(len >= SOVUCQ)
	{
		dst = do_80bit(dst, src);
		src += 10;
		len -= 10;
	}
	while(len >= SOVUC)
	{
		dst = do_40bit(dst, src);
		src += 5;
		len -= 5;
	}
	while(len >= 5)
	{
		uint64_t d = ((uint64_t)get_unaligned_be32(src) << 32) |
		             ((uint64_t)src[4] << 24);
		dst = do_40bit_int(dst, d);
		src += 5;
		len -= 5;
	}
	/* less than 32 bit left */
	if(len)
	{
		static const unsigned char base32c[] = "abcdefghijklmnopqrstuvwxyz234567=";
		unsigned b32chars = B32_LEN(len);
		uint32_t d;
		unsigned i;

		/* collect the bytes */
		for(i = len, d = 0; i; i--)
			d = (d << 8) | *src++;

		/* bring to start position */
		d = rol32(d, (sizeof(d) - len) * BITS_PER_CHAR + 5);
		i = 0;
		/* write out */
		do
		{
			*dst++ = base32c[d & 0x1F];
			d = rol32(d, 5);
		} while(++i < b32chars);
	}
	return dst;
}

static char const rcsid_tb32arn[] GCC_ATTR_USED_VAR = "$Id:$";
#else
# if defined(ARM_DSP_SANE)
#  define HAVE_DO_40BIT
static unsigned char *do_40bit(unsigned char *dst, uint64_t d1)
{
	uint64_t d2;
	uint32_t a1, a2;
	uint32_t b1, b2;
	uint32_t m1, m2;

	d2 = d1;                                   /* copy */
	d2 >>= 12;                                 /* shift copy */
	a1   = (d1 & 0xFFFFFFFF00000000ULL) >> 32; /* split it */
	a2   =  d2 & 0x00000000FFFFFFFFULL;
	b1   = a1;           b2   = a2;            /* copy */
	b1 >>= 6;            b2 >>= 6;             /* shift copy */
	b1  &= 0x0000FFFFUL; a1  &= 0xFFFF0000UL;  /* eliminate */
	b2  &= 0x0000FFFFUL; a2  &= 0xFFFF0000UL;
	a1  |= b1;           a2  |= b2;            /* join */
	b1   = a1;           b2   = a2;            /* copy */
	b1 >>= 3;            b2 >>= 3;             /* shift copy */
	b1  &= 0x00FF00FFUL; a1  &= 0xFF00FF00UL;  /* eliminate */
	b2  &= 0x00FF00FFUL; a2  &= 0xFF00FF00UL;
	a1  |= b1;           a2  |= b2;            /* join */
	a1 >>= 3;            a2 >>= 3;             /* bring bits down */
	a1  &= 0x1F1F1F1FUL; a2  &= 0x1F1F1F1FUL;  /* eliminate */

	/* convert */
	a1  += 0x61616161UL;          a2  += 0x61616161UL;
	m1   = alu_ucmp_gte_sel(a1, 0x7B7B7B7BUL, 0x49494949UL, 0);
	m2   = alu_ucmp_gte_sel(a2, 0x7B7B7B7BUL, 0x49494949UL, 0);
	a1  -= m1;                    a2 -= m2;
	/* write out */
	dst[7] = (a2 & 0x000000ff);
	a2 >>= BITS_PER_CHAR;
	dst[6] = (a2 & 0x000000ff);
	a2 >>= BITS_PER_CHAR;
	dst[5] = (a2 & 0x000000ff);
	a2 >>= BITS_PER_CHAR;
	dst[4] = (a2 & 0x000000ff);
	dst[3] = (a1 & 0x000000ff);
	a1 >>= BITS_PER_CHAR;
	dst[2] = (a1 & 0x000000ff);
	a1 >>= BITS_PER_CHAR;
	dst[1] = (a1 & 0x000000ff);
	a1 >>= BITS_PER_CHAR;
	dst[0] = (a1 & 0x000000ff);
	return dst + 8;
}

static char const rcsid_tb32ar[] GCC_ATTR_USED_VAR = "$Id:$";
# endif
/*
 * since arm has trouble with unaligned access, we depend
 * on this switch to lower the unaligned work (only load
 * 5 byte and shuffle them instead of 8)
 */
# define ONLY_REMAINDER

# include "../generic/to_base32.c"
#endif
/* EOF */
