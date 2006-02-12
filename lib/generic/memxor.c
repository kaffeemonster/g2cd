/*
 * memxor.c
 * xor two memory region efficient, generic implementation
 *
 * Copyright (c) 2004,2005,2006 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2
 * of the License, or any later version.
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
 * $Id:$
 */

void *memxor(void *dst, const void *src, size_t len)
{
	char *dst_char = dst;
	const char *src_char = src;

	if(!dst || !src)
		return dst;
	
	if(SYSTEM_MIN_BYTES_WORK > len)
		goto no_alignment_wanted;

	{
		char *tmp_dst = (char *)ALIGN(dst_char, SOST);
		const char *tmp_src = (const char *)ALIGN(src_char, SOST);
		if((tmp_dst - dst_char) == (tmp_src - src_char))
		{
			size_t bla = tmp_dst - dst_char;
			for(; bla && len; bla--, len--)
				*dst_char++ ^= *src_char++;
			goto alignment_size_t;
		}
	}

	/* fall throuh if alignment fails */
	goto no_alignment_possible;
	/*
	 * All archs fallback-code
	 * (hmmm, runs 3 times faster on Sparc)
	 */
alignment_size_t:
	{
		register size_t *dst_sizet = (size_t *)dst_char;
		register const size_t *src_sizet = (const size_t *)src_char;
		register size_t small_len = len / SOST;
		len %= SOST;

		while(small_len--)
			*dst_sizet++ ^= *src_sizet++;

		dst_char = (char *) dst_sizet;
		src_char = (const char *) src_sizet;
		goto handle_remaining;
	}

no_alignment_wanted:
no_alignment_possible:
handle_remaining:
	/* xor whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ ^= *src_char++;

	return dst;
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id:$";
