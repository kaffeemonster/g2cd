/*
 * str_spn_space.c
 * count white space span length, generic implementation
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

#ifdef STR_SPN_NO_PRETEST
# undef noinline
# define noinline
#endif

static noinline size_t str_spn_space_slow(const char *str)
{
	const char *p;
	size_t r, x;
	const size_t mask = MK_C(0x80808080ul);
	unsigned shift;

	/* align input data */
	p = (const char *)ALIGN_DOWN(str, SOST);
	shift = ALIGN_DOWN_DIFF(str, SOST) * BITS_PER_CHAR;
	x = *(const size_t *)p;
	if(!HOST_IS_BIGENDIAN)
		x >>= shift;
	/* find space */
	r  = has_eq_byte(x, MK_C(0x20202020ul));
	/* add everything between backspace and shift out */
	r |= has_between(x, 0x08, 0x0e);
	if(shift)
	{
		if(HOST_IS_BIGENDIAN) {
			r <<= shift;
			r = (r >> shift) | (mask << (SIZE_T_BITS - shift));
		} else
			r = (r << shift) | (mask >> (SIZE_T_BITS - shift));
	}


	/* as long as we didn't hit a Nul-byte and all bytes are whitespace */
	while(mask == (r & mask))
	{
		p += SOST;
		x = *(const size_t *)p;
		r  = has_eq_byte(x, MK_C(0x20202020ul));
		r |= has_between(x, 0x08, 0x0e);
	}
	/* invert whitespace match to single out first non white space */
	r  = (r & mask) ^ mask;
	return (p + nul_byte_index(r)) - str;
}

static char const rcsid_strspns[] GCC_ATTR_USED_VAR = "$Id: $";
