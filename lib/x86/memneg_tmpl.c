/*
 * memneg.c
 * neg a memory region efficient, x86 implementation, template
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

static void *DFUNC_NAME(memneg, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len)
{
#if defined(HAVE_MMX) || defined (HAVE_SSE) || defined (HAVE_AVX)
	static const uint32_t all_ones[8] GCC_ATTR_ALIGNED(32) = {~0U, ~0U, ~0U, ~0U, ~0U, ~0U, ~0U, ~0U};
#endif
	char *dst_char = dst;
	const char *src_char = src;

	if(!dst)
		return dst;

	if(!src)
		src = (const void *) dst;

	if(SYSTEM_MIN_BYTES_WORK > len || (ALIGNMENT_WANTED*4) > len)
		goto no_alignment_wanted;
	else
	{
		size_t i = ((char *)ALIGN(dst_char, ALIGNMENT_WANTED))-dst_char;
		len -= i;
		for(; i; i--)
			*dst_char++ = ~(*src_char++);
		i = (((intptr_t)dst_char)&((ALIGNMENT_WANTED*2)-1))^(((intptr_t)src_char)&((ALIGNMENT_WANTED*2)-1));
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
		if(i &  1)
			goto alignment_size_t;
		if(i &  2)
			goto alignment_size_t;
		if(i &  4)
			goto alignment_size_t;
		if(i &  8)
			goto alignment_8;
		if(i & 16)
			goto alignment_16;
		/* fall throuh */
		goto alignment_32;
	}

	/* fall throuh if alignment fails */
	goto no_alignment_possible;

/* Special implementations below here */

	/* 
	 * neg it with a hopefully bigger and
	 * maschine-native datatype
	 */
alignment_32:
#ifdef HAVE_AVX
alignment_16:
alignment_8:
	/*
	 * neging 256 bit at once even sounds better!
	 * and alignment is handeld more transparent!
	 * they only forgot the pxor instruction for
	 * avx, again...
	 */
	{
		register intptr_t d0;
		__asm__ __volatile__(
			SSE_PREFETCH(  (%1))
			SSE_PREFETCH(  (%2))
			SSE_PREFETCH(64(%1))
			SSE_PREFETCH(64(%2))
			SSE_PREFETCH(128(%1))
			SSE_PREFETCH(128(%2))
			AVX_MOVE(%4, %%ymm0)
			"test	%0, %0\n\t"
			"jz	2f\n\t"
			AVX_MOVE(%4, %%ymm1)
			".p2align 3\n"
			"1:\n\t"
			SSE_PREFETCH(196(%1))
			SSE_PREFETCH(196(%2))
			AVX_XOR(    (%1), %%ymm0, %%ymm2)
			AVX_XOR(  32(%1), %%ymm1, %%ymm3)
			"add	$64, %1\n\t"
			AVX_STORE(%%ymm2,   (%2))
			AVX_STORE(%%ymm3, 32(%2))
			"add	$64, %2\n\t"
			"dec	%0\n\t"
			"jnz	1b\n"
			/* loop done, handle trailer */
			"2:\n\t"
			"test	$32, %5\n\t"
			"jne	3f\n\t"
			AVX_XOR(    (%1), %%ymm0, %%ymm2)
			"add	$32, %1\n\t"
			AVX_STORE(%%ymm2,  (%2))
			"add	$32, %2\n"
			"3:\n\t"
			"test	$16, %5\n\t"
			"jne	4f\n\t"
			AVX_XOR(    (%1), %%xmm0, %%xmm2)
			"add	$16, %1\n\t"
			AVX_STORE(%%xmm2,  (%2))
			"add	$16, %2\n"
			"4:\n\t"
			/* done! */
			SSE_FENCE
			: "=&c" (d0), "+&r" (src_char), "+&r" (dst_char)
			: "0" (len/64), "m" (*all_ones), "r" (len%64)
			: "cc", "memory",
#ifdef __avx__
			  "ymm0", "ymm1", "ymm2", "ymm3"
#else
			  /*
			   * since these registers overlap, the compiler does not
			   * need to know where exactly the party is hapening, when
			   * he does not understand what ymm is
			   */
			  "xmm0", "xmm1", "xmm2", "xmm3"
#endif
		);
		len %= 16;
		goto handle_remaining;
	}
#else
alignment_16:
# ifdef HAVE_SSE
	/*
	 * neging 16 byte at once is quite attracktive,
	 * if its fast...
	 * there is no mmx/sse not, make it with xor
	 * __builtin_ia32_xorps
	 */
	{
		register intptr_t d0;

		__asm__ __volatile__(
			SSE_PREFETCH(  (%1))
			SSE_PREFETCH(  (%2))
			SSE_PREFETCH(32(%1))
			SSE_PREFETCH(32(%2))
			SSE_MOVE(%4, %%xmm2)
			"test	%0, %0\n\t"
			"jz	2f\n\t"
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
			"jnz	1b\n"
			/* loop done, handle trailer */
			"2:\n\t"
			"test	$16, %5\n\t"
			"jne	3f\n\t"
			SSE_MOVE(   (%1), %%xmm0)
			"add	$16, %1\n\t"
			SSE_XOR(  %%xmm2, %%xmm0)
			SSE_STORE(%%xmm0,  (%2))
			"add	$16, %2\n"
			"3:\n\t"
			/* done */
			SSE_FENCE
			: "=&c" (d0), "+&r" (src_char), "+&r" (dst_char)
			: "0" (len/32), "m" (*all_ones), "r" (len%32)
			: "cc", "xmm0", "xmm1", "xmm2", "xmm3", "memory"
		);
		len %= 16;
		goto handle_remaining;
	}
# endif
alignment_8:
# ifdef HAVE_SSE3
	{
		register intptr_t d0;

		__asm__ __volatile__(
			SSE_PREFETCH(  (%1))
			SSE_PREFETCH(  (%2))
			SSE_PREFETCH(32(%1))
			SSE_PREFETCH(32(%2))
			SSE_MOVE(%4, %%xmm2)
			"test	%0, %0\n\t"
			"jz	2f\n\t"
			SSE_MOVE(%4, %%xmm3)
			".p2align 3\n"
			"1:\n\t"
			SSE_PREFETCH(64(%1))
			SSE_PREFETCH(64(%2))
			SSE_LOAD(   (%1), %%xmm0)
			SSE_LOAD( 16(%1), %%xmm1)
			"add	$32, %1\n\t"
			SSE_XOR(  %%xmm2, %%xmm0)
			SSE_XOR(  %%xmm3, %%xmm1)
			SSE_STORE(%%xmm0,   (%2))
			SSE_STORE(%%xmm1, 16(%2))
			"add	$32, %2\n\t"
			"dec	%0\n\t"
			"jnz	1b\n"
			/* loop done, handle trailer */
			"2:\n\t"
			"test	$16, %5\n\t"
			"jne	3f\n\t"
			SSE_LOAD(   (%1), %%xmm0)
			"add	$16, %1\n\t"
			SSE_XOR(  %%xmm2, %%xmm0)
			SSE_STORE(%%xmm0,  (%2))
			"add	$16, %2\n"
			"3:\n\t"
			/* done */
			SSE_FENCE
			: "=&c" (d0), "+&r" (src_char), "+&r" (dst_char)
			: "0" (len/32), "m" (*all_ones), "r" (len%32)
			: "cc", "xmm0", "xmm1", "xmm2", "xmm3", "memory"
		);
		len %= 16;
		goto handle_remaining;
	}
# elif defined(HAVE_MMX) && !defined(__x86_64__)
	/*
	 * neging 8 byte on a 32Bit maschine is also atractive
	 * __builtin_ia32_pneg
	 */
	{
		register intptr_t d0;

		__asm__ __volatile__ (
			MMX_PREFETCH(  (%1))
			MMX_PREFETCH(  (%2))
			MMX_PREFETCH(32(%1))
			MMX_PREFETCH(32(%2))
			"movq	%4, %%mm4\n\t"
			"movq	%4, %%mm5\n\t"
			"test	%0, %0\n\t"
			"jz	2f\n\t"
			"movq	%4, %%mm6\n\t"
			"movq	%4, %%mm7\n\t"
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
			"jnz	1b\n"
			/* loop done, handle trailer */
			"2:\n\t"
			"test	$16, %5\n\t"
			"jne	3f\n\t"
			"movq	 (%1), %%mm0\n\t"
			"movq	8(%1), %%mm1\n\t"
			"add	$16, %1\n\t"
			"pxor	%%mm4, %%mm0\n\t"
			"pxor	%%mm5, %%mm1\n\t"
			MMX_STORE(%%mm0,  (%2))
			MMX_STORE(%%mm1, 8(%2))
			"add	$16, %2\n"
			"3:\n\t"
			"test	$8, %5\n\t"
			"jne	4f\n\t"
			"movq	 (%1), %%mm0\n\t"
			"add	$8, %1\n\t"
			"pxor	%%mm4, %%mm0\n\t"
			MMX_STORE(%%mm0,  (%2))
			"add	$8, %2\n"
			"4:\n\t"
			/* done */
			MMX_FENCE
			: "=&c" (d0), "+&r" (src_char), "+&r" (dst_char)
			: "0" (len/32), "m" (*all_ones), "r" (len%32)
			: "cc", "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7", "memory"
		);
		len %= 8;
		goto handle_remaining;
	}
# endif
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

static char const DVAR_NAME(rcsid_mn, ARCH_NAME_SUFFIX)[] GCC_ATTR_USED_VAR = "$Id:$";
