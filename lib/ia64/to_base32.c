/*
 * to_base32.c
 * convert binary string to base32, IA64 impl.
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

/*
 * Thanks go out to my brother for the idea how to group
 * those 5bit quantities.
 */
#define HAVE_DO_40BIT
#include "ia64.h"

static unsigned char *do_40bit(unsigned char *dst, uint64_t d1)
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
	d2   = pcmp1gt(d1, 0x1919191919191919ULL); /* i hate signed compare */
	d1  += (0x6161616161616161ULL & ~d2) | (0x0606060606060606ULL & d2);
	/* write out */
	dst[0] = (d1 & 0xff00000000000000ULL) >> 56;
	dst[1] = (d1 & 0x00ff000000000000ULL) >> 48;
	dst[2] = (d1 & 0x0000ff0000000000ULL) >> 40;
	dst[3] = (d1 & 0x000000ff00000000ULL) >> 32;
	dst[4] = (d1 & 0x00000000ff000000ULL) >> 24;
	dst[5] = (d1 & 0x0000000000ff0000ULL) >> 16;
	dst[6] = (d1 & 0x000000000000ff00ULL) >>  8;
	dst[7] = (d1 & 0x00000000000000ffULL);
	return dst + 8;
}

static char const rcsid_tb32ia64[] GCC_ATTR_USED_VAR = "$Id:$";
/*
 * since ia64 has trouble with unaligned access, we depend
 * on this switch to lower the unaligned work (only load
 * 5 byte and shuffle them instead of 8)
 */
#define ONLY_REMAINDER

#include "../generic/to_base32.c"
/* EOF */
