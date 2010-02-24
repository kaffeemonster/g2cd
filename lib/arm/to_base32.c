/*
 * to_base32.c
 * convert binary string to base32, arm impl.
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

/*
 * Thanks go out to my brother for the idea how to group
 * those 5bit quantities.
 */


#if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || \
      defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || \
      defined(__ARM_ARCH_7A__)

# define HAVE_DO_40BIT
# include "my_neon.h"

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
#endif
/*
 * since arm has trouble with unaligned access, we depend
 * on this switch to lower the unaligned work (only load
 * 5 byte and shuffle them instead of 8)
 */
#define ONLY_REMAINDER

#include "../generic/to_base32.c"
/* EOF */
