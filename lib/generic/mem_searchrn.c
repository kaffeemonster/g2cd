/*
 * mem_searchrn.c
 * search mem for a \r\n, generic implementation
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
 * keep it 32 Bit, so even 64Bit arches have a chance to use
 * a broader datatype and not immediatly baile out
 */
#define ALIGN_WANTED SO32

void *mem_searchrn(void *src, size_t len)
{
	char *src_c = src;
	size_t i = 0;

	prefetch(src);
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
		register uint32_t *src_b = (uint32_t *)src_c;
		size_t cycles;
		uint32_t r;

		cycles = i = len / SO32;
		for(; likely(i); i--, src_b++)
		{
			uint32_t v = *src_b ^ 0x0D0D0D0D; /* '\r\r\r\r' */
			r = has_nul_byte32(v);
			if(r)
			{
				unsigned o = nul_byte_index32(r);
				len   -= o;
				src_b  = (uint32_t *)(((char *)src_b) + o) ;
				break;
			}
		}

		len  -= (cycles - i) * SO32;
		src_c = (char *)src_b;
	}

OUT:
	for(; len; len--, src_c++) {
		if('\r' == *src_c && len-1 && '\n' == src_c[1])
			return src_c;
	}
	return NULL;
}

static char const rcsid_msrn[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
