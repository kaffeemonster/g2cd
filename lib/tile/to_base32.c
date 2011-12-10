/*
 * to_base32.c
 * convert binary string to base32, Tile impl.
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

#include "tile.h"
#define ARCH_NAME_SUFFIX tile
#define ONLY_REMAINDER

#include "../generic/to_base32.c"

unsigned char *to_base32(unsigned char *dst, const unsigned char *src, unsigned len)
{
	while(len >= sizeof(uint64_t))
	{
		uint64_t d;
		if(IS_ALIGN(src, sizeof(unsigned long)))
			d = *(const uint64_t *)src;
		else
		{
#ifdef __tilegx__
			uint64_t t;
			d = tile_ldna(src);
			t = tile_ldna(src + 8);
			d = tile_align(d, t, src);
# if (HOST_IS_BIGENDIAN-0) == 0
			d = __insn_revbytes(d);
# endif
#elif defined(__tilepro__)
			uint32_t t1,t2,t3;
			t1 = tile_ldna(src);
			t2 = tile_ldna(src + 4);
			t3 = tile_ldna(src + 8);
			t1 = tile_align(t1, t2, src);
			t2 = tile_align(t2, t3, src + 4);
# if (HOST_IS_BIGENDIAN-0) == 0
			t1 = __insn_bytex(t1);
			t2 = __insn_bytex(t2);
# endif
			d = (((uint64_t)t1) << 32) | t2;
#else
			d = get_unaligned_be64(src);
#endif
		}
		src += 5;
		len -= 5;
		dst = do_40bit(dst, d);
	}
	return to_base32_generic(dst, src, len);
}
static char const rcsid_tb32ti[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
