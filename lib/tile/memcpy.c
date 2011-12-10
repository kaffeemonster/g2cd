/*
 * memcpy.c
 * memcpy, Tile impl.
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

/*
 * Based on the Kernel memcpy for TileGX which is
 *
 * Copyright 2011 Tilera Corporation. All Rights Reserved.
 *
 * Licensed under the GPL2
 */
#include "tile.h"

#define PREFETCH_LINES 3
#ifdef __tilegx__
# define CACHE_LINE_SIZE 128
#else
# define CACHE_LINE_SIZE 64
#endif

static noinline GCC_ATTR_FASTCALL void *memcpy_big(void *restrict dst, const void *restrict src, size_t len)
{
	const char *restrict src_c = src;
	const char *restrict src_end = (const char *)src_c + len - 1;
	const char *restrict prefetch;
	const size_t *restrict src_s;
	size_t *restrict dst_s;
	uint8_t *restrict dst_c = dst;
	size_t final;

	/* throw in a prefetch */
	prefetch = src_c;
	for(final = 0; final < PREFETCH_LINES; final++) {
		prefetch(prefetch);
		prefetch += CACHE_LINE_SIZE;
		prefetch = prefetch > src_end ? prefetch : src_c;
	}

	/* try to bring dst to alignment */
	for(; IS_ALIGN(dst, SOST); len--)
		*dst_c++ = *src_c++;

	dst_s = (size_t *)dst_c;
	/* check if we achieved alignment */
	if(likely(IS_ALIGN(src, SOST)))
	{
		src_s = (const size_t *)src_c;

		if(len >= CACHE_LINE_SIZE)
		{
			/* copy until dst is cache-line aligned*/
			for(; IS_ALIGN(dst_s, CACHE_LINE_SIZE); len -= SOST)
				*dst_s++ = *src_s++;

			for(; len >= CACHE_LINE_SIZE; len -= CACHE_LINE_SIZE,
			    dst_s += CACHE_LINE_SIZE/SOST,
				 src_s += CACHE_LINE_SIZE/SOST)
			{
				__insn_wh64(dst_s);
#ifdef __tilegx__
				__insn_wh64(dst_s + ((CACHE_LINE_SIZE/2)/SOST));
#endif
				prefetch(prefetch);
				prefetch += CACHE_LINE_SIZE;
				prefetch = prefetch > src_end ? prefetch : (const char *)src_s;

				dst_s[ 0] = src_s[ 0];
				dst_s[ 1] = src_s[ 1];
				dst_s[ 2] = src_s[ 2];
				dst_s[ 3] = src_s[ 3];
				dst_s[ 4] = src_s[ 4];
				dst_s[ 5] = src_s[ 5];
				dst_s[ 6] = src_s[ 6];
				dst_s[ 7] = src_s[ 7];
				dst_s[ 8] = src_s[ 8];
				dst_s[ 9] = src_s[ 9];
				dst_s[10] = src_s[10];
				dst_s[11] = src_s[11];
				dst_s[12] = src_s[12];
				dst_s[13] = src_s[13];
				dst_s[14] = src_s[14];
				dst_s[15] = src_s[15];
			}
		}

		for(; len >= SOST; len -= SOST)
			*dst_s++ = *src_s++;

		if(likely(!len))
			return dst;

		final = *src_s;
	}
	else
	{
		size_t c, t;

		src_s = (const size_t *)ALIGN_DOWN(src_c, SOST);
		c = *src_s++;
		for(; len >= SOST; len -= SOST) {
			t = *src_s++;
			c = tile_align(c, t, src_c);
			*dst_s++ = c;
			c = t;
		}

		if(!len)
			return dst;

		t = ((const char *)src_s) <= src_end ? *src_s : 0;
		final = tile_align(c, t, src_c);
	}

	/* len is != 0 here, write out trailer */
	dst_c = (uint8_t *)dst_s;
	if(SOST >= 4 && len & 4) {
		*(uint32_t *)dst_c = final;
		dst_c  += 4;
		final >>= 32;
		len    &= 3;
	}
	if(len & 2) {
		*(uint16_t *)dst_c = final;
		dst_c  += 2;
		final >>= 16;
		len    &= 1;
	}
	if(len)
		*dst_c = final;

	return dst;
}

void *my_memcpy(void *restrict dst, const void *restrict src, size_t len)
{
	if(likely(len < 16))
		return cpy_rest_o(dst, src, len);

	/* trick gcc to generate lean stack frame and do a tailcail */
	return memcpy_big(dst, src, len);
}
void *my_memcpy_fwd(void *dst, const void *src, size_t len) GCC_ATTR_ALIAS("my_memcpy");

#include "../generic/memcpy_rev.c"

static char const rcsid_mcti[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
