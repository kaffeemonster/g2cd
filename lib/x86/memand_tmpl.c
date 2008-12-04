/*
 * memand.c
 * and two memory region efficient, x86 implementation, template
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

static void *DFUNC_NAME(memand, ARCH_NAME_SUFFIX)(void *dst, const void *src, size_t len)
{
	char *dst_char = dst;
	const char *src_char = src;

	if(!dst || !src)
		return dst;

	/* we will access the arrays, fetch them */
	__asm__ __volatile__(
		PREFETCH(  (%0))
		PREFETCH(32(%0))
		PREFETCH(64(%0))
		PREFETCH(96(%0))
		PREFETCHW(  (%1))
		PREFETCHW(32(%1))
		PREFETCHW(64(%1))
		PREFETCHW(96(%1))
		"" : /* no out */ : "r" (src), "r" (dst)
	);

	if(unlikely(SYSTEM_MIN_BYTES_WORK > len || (ALIGNMENT_WANTED*2) > len))
		goto no_alignment_wanted;
	else
	{
		size_t i = ((char *)ALIGN(dst_char, ALIGNMENT_WANTED))-dst_char;
		len -= i;
		for(; i; i--)
			*dst_char++ &= *src_char++;

		i = (((intptr_t)dst_char) & ((ALIGNMENT_WANTED * 2) - 1)) ^
		    (((intptr_t)src_char) & ((ALIGNMENT_WANTED * 2) - 1));

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
		/* fall throuh*/
		goto alignment_32;
		/* make label used */
		goto handle_remaining;
	}

	/* fall throuh if alignment fails */
	goto no_alignment_possible;

/* Special implementations below here */

	/* 
	 * and it with a hopefully bigger and
	 * maschine-native datatype
	 */
alignment_32:
#ifdef HAVE_AVX
alignment_16:
alignment_8:
	/*
	 * anding 256 bit at once even sounds better!
	 * and alignment is handeld more transparent!
	 * they only forgot the pand instruction for
	 * avx, again...
	 */
	{
		register intptr_t d0;

		__asm__ __volatile__(
			SSE_PREFETCH(  (%1))
			SSE_PREFETCHW(  (%2))
			"test	%0, %0\n\t"
			"jz	2f\n\t"
			SSE_PREFETCH(64(%1))
			SSE_PREFETCH(128(%1))
			SSE_PREFETCH(196(%1))
			SSE_PREFETCHW(64(%2))
			SSE_PREFETCHW(128(%2))
			SSE_PREFETCHW(196(%2))
			".p2align 3\n"
			"1:\n\t"
			SSE_PREFETCH(256(%1))
			SSE_PREFETCHW(256(%2))
			AVX_MOVE(   (%2), %%ymm0)
			AVX_MOVE( 32(%2), %%ymm1)
			AVX_AND(    (%1), %%ymm0, %%ymm2)
			AVX_AND(  32(%1), %%ymm1, %%ymm3)
			"add	$64, %1\n\t"
			AVX_STORE(%%ymm2,   (%2))
			AVX_STORE(%%ymm3, 32(%2))
			"add	$64, %2\n\t"
			"dec	%0\n\t"
			"jnz	1b\n"
			/* loop done, handle trailer */
			"2:\n\t"
			"test	$32, %4\n\t"
			"je	3f\n\t"
			AVX_MOVE(   (%2), %%ymm0)
			"add	$32, %1\n\t"
			AVX_AND(    (%1), %%ymm0, %%ymm2)
			AVX_STORE(%%ymm2,  (%2))
			"add	$32, %2\n"
			"3:\n\t"
			"test	$16, %4\n\t"
			"je	4f\n\t"
			AVX_MOVE(   (%2), %%xmm0)
			"add	$16, %1\n\t"
			AVX_AND(    (%1), %%xmm0, %%xmm2)
			AVX_STORE(%%xmm2,  (%2))
			"add	$16, %2\n"
			"4:\n\t"
			/* done! */
			SSE_FENCE
			: "=&c" (d0), "+&r" (src_char), "+&r" (dst_char)
			: "0" (len/64), "r" (len%64)
			: "cc",
# ifdef __SSE__
#  ifdef __avx__
			  "ymm0", "ymm1", "ymm2", "ymm3",
#  else
			  /*
			   * since these registers overlap, the compiler does not
			   * need to know where exactly the party is hapening, when
			   * he does not understand what ymm is
			   */
			  "xmm0", "xmm1", "xmm2", "xmm3",
#  endif
# endif
			  "memory"
		);
		len %= 16;
		goto handle_remaining;
	}
#else
alignment_16:
# ifdef HAVE_SSE
	/*
	 * anding 16 byte at once is quite attracktive,
	 * if its fast...
	 *  __builtin_ia32_andps
	 */
	{
		register intptr_t d0;

		__asm__ __volatile__(
			SSE_PREFETCH(  (%1))
			SSE_PREFETCHW(  (%2))
			"test	%0, %0\n\t"
			"jz	2f\n\t"
			SSE_PREFETCH(32(%1))
			SSE_PREFETCH(64(%1))
			SSE_PREFETCH(96(%1))
			SSE_PREFETCHW(32(%2))
			SSE_PREFETCHW(64(%2))
			SSE_PREFETCHW(96(%2))
			".p2align 3\n"
			"1:\n\t"
			SSE_PREFETCH(128(%1))
			SSE_PREFETCHW(128(%2))
			SSE_MOVE(   (%1), %%xmm0)
			SSE_MOVE( 16(%1), %%xmm1)
			"add	$32, %1\n\t"
			SSE_AND(    (%2), %%xmm0)
			SSE_AND(  16(%2), %%xmm1)
			SSE_STORE(%%xmm0,   (%2))
			SSE_STORE(%%xmm1, 16(%2))
			"add	$32, %2\n\t"
			"dec	%0\n\t"
			"jnz	1b\n"
			/* loop done, handle trailer */
			"2:\n\t"
			"test	$16, %4\n\t"
			"je	3f\n\t"
			SSE_MOVE(   (%1), %%xmm0)
			"add	$16, %1\n\t"
			SSE_AND(    (%2), %%xmm0)
			SSE_STORE(%%xmm0,  (%2))
			"add	$16, %2\n"
			"3:\n\t"
			/* done */
			SSE_FENCE
			: "=&c" (d0), "+&r" (src_char), "+&r" (dst_char)
			: "0" (len/32), "r" (len%32)
			: "cc",
#  ifdef __SSE__
			  "xmm0", "xmm1",
#  endif
			  "memory"
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
			SSE_PREFETCHW(  (%2))
			"test	%0, %0\n\t"
			"jz	2f\n\t"
			SSE_PREFETCH(32(%1))
			SSE_PREFETCH(64(%1))
			SSE_PREFETCH(96(%1))
			SSE_PREFETCHW(32(%2))
			SSE_PREFETCHW(64(%2))
			SSE_PREFETCHW(96(%2))
			".p2align 3\n"
			"1:\n\t"
			SSE_PREFETCH(128(%1))
			SSE_PREFETCHW(128(%2))
			SSE_LOAD(   (%1), %%xmm0)
			SSE_LOAD( 16(%1), %%xmm1)
			"add	$32, %1\n\t"
			SSE_AND(    (%2), %%xmm0)
			SSE_AND(  16(%2), %%xmm1)
			SSE_STORE(%%xmm0,   (%2))
			SSE_STORE(%%xmm1, 16(%2))
			"add	$32, %2\n\t"
			"dec	%0\n\t"
			"jnz	1b\n"
			/* loop done, handle trailer */
			"2:\n\t"
			"test	$16, %4\n\t"
			"je	3f\n\t"
			SSE_LOAD(   (%1), %%xmm0)
			"add	$16, %1\n\t"
			SSE_AND(    (%2), %%xmm0)
			SSE_STORE(%%xmm0,  (%2))
			"add	$16, %2\n"
			"3:\n\t"
			/* done */
			SSE_FENCE
			: "=&c" (d0), "+&r" (src_char), "+&r" (dst_char)
			: "0" (len/32), "r" (len%32)
			: "cc",
#  ifdef __SSE__
			  "xmm0", "xmm1",
#  endif
			  "memory"
		);
		len %= 16;
		goto handle_remaining;
	}
# elif defined(HAVE_MMX) && ! defined(__x86_64__)
	/*
	 * anding 8 byte on a 32Bit maschine is also atractive
	 * __builtin_ia32_pand
	 */
	{
		register intptr_t d0;

		__asm__ __volatile__ (
			MMX_PREFETCH(  (%1))
			MMX_PREFETCHW(  (%2))
			"test	%0, %0\n\t"
			"jz	2f\n\t"
			MMX_PREFETCH(32(%1))
			MMX_PREFETCH(64(%1))
			MMX_PREFETCH(96(%1))
			MMX_PREFETCHW(32(%2))
			MMX_PREFETCHW(64(%2))
			MMX_PREFETCHW(96(%2))
			".p2align 3\n"
			"1:\n\t"
			MMX_PREFETCH(128(%1))
			MMX_PREFETCHW(128(%2))
			"movq	  (%1), %%mm0\n\t"
			"movq	 8(%1), %%mm1\n\t"
			"movq	16(%1), %%mm2\n\t"
			"movq	24(%1), %%mm3\n\t"
			"add	$32, %1\n\t"
			"pand	  (%2), %%mm0\n\t"
			"pand	 8(%2), %%mm1\n\t"
			"pand	16(%2), %%mm2\n\t"
			"pand	24(%2), %%mm3\n\t"
			MMX_STORE(%%mm0,   (%2))
			MMX_STORE(%%mm1,  8(%2))
			MMX_STORE(%%mm2, 16(%2))
			MMX_STORE(%%mm3, 24(%2))
			"add	$32, %2\n\t"
			"dec	%0\n\t"
			"jnz	1b\n"
			/* loop done, handle trailer */
			"2:\n\t"
			"test	$16, %4\n\t"
			"je	3f\n\t"
			"movq	  (%1), %%mm0\n\t"
			"movq	 8(%1), %%mm1\n\t"
			"add	$16, %1\n\t"
			"pand	  (%2), %%mm0\n\t"
			"pand	 8(%2), %%mm1\n\t"
			MMX_STORE(%%mm0,  (%2))
			MMX_STORE(%%mm1, 8(%2))
			"add	$16, %2\n"
			"3:\n\t"
			"test	$8, %4\n\t"
			"je	4f\n\t"
			"movq	  (%1), %%mm0\n\t"
			"add	$8, %1\n\t"
			"pand	  (%2), %%mm0\n\t"
			MMX_STORE(%%mm0,  (%2))
			"add	$8, %2\n"
			"4:\n\t"
			/* done */
			MMX_FENCE
			: "=&c" (d0), "+&r" (src_char), "+&r" (dst_char)
			: "0" (len/32), "r" (len%32)
			: "cc",
#  ifdef __MMX__
			  "mm0", "mm1", "mm2", "mm3",
#  endif
			  "memory"
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
handle_remaining:
alignment_size_t:
	{
		size_t tmp1, tmp2, cnt1, cnt2, rem = len%(SOST*4);

		__asm__ __volatile__(
			PREFETCH(  (%4))
			PREFETCHW(  (%5))
			"test	%3, %3\n\t"
			"jz	2f\n\t"
			PREFETCH(32(%4))
			PREFETCH(64(%4))
			PREFETCH(96(%4))
			PREFETCHW(32(%5))
			PREFETCHW(64(%5))
			PREFETCHW(96(%5))
			/* fuck -fpic */
			"mov	%2, %0\n\t"
			"mov	%%ebx, %2\n\t"
			"mov	%0, %%ebx\n\t"
			".p2align 3\n"
			"1:\n\t"
			PREFETCH(128(%4,%%ebx,1))
			PREFETCHW(128(%5,%%ebx,1))
			"mov	"str_it(SIZE_T_BYTE)"*0(%4,%%ebx,1), %0\n\t"
			"mov	"str_it(SIZE_T_BYTE)"*1(%5,%%ebx,1), %1\n\t"
			"and	%0,"str_it(SIZE_T_BYTE)"*0(%4,%%ebx,1)\n\t"
			"and	%1,"str_it(SIZE_T_BYTE)"*1(%5,%%ebx,1)\n\t"
			"mov	"str_it(SIZE_T_BYTE)"*2(%4,%%ebx,1), %0\n\t"
			"mov	"str_it(SIZE_T_BYTE)"*3(%5,%%ebx,1), %1\n\t"
			"and	%0,"str_it(SIZE_T_BYTE)"*2(%4,%%ebx,1)\n\t"
			"and	%1,"str_it(SIZE_T_BYTE)"*3(%5,%%ebx,1)\n\t"
			"add	$"str_it(SIZE_T_BYTE)"*4,%%ebx\n\t"
			"dec	%3\n\t"
			"jnz	1b\n\t"
			"lea	(%4,%%ebx,1), %4\n\t"
			"lea	(%5,%%ebx,1), %5\n\t"
			"mov	%2, %%ebx\n"
			"2:\n\t"
			"mov	%9, %3\n\t"
			"shr	$"str_it(SIZE_T_SHIFT)", %3\n\t"
			"test	%3, %3\n\t"
			"jz	4f\n\t"
			"xor	%1, %1\n"
			"3:\n\t"
			"mov	(%4,%1,"str_it(SIZE_T_BYTE)"), %0\n\t"
			"and	%0,(%4,%1,"str_it(SIZE_T_BYTE)")\n\t"
			"inc	%1\n\t"
			"dec	%3\n\t"
			"jnz	3b\n\t"
			"lea	(%4,%1,"str_it(SIZE_T_BYTE)"), %4\n\t"
			"lea	(%5,%1,"str_it(SIZE_T_BYTE)"), %5\n"
			"4:\n"
			: /* %0 */ "=&a" (tmp1),
			  /* %1 */ "=&d" (tmp2),
			  /* %2 */ "=&rm" (cnt1),
			  /* %3 */ "=&c" (cnt2),
			  /* %4 */ "+&D" (src_char),
			  /* %5 */ "+&S" (dst_char),
			  /* %6 */ "=m" (dst_char)
			: /* %7 */ "3" (len/(SOST*4)),
			  /* %8 */ "2" (0),
			  /* %9 */ "m" (rem),
			  /* %10 */ "m" (src_char)
			: "cc"
		);
		len %= SOST;
	}

no_alignment_wanted:
no_alignment_possible:
	/* and whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ &= *src_char++;

	return dst;
}

static char const DVAR_NAME(rcsid_ma, ARCH_NAME_SUFFIX)[] GCC_ATTR_USED_VAR = "$Id:$";
