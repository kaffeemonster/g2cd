/*
 * to_base16.c
 * convert binary string to hex, alpha impl.
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

#define ONLY_REMAINDER
#define ARCH_NAME_SUFFIX _generic
#include "../generic/to_base16.c"
#include "alpha.h"

#define TCINUL (SOUL / sizeof(tchar_t))

tchar_t *to_base16(tchar_t *dst, const unsigned char *src, unsigned len)
{
	static const uint64_t vals[] =
	{
		0x0f0f0f0f0f0f0f0fULL,
		0x3030303030303030ULL,
		0x3A3A3A3A3A3A3A3AULL,
		0x2727272727272727ULL,
	};

	for(; len >= SOUL; len -= SOUL, src += SOUL, dst += 2 * SOUL)
	{
		uint64_t in_l, in_h;

		in_l  = get_unaligned((const uint64_t *)src);
		in_h  = (in_l & (~vals[0])) >> 4;
		in_l &= vals[0];
		in_h += vals[1];
		in_l += vals[1];
		in_h += zapnot(vals[3], cmpbge(in_h, vals[2]));
		in_l += zapnot(vals[3], cmpbge(in_l, vals[2]));

#if defined(__alpha_bwx__) || defined(__alpha_max__)
# ifdef __alpha_max__
		{
		uint64_t out, t1, t2, t3, t4;
		if(!HOST_IS_BIGENDIAN)
		{
			out = unpkbw(in_h)       | (unpkbw(in_l) << 8);
			t1  = unpkbw(out);
			t2  = unpkbw(out >> 32);
			out = unpkbw(in_h >> 32) | (unpkbw(in_l >> 32) << 8);
			t3  = unpkbw(out);
			t4  = unpkbw(out >> 32);
		}
		else
		{
			out = (unpkbw(in_h >> 32) << 8) | unpkbw(in_l >> 32);
			t1  = unpkbw(out >> 32);
			t2  = unpkbw(out);
			out = (unpkbw(in_h)       << 8) | unpkbw(in_l);
			t3  = unpkbw(out >> 32);
			t4  = unpkbw(out);
		}
		put_unaligned(t1, (uint64_t *)(dst + 0 * TCINUL));
		put_unaligned(t2, (uint64_t *)(dst + 1 * TCINUL));
		put_unaligned(t3, (uint64_t *)(dst + 2 * TCINUL));
		put_unaligned(t4, (uint64_t *)(dst + 3 * TCINUL));
		}
# else
		if(!HOST_IS_BIGENDIAN)
		{
			dst[ 0] = (in_h & 0x00000000000000ffULL);
			dst[ 1] = (in_l & 0x00000000000000ffULL);
			dst[ 2] = (in_h & 0x000000000000ff00ULL) >>  8;
			dst[ 3] = (in_l & 0x000000000000ff00ULL) >>  8;
			dst[ 4] = (in_h & 0x0000000000ff0000ULL) >> 16;
			dst[ 5] = (in_l & 0x0000000000ff0000ULL) >> 16;
			dst[ 6] = (in_h & 0x00000000ff000000ULL) >> 24;
			dst[ 7] = (in_l & 0x00000000ff000000ULL) >> 24;
			dst[ 8] = (in_h & 0x000000ff00000000ULL) >> 32;
			dst[ 9] = (in_l & 0x000000ff00000000ULL) >> 32;
			dst[10] = (in_h & 0x0000ff0000000000ULL) >> 40;
			dst[11] = (in_l & 0x0000ff0000000000ULL) >> 40;
			dst[12] = (in_h & 0x00ff000000000000ULL) >> 48;
			dst[13] = (in_l & 0x00ff000000000000ULL) >> 48;
			dst[14] = (in_h & 0xff00000000000000ULL) >> 56;
			dst[15] = (in_l & 0xff00000000000000ULL) >> 56;
		}
		else
		{
			dst[ 0] = (in_h & 0xff00000000000000ULL) >> 56;
			dst[ 1] = (in_l & 0xff00000000000000ULL) >> 56;
			dst[ 2] = (in_h & 0x00ff000000000000ULL) >> 48;
			dst[ 3] = (in_l & 0x00ff000000000000ULL) >> 48;
			dst[ 4] = (in_h & 0x0000ff0000000000ULL) >> 40;
			dst[ 5] = (in_l & 0x0000ff0000000000ULL) >> 40;
			dst[ 6] = (in_h & 0x000000ff00000000ULL) >> 32;
			dst[ 7] = (in_l & 0x000000ff00000000ULL) >> 32;
			dst[ 8] = (in_h & 0x00000000ff000000ULL) >> 24;
			dst[ 9] = (in_l & 0x00000000ff000000ULL) >> 24;
			dst[10] = (in_h & 0x0000000000ff0000ULL) >> 16;
			dst[11] = (in_l & 0x0000000000ff0000ULL) >> 16;
			dst[12] = (in_h & 0x000000000000ff00ULL) >>  8;
			dst[13] = (in_l & 0x000000000000ff00ULL) >>  8;
			dst[14] = (in_h & 0x00000000000000ffULL);
			dst[15] = (in_l & 0x00000000000000ffULL);
		}
# endif
#else
		{
		uint64_t o1, o2, o3, o4;
		if(!HOST_IS_BIGENDIAN)
		{
			o1  = (in_h & 0x00000000000000ffULL);
			o1 |= (in_l & 0x00000000000000ffULL) <<  16;
			o1 |= (in_h & 0x000000000000ff00ULL) << (32 - 8);
			o1 |= (in_l & 0x000000000000ff00ULL) << (48 - 8);
			o2  = (in_h & 0x0000000000ff0000ULL) >>   16;
			o2 |= (in_l & 0x0000000000ff0000ULL);
			o2 |= (in_h & 0x00000000ff000000ULL) << (16 - 8);
			o2 |= (in_l & 0x00000000ff000000ULL) << (32 - 8);
			o3  = (in_h & 0x000000ff00000000ULL) >>  32;
			o3 |= (in_l & 0x000000ff00000000ULL) >>  16;
			o3 |= (in_h & 0x0000ff0000000000ULL) >>   8;
			o3 |= (in_l & 0x0000ff0000000000ULL) <<   8;
			o4  = (in_h & 0x00ff000000000000ULL) >> 48;
			o4 |= (in_l & 0x00ff000000000000ULL) >> 32;
			o4 |= (in_h & 0xff00000000000000ULL) >> 24;
			o4 |= (in_l & 0xff00000000000000ULL) >>  8;
		}
		else
		{
			o1  = (in_h & 0xff00000000000000ULL) >>   8;
			o1 |= (in_l & 0xff00000000000000ULL) >>  24;
			o1 |= (in_h & 0x00ff000000000000ULL) >> (40 - 8);
			o1 |= (in_l & 0x00ff000000000000ULL) >> (56 - 8);
			o2  = (in_h & 0x0000ff0000000000ULL) <<   8;
			o2 |= (in_l & 0x0000ff0000000000ULL) >>   8;
			o2 |= (in_h & 0x000000ff00000000ULL) >> (24 - 8);
			o2 |= (in_l & 0x000000ff00000000ULL) >> (40 - 8);
			o3  = (in_h & 0x00000000ff000000ULL) << 24;
			o3 |= (in_l & 0x00000000ff000000ULL) <<  8;
			o3 |= (in_h & 0x0000000000ff0000ULL);
			o3 |= (in_l & 0x0000000000ff0000ULL) >> 16;
			o4  = (in_h & 0x000000000000ff00ULL) <<  40;
			o4 |= (in_l & 0x000000000000ff00ULL) <<  24;
			o4 |= (in_h & 0x00000000000000ffULL) << (8 + 8);
			o4 |= (in_l & 0x00000000000000ffULL);
		}
		put_unaligned(o1, (uint64_t *)(dst + 0 * TCINUL));
		put_unaligned(o2, (uint64_t *)(dst + 1 * TCINUL));
		put_unaligned(o3, (uint64_t *)(dst + 2 * TCINUL));
		put_unaligned(o4, (uint64_t *)(dst + 3 * TCINUL));
		}
#endif
	}

	if(unlikely(len))
		return to_base16_generic(dst, src, len);
	return dst;
}

static char const rcsid_tb16al[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
