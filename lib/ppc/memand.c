/*
 * memand.c
 * and two memory region efficient, ppc implementation
 *
 * Copyright (c) 2005,2006 Jan Seiffert
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

#ifdef __ALTIVEC__
# include <altivec.h>
#endif

void *memand(void *dst, const void *src, size_t len)
{
	char *dst_char = dst;
	const char *src_char = src;

	if(!dst || !src)
		return dst;
	
	if(SYSTEM_MIN_BYTES_WORK > len)
		goto no_alignment_wanted;
	
	{
		char *tmp_dst;
		const char *tmp_src;
#ifdef __ALTIVEC__
		tmp_dst = (char *)ALIGN(dst_char, 16);
		tmp_src = (const char *)ALIGN(src_char, 16);

		if((tmp_dst - dst_char) == (tmp_src - src_char))
		{
			size_t bla = tmp_dst - dst_char;
			for(; bla && len; bla--, len--)
				*dst_char++ &= *src_char++;
			goto alignment_16;
		}
#endif
		tmp_dst = (char *)ALIGN(dst_char, SOST);
		tmp_src = (const char *)ALIGN(src_char, SOST);

		if((tmp_dst - dst_char) == (tmp_src - src_char))
		{
			size_t bla = tmp_dst - dst_char;
			for(; bla && len; bla--, len--)
				*dst_char++ &= *src_char++;
			goto alignment_size_t;
		}
	}

	/* fall throuh if alignment fails */
	goto no_alignment_possible;
/* Special implementations below here */

	/* 
	 * and it with a hopefully bigger and
	 * maschine-native datatype
	 */
#ifdef __ALTIVEC__
	/*
	 * blaerch, what have they done to my 68k...
	 * Oh, and a WARNING: totaly untested, not even compiled,
	 * i don't own the hardware.
	 *
	 * this needs an #include <altivec.h>
	 */
alignment_16:
	if(len/16)
	{
		register vector int *dst_vec = (vector int *) dst_char;
		register vector const int *src_vec = (vector const int *) src_char;
		register size_t small_len = len / (sizeof(*dst_vec)/sizeof(*dst_char));
		
		/* 
		 * Oh man...
		 * Altivec can pump xx GB of data through its pipelines when
		 * simple instruction are computet (like this), but the FSB
		 * gets filled very early (G5 2 GHz -> 1.2 GB), even the L1
		 * cache can not keep up with it ;(
		 * If Apple, IBM an Freescale would had worked *really* on
		 * this to solve it - killer...
		 * So since this is totaly IO bound, let the compiler do the
		 * dirty work, esp. prefetch. Don't forget to switch on the
		 * compiler flags:
		 *  -Ox {-mcpu=bla|-maltivec} -fprefetch -funroll-loops
		 */
		while(small_len--)
		{
			*dst_vec = __builtin_altivec_vand(*dst_vec, *src_vec);
/* my cross compiler bails out with a parsing error, fscking marcro-magic
			*dst_vec = vec_and(*dest_vec, *src_vec); */
			dst_vec++;
			src_vec++;
		}
		
		len %= sizeof(*dst_vec)/sizeof(*dst_char);
		dst_char = (char *) dst_vec;
		src_char  = (const char *) src_vec;
		goto handle_remaining;
	}
#endif
/* All archs fallback-code
 * (hmmm, runs 3 times faster on Sparc)
 */
alignment_size_t:
	if(len/SOST)
	{
		register size_t *dst_sizet = (size_t *)dst_char;
		register const size_t *src_sizet = (const size_t *)src_char;
		register size_t small_len = len / SOST;
		len %= SOST;

		while(small_len--)
			*dst_sizet++ &= *src_sizet++;

		dst_char = (char *) dst_sizet;
		src_char = (const char *) src_sizet;
		goto handle_remaining;
	}

no_alignment_wanted:
no_alignment_possible:
handle_remaining:
	/* and whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ &= *src_char++;

	return dst;
}

static char const rcsid_ma[] GCC_ATTR_USED_VAR = "$Id:$";
