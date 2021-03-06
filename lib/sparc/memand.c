/*
 * memand.c
 * and two memory region efficient, sparc/sparc64 implementation
 *
 * Copyright (c) 2009-2010 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id:$
 */

#if defined(HAVE_VIS) && (defined(HAVE_REAL_V9) || defined(__sparcv9) || defined(__sparc_v9__))
# include "sparc_vis.h"
# define ALIGNMENT_WANTED SOVV

void *memand(void *dst, const void *src, size_t len)
{
	char *dst_char = dst;
	const char *src_char = src;

	if(!dst || !src)
		return dst;

	/* we will kick this mem, fetch it... */
	prefetch(src_char);
	prefetch(src_char + 64);
	prefetch(src_char + 128);
	prefetchw(dst_char);
	prefetchw(dst_char + 64);
	prefetchw(dst_char + 128);

	if(SYSTEM_MIN_BYTES_WORK > len)
		goto handle_remaining;
	else
	{
		/* blindly align dst ... */
		size_t i = ALIGN_DIFF(dst_char, ALIGNMENT_WANTED);
		i += SOVV; /* make sure src is at least one vector in the memblock */
		len -= i;
		for(; i; i--)
			*dst_char++ &= *src_char++;
		/* ... and check the outcome */
		i = (((intptr_t)dst_char) & ((ALIGNMENT_WANTED * 2) - 1)) ^
		    (((intptr_t)src_char) & ((ALIGNMENT_WANTED * 2) - 1));
		if(unlikely(i &  1))
			goto no_alignment_wanted;
		if(unlikely(i &  2))
			goto no_alignment_wanted;
		if(unlikely(i &  4))
			goto alignment_size_t;
		if(i &  8)
			goto alignment_8;
		/* fall throuh */
		goto alignment_16;
		goto no_alignment_possible;
	}

	/* fall throuh if alignment fails */
	goto no_alignment_possible;

alignment_16:
alignment_8:
	if(len / SOVV)
	{
		register unsigned long long *dst_vec = (unsigned long long *) dst_char;
		register const unsigned long long *src_vec = (const unsigned long long *) src_char;
		size_t small_len = len / SOVV;
		register size_t smaller_len = small_len / 4;
		small_len %= 4;

		asm (
			"tst	%2\n\t"
			"bz	2f\n\t"
			"nop	\n"
			"1:\n\t"
			"prefetch	[%0 + 128], 0\n\t"
			"ldd	[%0 +  0], %%f0\n\t"
			"ldd	[%0 +  8], %%f2\n\t"
			"ldd	[%0 + 16], %%f4\n\t"
			"ldd	[%0 + 24], %%f6\n\t"
			"inc	4*8, %0\n\t"
			"prefetch	[%1 + 128], 2\n\t"
			"ldd	[%1 +  0], %%f8\n\t"
			"ldd	[%1 +  8], %%f10\n\t"
			"ldd	[%1 + 16], %%f12\n\t"
			"ldd	[%1 + 24], %%f14\n\t"
			"fand	%%f0, %%f8, %%f0\n\t"
			"fand	%%f2, %%f10, %%f2\n\t"
			"fand	%%f4, %%f12, %%f4\n\t"
			"fand	%%f6, %%f14, %%f6\n\t"
			"std	%%f0, [%1 +  0]\n\t"
			"std	%%f2, [%1 +  8]\n\t"
			"std	%%f4, [%1 + 16]\n\t"
			"std	%%f6, [%1 + 24]\n\t"
			"inc	4*8, %1\n\t"
			"dec	1, %2\n\t"
			"cmp	%2, -1\n\t"
			"bne	1b\n\t"
			"nop\n"
			"2:\n\t"
			"tst	%3\n\t"
			"bz	4f\n\t"
			"nop\n"
			"3:\n\t"
			"ldd	[%0 + 0], %%f0\n\t"
			"inc	8, %0\n\t"
			"ldd	[%1 + 0], %%f8\n\t"
			"fand	%%f0, %%f8, %%f0\n\t"
			"std	%%f0, [%1 + 0]\n\t"
			"inc	8, %1\n\t"
			"dec	1, %3\n\t"
			"cmp	%3, -1\n\t"
			"bne	3b\n\t"
			"nop\n"
			"4:\n\t"
			: /* %0 */ "=r" (src_vec),
			  /* %1 */ "=r" (dst_vec),
			  /* %2 */ "=r" (smaller_len),
			  /* %3 */ "=r" (small_len)
			: /* %4 */ "0" (src_vec),
			  /* %6 */ "1" (dst_vec),
			  /* %7 */ "2" (smaller_len),
			  /* %8 */ "3" (small_len)
			: "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8",
			  "f9", "f10", "f11", "f12", "f13", "f14", "f15"
		);

		len %= SOVV;
		dst_char = (char *) dst_vec;
		src_char = (const char *) src_vec;
		goto handle_remaining;
	}
alignment_size_t:
no_alignment_wanted:
	if(len/(4*SOVV))
	{
		/* dst is aligned */
		register unsigned long long *dst_vec = (unsigned long long *) dst_char;
		/* only src sucks */
		register const unsigned long long *src_vec;
		size_t small_len = (len / SOVV) - 1; /* make shure not to overread */
		register size_t smaller_len = small_len / 4;
		small_len %= 4;

		src_vec = alignaddr(src_char, 0);
		asm (
			"ldd	[%0 +  0], %%f16\n\t"
			"tst	%2\n\t"
			"bz	2f\n\t"
			"nop	\n"
			"fmovd	%%f16, %%f0\n\t"
			"1:\n\t"
			"prefetch	[%0 + 128], 0\n\t"
			"ldd	[%0 +  8], %%f2\n\t"
			"ldd	[%0 + 16], %%f4\n\t"
			"ldd	[%0 + 24], %%f6\n\t"
			"ldd	[%0 + 32], %%f16\n\t"
			"inc	4*8, %0\n\t"
			"prefetch	[%0 + 128], 2\n\t"
			"ldd	[%1 +  0], %%f8\n\t"
			"ldd	[%1 +  8], %%f10\n\t"
			"ldd	[%1 + 16], %%f12\n\t"
			"ldd	[%1 + 24], %%f14\n\t"
			"faligndata	%%f0, %%f2, %%f0\n\t"
			"faligndata	%%f2, %%f4, %%f2\n\t"
			"faligndata	%%f4, %%f6, %%f4\n\t"
			"faligndata	%%f6, %%f16, %%f6\n\t"
			"fand	%%f0, %%f8, %%f0\n\t"
			"fand	%%f2, %%f10, %%f2\n\t"
			"fand	%%f4, %%f12, %%f4\n\t"
			"fand	%%f6, %%f14, %%f6\n\t"
			"std	%%f0, [%1 +  0]\n\t"
			"std	%%f2, [%1 +  8]\n\t"
			"std	%%f4, [%1 + 16]\n\t"
			"std	%%f6, [%1 + 24]\n\t"
			"inc	4*8, %1\n\t"
			"dec	1, %2\n\t"
			"cmp	%2, -1\n\t"
			"bne	1b\n\t"
			"fmovd	%%f16, %%f0\n\t"
			"2:\n\t"
			"tst	%3\n\t"
			"bz	4f\n\t"
			"nop\n"
			"fmovd	%%f16, %%f0\n\t"
			"3:\n\t"
			"ldd	[%0 + 8], %%f2\n\t"
			"inc	8, %0\n\t"
			"ldd	[%1 + 0], %%f8\n\t"
			"faligndata	%%f0, %%f16, %%f0\n\t"
			"fand	%%f0, %%f8, %%f0\n\t"
			"std	%%f0, [%1 + 0]\n\t"
			"inc	8, %1\n\t"
			"dec	1, %3\n\t"
			"cmp	%3, -1\n\t"
			"bne	3b\n\t"
			"fmovd	%%f16, %%f0\n\t"
			"4:\n\t"
			: /* %0 */ "=r" (src_vec),
			  /* %1 */ "=r" (dst_vec),
			  /* %2 */ "=r" (smaller_len),
			  /* %3 */ "=r" (small_len)
			: /* %4 */ "0" (src_vec),
			  /* %6 */ "1" (dst_vec),
			  /* %7 */ "2" (smaller_len),
			  /* %8 */ "3" (small_len)
			: "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8",
			  "f9", "f10", "f11", "f12", "f13", "f14", "f15", "f16",
			  "f17"
		);

// TODO: Hmmm, how many bytes are left???
		len %= SOVV;
		dst_char = (char *) dst_vec;
		src_char = (const char *) src_vec;
		goto handle_remaining;
	}
handle_remaining:
no_alignment_possible:
	/* and whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ &= *src_char++;

	return dst;
}

static char const rcsid_mas[] GCC_ATTR_USED_VAR = "$Id:$";
#else
# include "../generic/memand.c"
#endif
