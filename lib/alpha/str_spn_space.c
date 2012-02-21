/*
 * str_spn_space.c
 * count white space span length, alpha implementation
 *
 * Copyright (c) 2012 Jan Seiffert
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

#include "alpha.h"
/*
 * alpha without bwx does not gain anything with the pretest execpt pain
 * and has the cmpbge, everythings good
 */
#define STR_SPN_NO_PRETEST

static inline size_t str_spn_space_slow(const char *str)
{
	const char *p;
	unsigned long n, r, mask, x;
	unsigned shift;

	/* align input data */
	p = (const char *)ALIGN_DOWN(str, SOUL);
	shift = ALIGN_DOWN_DIFF(str, SOUL);
	x = *(const unsigned long *)p;

	/* find Nul-bytes */
	n  = cmpbeqz(x);
	/* find space */
	r  = cmpbeqm(x, 0x2020202020202020UL);
	/* add everything between backspace and shift out */
	r |= cmpb_between(x, 0x08ul, 0x0eul);
	if(!HOST_IS_BIGENDIAN) {
		n >>= shift;
		r >>= shift;
		mask = 0xfful >> shift;
	} else {
		n <<= shift + SOULM1 * BITS_PER_CHAR;
		r <<= shift + SOULM1 * BITS_PER_CHAR;
		mask = 0xfful << (shift + SOULM1 * BITS_PER_CHAR);
	}
	if(likely(n || mask != r)) {
		/* invert whitespace match to single out first non white space */
		r ^= mask;
		/* add Nul-byte mask on top */
		r |= n;
		return alpha_nul_byte_index_b(r);
	}

	do
	{
		p += SOUL;
		x  = *(const unsigned long *)p;
		n  = cmpbeqz(x);
		r  = cmpbeqm(x, 0x2020202020202020UL);
		r |= cmpb_between(x, 0x08, 0x0e);
		/* as long as we didn't hit a Nul-byte and all bytes are whitespace */
	} while(!n && 0xfful == r);
	/* invert whitespace match to single out first non white space */
	r ^= 0xfful;
	/* add Nul-byte mask on top */
	r |= n;
	return (p + alpha_nul_byte_index_e(r)) - str;
}


static char const rcsid_snccaa[] GCC_ATTR_USED_VAR = "$Id: $";
