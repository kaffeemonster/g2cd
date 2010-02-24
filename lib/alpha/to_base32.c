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
	d1  += 0x6161616161616161ULL;
	d1  -= zapnot(0x4949494949494949ULL, cmpbge(d1, 0x7B7B7B7B7B7B7B7BULL));
	/* write out */
	put_unaligned_be64(d1, dst);
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
