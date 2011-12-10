/*
 * memneg.c
 * neg a memory region efficient, Tile implementation
 *
 * Copyright (c) 2006-2011 Jan Seiffert
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
 * $Id:$
 */

#include "tile.h"

void *memneg(void *dst, const void *src, size_t len)
{
	char *dst_char;
	const char *src_char;

	if(!dst)
		return dst;

	if(!src)
		src = (const void *)dst;

	dst_char = dst;
	src_char = src;

	if(SYSTEM_MIN_BYTES_WORK > len)
		goto no_alignment_wanted;
	else
	{
		/* blindly align dst ... */
		size_t i = ALIGN_DIFF(dst_char, SOST);
		len -= i;
		for(; i; i--)
			*dst_char++ = ~(*src_char++);
		/* ... and check the outcome */
		i = ALIGN_DOWN_DIFF(dst_char, SOST * 2) ^ ALIGN_DOWN_DIFF(src_char, SOST * 2);
		if(!(i & SOSTM1))
			goto alignment_size_t;
		/* uhoh... */
		goto no_alignment_possible;
	}

	/* fall throuh if alignment fails */
	goto no_alignment_possible;

alignment_size_t:
	/*
	 * All archs fallback-code
	 * (hmmm, runs 3 times faster on Sparc)
	 */
	{
		register size_t *dst_sizet = (size_t *)dst_char;
		register const size_t *src_sizet = (const size_t *)src_char;
		register size_t small_len = len / SOST;
		len %= SOST;

		while(small_len--)
			*dst_sizet++ = ~(*src_sizet++);

		dst_char = (char *) dst_sizet;
		src_char = (const char *) src_sizet;
		goto handle_remaining;
	}
no_alignment_possible:
	/*
	 * try to skim the data in place.
	 * This is expensive, and maybe a loss. Generic CPUs are
	 * no DSPs. Still, fiddling bytewise is expensive on most
	 * CPUs, espec. those RISC which prohibit unaligned access.
	 * So a big read/write and a little swizzle within the
	 * registers (and some help from the compiler, unrolling,
	 * scheduling) is hopefully faster.
	 */
	{
		size_t *dst_sizet = (size_t *)dst_char;
		const size_t *src_sizet;
		register size_t c, c_ex, small_len, cycles;
#if !(defined(__tilegx__) || defined(__tilepro__))
		register unsigned shift1, shift2;
		shift1 = (unsigned) ALIGN_DOWN_DIFF(src_char, SOST);
		shift2 = SOST - shift1;
		shift1 *= BITS_PER_CHAR;
		shift2 *= BITS_PER_CHAR;
		src_sizet = (const size_t *)ALIGN_DOWN(src_char, SOST);
		c_ex = *src_sizet++;
#else
		src_sizet = (const size_t *)src_char;
		c_ex      = tile_ldna(src_sizet++);
#endif
		cycles = small_len = (len / SOST) - 1;
		while(small_len--)
		{
			c = c_ex;
#if !(defined(__tilegx__) || defined(__tilepro__))
			c_ex = *src_sizet++;
			if(HOST_IS_BIGENDIAN)
				c = c << shift1 | c_ex >> shift2;
			else
				c = c >> shift1 | c_ex << shift2;
#else
			c_ex = tile_ldna(src_sizet++);
			c    = tile_align(c, c_ex, src_sizet);
#endif

			*dst_sizet++ = ~c;
		}

		dst_char  = (char *) dst_sizet;
		src_char += cycles * SOST;
		len      -= cycles * SOST;
		goto handle_remaining;
	}

no_alignment_wanted:
handle_remaining:
	/* neg whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ = ~(*src_char++);

	return dst;
}

static char const rcsid_mngti[] GCC_ATTR_USED_VAR = "$Id:$";
