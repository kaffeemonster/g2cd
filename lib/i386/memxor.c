/*
 * memxor.c
 * xor two memory region efficient, i386 implementation
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
		char *tmp_dst;
		const char *tmp_src;
#ifdef __SSE__
		tmp_dst = (char *)ALIGN(dst_char, 16);
		tmp_src = (const char *)ALIGN(src_char, 16);

		if((tmp_dst - dst_char) == (tmp_src - src_char))
		{
			size_t bla = tmp_dst - dst_char;
			for(; bla && len; bla--, len--)
				*dst_char++ ^= *src_char++;
			goto alignment_16;
		}
#endif
#ifdef __MMX__
		tmp_dst = (char *)ALIGN(dst_char, 8);
		tmp_src = (const char *)ALIGN(src_char, 8);

		if((tmp_dst - dst_char) == (tmp_src - src_char))
		{
			size_t bla = tmp_dst - dst_char;
			for(; bla && len; bla--, len--)
				*dst_char++ ^= *src_char++;
			goto alignment_8;
		}
#endif
		tmp_dst = (char *)ALIGN(dst_char, SOST);
		tmp_src = (const char *)ALIGN(src_char, SOST);

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

/* Special implementations below here */

	/* 
	 * xor it with a hopefully bigger and
	 * maschine-native datatype
	 */
#ifdef __SSE__
	/*
	 * xoring 16 byte at once is quite attracktive,
	 * if its fast...
	 *  __builtin_ia32_xorps
	 */
alignment_16:
	if(len/32)
	{
		register intptr_t d0;

		__asm__ __volatile__(
			".p2align 4\n"
			"1:\n\t"
			"movaps	(%2), %%xmm0\n\t"
			"movaps	16(%2), %%xmm1\n\t"
			"xorps	(%1), %%xmm0\n\t"
			"xorps	16(%1), %%xmm1\n\t"
			"add	$32, %1\n\t"
			"movaps	%%xmm0, (%2)\n\t"
			"movaps	%%xmm1, 16(%2)\n\t"
			"add	$32, %2\n\t"
			"dec	%0\n\t"
			"jnz	1b\n"
			: "=&c" (d0), "+&g" (src_char), "+&g" (dst_char)
			: "0" (len/32)
			: "cc", "xmm0", "xmm1", "memory"
		);
		len %= 32;
		goto handle_remaining;
	}
#endif
#ifdef __MMX__
	/*
	 * xoring 8 byte on a 32Bit maschine is also atractive
	 * __builtin_ia32_pxor
	 */
alignment_8:
	if(len/32)
	{
		register intptr_t d0;

		__asm__ __volatile__ (
			".p2align 4\n"
			"1:\n\t"
			"movq	(%2), %%mm0\n\t"
			"movq	8(%2), %%mm1\n\t"
			"movq	16(%2), %%mm2\n\t"
			"movq	24(%2), %%mm3\n\t"
			"pxor	(%1), %%mm0\n\t"
			"pxor	8(%1), %%mm1\n\t"
			"pxor	16(%1), %%mm2\n\t"
			"pxor	24(%1), %%mm3\n\t"
			"add	$32, %1\n\t"
			"movq	%%mm0, (%2)\n\t"
			"movq	%%mm1, 8(%2)\n\t"
			"movq	%%mm2, 16(%2)\n\t"
			"movq	%%mm3, 24(%2)\n\t"
			"add	$32, %2\n\t"
			"dec	%0\n\t"
			"jnz	1b\n"
			: "=&c" (d0), "+&g" (src_char), "+&g" (dst_char)
			: "0" (len/32)
			: "cc", "mm0", "mm1", "mm2", "mm3", "memory"
		);
		len %= 32;
		goto handle_remaining;
	}
#endif
	/*
	 * unfortunadly my gcc created horrible loop-code with two
	 * stack-accesses additionaly to load and store the arrays.
	 * Since this is the *main*-loop which will take the bigest
	 * part of the approx. 128kb map (2^20) or 32k loop-
	 * iterations, i had to do somehting. If this is clever, i
	 * don't know...
	 * to be honest, no great speedup
	 */
alignment_size_t:
	if(len/SOST)
	{
		register intptr_t d0, d1;

		__asm__ __volatile__(
			"cld\n\t"
			".p2align 4\n"
			"1:\n\t"
			"lodsl\n\t"
			"xorl	(%3), %%eax\n\t"
			"addl	$4, %3\n\t"
			"stosl\n\t"
			"decl	%0\n\t"
			"jnz	1b\n"
			: "=&c" (d0), "=&S" (d1), "+&D" (dst_char), "+&b" (src_char)
			: "0" (len/SOST), "1" (dst_char)
			: "cc", "eax", "memory"
		);
		len %= SOST;
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
