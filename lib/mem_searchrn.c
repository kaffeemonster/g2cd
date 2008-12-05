/*
 * mem_searchrn.c
 * search mem for a \r\n
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
 * mem_searchrn - search memory for a \r\n
 * src: where to search
 * len: the maximum length
 *
 * return value: a pointer at the \r\n, or NULL if not found.
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

/*
 * keep it 32 Bit, so even 64Bit arches have a chance to use
 * a broader datatype and not immediatly baile out
 */
#define ALIGN_WANTED (sizeof(uint32_t))
#define has_nul_byte(x) \
	(((x) - 0x01010101) & ~(x) & 0x80808080)

void *mem_searchrn(void *src, size_t len)
{
	char *src_c = src;
	size_t i = 0;

	if(unlikely(!src_c || !len))
		return NULL;

	if(unlikely(len < 4))
		goto OUT;

	if(!UNALIGNED_OK)
	{
		/* Unaligned access is not ok.
		 * Blindly try to align src.
		 */
		i = (char *)ALIGN(src_c, ALIGN_WANTED) - src_c;
		for(; len && i; len--, i--, src_c++) {
			if('\r' == *src_c && len-1 && '\n' == src_c[1])
					return src_c;
		}
	}

	/* Everything's aligned, life is good... */
	{
		uint32_t *src_b = (uint32_t *)src_c;
		size_t cycles;

		cycles = i = len / sizeof(uint32_t);
		for(; likely(i); i--, src_b++) {
			uint32_t v = *src_b ^ 0x0D0D0D0D; /* '\r\r\r\r' */
			if(has_nul_byte(v))
				break;
		}

		len  -= (cycles - i) * sizeof(uint32_t);
		src_c = (char *)src_b;
	}

OUT:
	for(; len; len--, src_c++) {
		if('\r' == *src_c && len-1 && '\n' == src_c[1])
			return src_c;
	}
	return NULL;
}

static char const rcsid_snpcg[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
