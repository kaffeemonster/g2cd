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

static tchar_t *do_40bit(tchar_t *dst, uint64_t d1)
{
	uint64_t d2;
	uint32_t a1, a2;
	uint32_t b1, b2;
	uint32_t m1, m2, t1, t2, t3, t4;

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
	a1  += 0x41414141UL;          a2  += 0x41414141UL;
	m1   = alu_ucmp_gte_sel(a1, 0x5B5B5B5BUL, 0x29292929UL, 0);
	m2   = alu_ucmp_gte_sel(a2, 0x5B5B5B5BUL, 0x29292929UL, 0);
	a1  -= m1;                    a2 -= m2;
	/* write out */
	/*
	 * on the one hand the compiler tends to genreate
	 * crazy instruction sequences with
	 * ux* himself, but when he should solve this, he
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
	asm("uxtb16	%0, %1, ror #24" : "=r" (t1) : "r" (a1));
	asm("uxtb16	%0, %1, ror #16" : "=r" (t2) : "r" (a1));
	asm("uxtb16	%0, %1, ror #24" : "=r" (t3) : "r" (a2));
	asm("uxtb16	%0, %1, ror #16" : "=r" (t4) : "r" (a2));
	dst[0] = (t1 & 0x0000ffff);
	dst[1] = (t2 & 0x0000ffff);
	dst[2] = (t1 & 0xffff0000) >> 16;
	dst[3] = (t2 & 0xffff0000) >> 16;
	dst[4] = (t3 & 0x0000ffff);
	dst[5] = (t4 & 0x0000ffff);
	dst[6] = (t3 & 0xffff0000) >> 16;
	dst[7] = (t4 & 0xffff0000) >> 16;
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
