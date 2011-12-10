/*
 * to_base16.c
 * convert binary string to hex, Tile impl.
 *
 * Copyright (c) 2011 Jan Seiffert
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

static char const rcsid_tb16ti[] GCC_ATTR_USED_VAR = "$Id: $";
#include "tile.h"
#ifdef __tilegx__
# define ONLY_REMAINDER
# define ARCH_NAME_SUFFIX tile
#endif

#include "../generic/to_base16.c"

#ifdef __tilegx__
unsigned char *to_base16(unsigned char *dst, const unsigned char *src, unsigned len)
{
	static const unsigned long vals[] = {
		0x3736353433323130UL,
		0x6665646362613938UL
	};

	for(; len >= SOST; len -= SOST, src += SOST)
	{
		size_t in_l, in_h;
		size_t t1, t2;

		in_l = get_unaligned((const size_t *)src);

		in_h = tile_shufflebytes(vals[0], vals[1], in_l >> 4);
		in_l = tile_shufflebytes(vals[0], vals[1], in_l);
		if(!HOST_IS_BIGENDIAN) {
			t1 = v1int_l(in_l, in_h);
			t2 = v1int_h(in_l, in_h);
		} else {
			t1 = v1int_h(in_h, in_l);
			t2 = v1int_l(in_h, in_l);
		}
		put_unaligned(t1, (size_t *)dst);
		dst += SOST;
		put_unaligned(t2, (size_t *)dst);
		dst += SOST;
	}
	if(len >= SO32)
	{
		size_t in_l, in_h;
		size_t t1;

		in_l = get_unaligned((const uint32_t *)src);

		in_h = tile_shufflebytes(vals[0], vals[1], in_l >> 4);
		in_l = tile_shufflebytes(vals[0], vals[1], in_l);
		if(!HOST_IS_BIGENDIAN)
			t1 = v1int_l(in_l, in_h);
		else
			t1 = v1int_l(in_h, in_l);
		put_unaligned(t1, (size_t *)dst);
		dst += SOST;
	}
	return to_base16_generic(dst, src, len);
}
#endif
/* EOF */
