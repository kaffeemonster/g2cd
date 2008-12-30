/*
 * mempcpy.c
 * memcpy
 *
 * Copyright (c) 2008 Jan Seiffert
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
 * memcpy - memcpy
 * dst: where to copy to
 * src: from where to copy
 * len: how much to copy
 *
 * return value: a pointer to dst
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifndef MEMCPY_DEFINED
void *memcpy(void *restrict dst, const void *restrict src, size_t len);
#define MEMCPY_DEFINED
#endif

/*
 * let the compiler get fancy at optimizing this.
 * if he doesn't know "restrict": it's not that often used
 * and propaply still better than strcpys and strcats
 * normaly used in this situation.
 */

static noinline GCC_ATTR_FASTCALL void *memcpy_big(void *restrict dst, const void *restrict src, size_t len)
{
	size_t i = 0;
	char *restrict dst_c = dst;
	const char *restrict src_c = src;

	if(UNALIGNED_OK)
	{
		if(len < 32) {
			((uint64_t *)dst_c)[0] = ((const uint64_t *)src_c)[0];
			((uint64_t *)dst_c)[1] = ((const uint64_t *)src_c)[1];
			return cpy_rest_o(dst_c + 16, src_c + 16, len - 16);
		}
	}

	for(; i < len; i++)
		dst_c[i] = src_c[i];

	return dst;
}

void *memcpy(void *restrict dst, const void *restrict src, size_t len)
{
	if(likely(len < 16))
		return cpy_rest_o(dst, src, len);

	/* trick gcc to generate lean stack frame and do a tailcail */
	return memcpy_big(dst, src, len);
}

static char const rcsid_mcg[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
