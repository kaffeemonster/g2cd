/*
 * memneg.c
 * neg a memory region efficient, ppc implementation
 *
 * Copyright (c) 2006,2007 Jan Seiffert
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
 * $Id:$
 */

#ifdef __ALTIVEC__
# include <altivec.h>
#endif

void *memneg(void *dst, const void *src, size_t len)
{
	char *dst_char;
	const char *src_char;

	if(!dst)
		return dst;

	if(!src)
		src = (const void*) dst;

	dst_char = dst;
	src_char = src;
	
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
				*dst_char++ = ~(*src_char++);
			goto alignment_16;
		}
#endif
		tmp_dst = (char *)ALIGN(dst_char, SOST);
		tmp_src = (const char *)ALIGN(src_char, SOST);

		if((tmp_dst - dst_char) == (tmp_src - src_char))
		{
			size_t bla = tmp_dst - dst_char;
			for(; bla && len; bla--, len--)
				*dst_char++ = ~(*src_char++);
			goto alignment_size_t;
		}

		/*
		 * Achiving an alignment of 8 is sometimes hard.
		 * since this is also the 64 bit impl., make a 4 byte
		 * fallback, if the Compiler is smart enough, he will
		 * kick it out if SOST == 4
		 */
		tmp_dst = (char *)ALIGN(dst_char, 4);
		tmp_src = (const char *)ALIGN(src_char, 4);

		if((tmp_dst - dst_char) == (tmp_src - src_char))
		{
			size_t bla = tmp_dst - dst_char;
			for(; bla && len; bla--, len--)
				*dst_char++ = ~(*src_char++);
			goto alignment_4;
		}
	}

	/* fall throuh if alignment fails */
	goto no_alignment_possible;
/* Special implementations below here */

	/* 
	 * neg it with a hopefully bigger and
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
		static vector const int all_one = {~0, ~0, ~0, ~0};
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
			*dst_vec = __builtin_altivec_vxor(*src_vec, all_one);
/* my cross compiler bails out with a parsing error, fscking marcro-magic
			*dst_vec = vec_xor(*dest_vec, *src_vec); */
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
			*dst_sizet++ = ~(*src_sizet++);

		dst_char = (char *) dst_sizet;
		src_char = (const char *) src_sizet;
		goto handle_remaining;
	}
alignment_4:
	if(len/4)
	{
		register uint32_t *dst_u32 = (uint32_t *)dst_char;
		register const uint32_t *src_u32 = (const uint32_t *)src_char;
		register size_t small_len = len / 4;
		len %= 4;

		while(small_len--)
			*dst_u32++ = ~(*src_u32++);

		dst_char = (char *) dst_u32;
		src_char = (const char *) src_u32;
		goto handle_remaining;
	}

no_alignment_wanted:
no_alignment_possible:
handle_remaining:
	/* neg whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ = ~(*src_char++);

	return dst;
}

static char const rcsid_mx[] GCC_ATTR_USED_VAR = "$Id:$";
