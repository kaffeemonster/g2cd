/*
 * memneg.c
 * neg a memory region efficient, x86_64 implementation
 *
 * Copyright (c) 2006,2007 Jan Seiffert
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

#include "../i386/x86.h"

void *memneg(void *dst, const void *src, size_t len)
{
#ifdef __SSE__
	static const uint32_t all_ones[4] GCC_ATTR_ALIGNED(16) = {~0, ~0, ~0, ~0};
#endif
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
		 * getting an alignment of 8 is sometimes hard to achive
		 * so better have a 4 byte fallback
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
			SSE_MOVE(%4, %%xmm2)
			SSE_MOVE(%4, %%xmm3)
			".p2align 4\n"
			"1:\n\t"
			SSE_PREFETCH(32(%1))
			SSE_PREFETCH(32(%2))
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
			: "=&c" (d0), "+&g" (src_char), "+&g" (dst_char)
			: "0" (len/32), "m" (*all_ones)
			: "cc", "xmm0", "xmm1", "xmm2", "xmm3", "memory"
		);
		len %= 32;
		goto handle_remaining;
	}
#endif

	/*
	 * some code for Athlon 64, but it's some kind of useless,
	 * as long as the OS (and the User-Space) don't run in
	 * 64Bit mode, using the 64 bit Regs results in a illegal-
	 * instruction-exception
	 */
alignment_size_t:
	if(len/SOST)
	{
		register intptr_t d0, d1;
			__asm__ __volatile__(
			"cld\n\t"
			".p2align 4\n"
			"1:\n\t"
			"lodsq\n\t"
			"notq	%%rax\n\t"
			"stosq\n\t"
			"decq	%0\n\t"
			"jnz	1b\n"
			: "=&c" (d0), "=&S" (d1), "+&D" (dst_char)
			: "0" (len / SOST), "1" (src_char)
			: "cc", "rax", "memory"
		);
		len %= SOST;
		goto handle_remaining;
	}
alignment_4:
	if(len/4)
	{
		register intptr_t d0, d1;
			__asm__ __volatile__(
			"cld\n\t"
			".p2align 4\n"
			"1:\n\t"
			"lodsl\n\t"
			"notl	%%eax\n\t"
			"stosl\n\t"
			"decq	%0\n\t"
			"jnz	1b\n"
			: "=&c" (d0), "=&S" (d1), "+&D" (dst_char)
			: "0" (len / SOST), "1" (src_char)
			: "cc", "eax", "memory"
		);
		len %= 4;
		goto handle_remaining;
	}

no_alignment_wanted:
no_alignment_possible:
handle_remaining:
	/* neg whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ = ~(*src_char++);

	return old_dst;
}

static char const rcsid_mn[] GCC_ATTR_USED_VAR = "$Id:$";
