/*
 * to_base32.c
 * convert binary string to base32, alpha impl.
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
#define HAVE_DO_40BIT
#include "alpha.h"

#define TCINUL (SOUL / sizeof(tchar_t))

static tchar_t *do_40bit(tchar_t *dst, uint64_t d1)
{
	uint64_t d2;

	d2   = d1;                    /* copy */
	d2 >>= 12;                    /* shift copy */
	d1  &= 0xFFFFFFFF00000000ULL; /* eliminate */
	d2  &= 0x00000000FFFFFFFFULL;
	d1  |= d2;                    /* join */
	d2   = d1;                    /* copy */
	d2 >>= 6;                     /* shift copy */
	d1  &= 0xFFFF0000FFFF0000ULL; /* eliminate */
 	d2  &= 0x0000FFFF0000FFFFULL;
	d1  |= d2;                    /* join */
	d2   = d1;                    /* copy */
	d2 >>= 3;                     /* shift copy */
	d1  &= 0xFF00FF00FF00FF00ULL; /* eliminate */
	d2  &= 0x00FF00FF00FF00FFULL;
	d1  |= d2;                    /* join */
	d1 >>= 3;                     /* bring it down */
	d1  &= 0x1F1F1F1F1F1F1F1FULL; /* eliminate */

	/* convert */
	d1  += 0x4141414141414141ULL;
	d1  -= zapnot(0x2929292929292929ULL, cmpbge(d1, 0x5B5B5B5B5B5B5B5BULL));
	/* write out */
#if defined(__alpha_bwx__) || defined(__alpha_max__)
# if defined(__alpha_max__) && HOST_IS_BIGENDIAN != 0
	{
	uint64_t t1, t2;
	t1 = unpkbw(d1 >> 32);
	t2 = unpkbw(d1);
	put_unaligned(t1, (uint64_t *)(dst + 0 * TCINUL));
	put_unaligned(t2, (uint64_t *)(dst + 1 * TCINUL));
	}
# else
	dst[0] = (d1 & 0xff00000000000000ULL) >> 56;
	dst[1] = (d1 & 0x00ff000000000000ULL) >> 48;
	dst[2] = (d1 & 0x0000ff0000000000ULL) >> 40;
	dst[3] = (d1 & 0x000000ff00000000ULL) >> 32;
	dst[4] = (d1 & 0x00000000ff000000ULL) >> 24;
	dst[5] = (d1 & 0x0000000000ff0000ULL) >> 16;
	dst[6] = (d1 & 0x000000000000ff00ULL) >>  8;
	dst[7] = (d1 & 0x00000000000000ffULL);
# endif
#else
	{
	uint64_t o1, o2;
	if(!HOST_IS_BIGENDIAN)
	{
		o1  = (d1 & 0xff00000000000000ULL);
		o1 |= (d1 & 0x00ff000000000000ULL) >>  8;
		o1 |= (d1 & 0x0000ff0000000000ULL) >> 16;
		o1 |= (d1 & 0x000000ff00000000ULL) >> 24;
		o2  = (d1 & 0x00000000ff000000ULL) << 32;
		o2 |= (d1 & 0x0000000000ff0000ULL) << 24;
		o2 |= (d1 & 0x000000000000ff00ULL) << 16;
		o2 |= (d1 & 0x00000000000000ffULL) <<  8;
	}
	else
	{
		o1  = (d1 & 0xff00000000000000ULL) >>  8;
		o1 |= (d1 & 0x00ff000000000000ULL) >> 16;
		o1 |= (d1 & 0x0000ff0000000000ULL) >> 24;
		o1 |= (d1 & 0x000000ff00000000ULL) >> 32;
		o2  = (d1 & 0x00000000ff000000ULL) << 24;
		o2 |= (d1 & 0x0000000000ff0000ULL) << 16;
		o2 |= (d1 & 0x000000000000ff00ULL) <<  8;
		o2 |= (d1 & 0x00000000000000ffULL);
	}
	put_unaligned(o1, (uint64_t *)(dst + 0 * TCINUL));
	put_unaligned(o2, (uint64_t *)(dst + 1 * TCINUL));
	}
#endif
	return dst + 8;
}

static char const rcsid_tb32al[] GCC_ATTR_USED_VAR = "$Id:$";
/*
 * since alpha has trouble with unaligned access, we depend
 * on this switch to lower the unaligned work (only load
 * 5 byte and shuffle them instead of 8)
 */
#define ONLY_REMAINDER

#include "../generic/to_base32.c"
/* EOF */
