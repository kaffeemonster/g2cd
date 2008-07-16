/*
 * memneg.c
 * neg a memory region efficient, x86 implementation
 *
 * Copyright (c) 2006-2008 Jan Seiffert
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

#include "x86.h"

/*
 * Make sure to avoid asm operant size suffixes, this is
 * used in 32Bit & 64Bit
 */

void *memneg(void *dst, const void *src, size_t len)
{
#if defined(__MMX__) || defined (__SSE__)
	static const uint32_t all_ones[4] GCC_ATTR_ALIGNED(16) = {~0U, ~0U, ~0U, ~0U};
#endif
	char *dst_char;
	const char *src_char;

	if(!dst)
		return dst;

	if(!src)
		src = (const void *) dst;
	
	dst_char = dst;
	src_char = src;

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
				*dst_char++ = ~(*src_char++);
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
				*dst_char++ = ~(*src_char++);
			goto alignment_8;
		}
#endif
		tmp_dst = (char *)ALIGN(dst_char, SOST);
		tmp_src = (const char *)ALIGN(src_char, SOST);

		/*
		 * x86 special:
		 * x86 handles misalignment in hardware for ordinary ops.
		 * It has a penalty, but to much software relies on it
		 * (oh-so important cs enterprisie bullshit bingo soft).
		 * We simply align the write side, and "don't care" for
		 * the read side.
		 * Either its also aligned by accident, or working
		 * unaligned dwordwise is still faster than bytewise.
		 */
		{
			size_t bla = tmp_dst - dst_char;
			for(; bla && len; bla--, len--)
				*dst_char++ = ~(*src_char++);
			goto alignment_size_t;
		}
	}

	/* fall throuh if alignment fails */
	goto no_alignment_possible;

/* Special implementations below here */

	/* 
	 * neg it with a hopefully bigger and
	 * maschine-native datatype
	 */
#ifdef __SSE__
	/*
	 * neging 16 byte at once is quite attracktive,
	 * if its fast...
	 * there is no mmx/sse not, make it with xor
	 * __builtin_ia32_xorps
	 */
alignment_16:
	if(len/32)
	{
		register intptr_t d0;

		__asm__ __volatile__(
			SSE_PREFETCH(  (%1))
			SSE_PREFETCH(  (%2))
			SSE_PREFETCH(32(%1))
			SSE_PREFETCH(32(%2))
			SSE_MOVE(%4, %%xmm2)
			SSE_MOVE(%4, %%xmm3)
			".p2align 3\n"
			"1:\n\t"
			SSE_PREFETCH(64(%1))
			SSE_PREFETCH(64(%2))
			SSE_MOVE(   (%1), %%xmm0)
			SSE_MOVE( 16(%1), %%xmm1)
			"add	$32, %1\n\t"
			SSE_XOR(  %%xmm2, %%xmm0)
			SSE_XOR(  %%xmm3, %%xmm1)
			SSE_STORE(%%xmm0,   (%2))
			SSE_STORE(%%xmm1, 16(%2))
			"add	$32, %2\n\t"
			"dec	%0\n\t"
			"jnz	1b\n\t"
			SSE_FENCE
			: "=&c" (d0), "+&r" (src_char), "+&r" (dst_char)
			: "0" (len/32), "m" (*all_ones)
			: "cc", "xmm0", "xmm1", "xmm2", "xmm3", "memory"
		);
		len %= 32;
		goto handle_remaining;
	}
#endif
#ifdef __MMX__
	/*
	 * neging 8 byte on a 32Bit maschine is also atractive
	 * __builtin_ia32_pneg
	 */
alignment_8:
	if(len/32)
	{
		register intptr_t d0;

		__asm__ __volatile__ (
			MMX_PREFETCH(  (%1))
			MMX_PREFETCH(  (%2))
			MMX_PREFETCH(32(%1))
			MMX_PREFETCH(32(%2))
			"movq %4, %%mm4\n\t"
			"movq %4, %%mm5\n\t"
			"movq %4, %%mm6\n\t"
			"movq %4, %%mm7\n\t"
			".p2align 3\n"
			"1:\n\t"
			MMX_PREFETCH(64(%1))
			MMX_PREFETCH(64(%2))
			"movq	  (%1), %%mm0\n\t"
			"movq	 8(%1), %%mm1\n\t"
			"movq	16(%1), %%mm2\n\t"
			"movq	24(%1), %%mm3\n\t"
			"add	$32, %1\n\t"
			"pxor	%%mm4, %%mm0\n\t"
			"pxor	%%mm5, %%mm1\n\t"
			"pxor	%%mm6, %%mm2\n\t"
			"pxor	%%mm7, %%mm3\n\t"
			MMX_STORE(%%mm0,   (%2))
			MMX_STORE(%%mm1,  8(%2))
			MMX_STORE(%%mm2, 16(%2))
			MMX_STORE(%%mm3, 24(%2))
			"add	$32, %2\n\t"
			"dec	%0\n\t"
			"jnz	1b\n\t"
			MMX_FENCE
			: "=&c" (d0), "+&r" (src_char), "+&r" (dst_char)
			: "0" (len/32), "m" (*all_ones)
			: "cc", "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7", "memory"
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
		register size_t tmp, cnt;

		__asm__ __volatile__(
			PREFETCH(  (%2))
			PREFETCH(  (%3))
			".p2align 3\n"
			"1:\n\t"
			"mov	(%2,%1," str_it(SIZE_T_BYTE) "), %0\n\t"
			"not	%0\n\t"
			"mov	%0, (%3,%1," str_it(SIZE_T_BYTE) ")\n\t"
			"inc	%1\n\t"
			"cmp	%5,%1\n\t"
			"jne	1b\n\t"
			"lea	0x0(,%5," str_it(SIZE_T_BYTE) "), %0\n\t"
			"lea	(%2,%0,1), %2\n\t"
			"lea	(%3,%0,1), %3\n"
			: /* %0 */ "=&r" (tmp),
			  /* %1 */ "=&r" (cnt),
			  /* %2 */ "+&r" (dst_char),
			  /* %3 */ "+&r" (src_char),
			  /* %4 */ "=m" (dst_char)
			: /* %5 */ "r" (len/SOST),
			  /* %6 */ "m" (src_char),
			  /* %7 */ "1" (0)
			: "cc"

		);
		len %= SOST;
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

static char const rcsid_mn[] GCC_ATTR_USED_VAR = "$Id:$";
