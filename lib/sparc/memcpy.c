/*
 * memcpy.c
 * memcpy, sparc/sparc64 impl.
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
 * $Id: $
 */

#if defined(HAVE_VIS) && (defined(HAVE_REAL_V9) || defined(__sparcv9) || defined(__sparc_v9__))
# include "sparc_vis.h"
# define ALIGNMENT_WANTED SOVV

noinline GCC_ATTR_FASTCALL void *memcpy_big(void *restrict dst, const void *restrict src, size_t len)
{
	const unsigned long long *src_p, *src_p_n;
	unsigned long long *dst_p;
	char *dst_c = dst, *dst_end, *dst_end2;
	const char *src_c = src, *src_end;
	unsigned long long s, s1, s2, s0;
	unsigned emask, off;

	prefetch(src_c);
	prefetch(src_c + 64);
	prefetch(src_c + 128);
	prefetchw(dst_c);
	prefetchw(dst_c + 64);
	prefetchw(dst_c + 128);

	if(SYSTEM_MIN_BYTES_WORK < len)
	{
		/* see if an alignment is possible */
		size_t i = ALIGN_DOWN_DIFF(dst_c, ALIGNMENT_WANTED) ^
		           ALIGN_DOWN_DIFF(src_c, ALIGNMENT_WANTED);
		if(unlikely(!i))
		{
			size_t cnt, t1, t2;
			/* and there was much rejoicing... */
			i = ALIGN_DIFF(dst_c, ALIGNMENT_WANTED);
			len -= i;
			/*
			 * ldda from %asi QUAD_LDD (quad load 16 b aligned?)
			 * does not use the dcache, so a membar #sync is needed
			 * before (errata)
			 * there are many problems with block loads, so a
			 * membar #sync is the best way to prevent that
			 * block stores could hang US-III+ with 7:1(1050MHz)
			 * or 8:1 (1200MHz) Fireplane clock ratio
			 * a store to the same cache line after a block store
			 * -> data corruption -> membar #sync
			 */
			asm (
				"tst	%3\n\t"
				"bz	7f\n"
				"8:\n\t"
				"ldub	[%0], %4\n\t"
				"inc	1, %0\n\t"
				"deccc	1, %3\n\t"
				"stb	%4, [%1]\n\t"
				"bnz	8b\n\t"
				"inc	1, %1\n\t"
				"7:\n\t"
				"srl	%2, 4, %3\n\t"
				"tst	%3\n\t"
				"bz	2f\n"
				"1:\n\t"
				"prefetch	[%0 + 128], 0\n\t"
				"ldx	[%0], %4\n\t"
				"ldx	[%0 + 8], %5\n\t"
				"inc	16, %0\n\t"
				"prefetch	[%1 + 128], 2\n\t"
				"deccc	1, %3\n\t"
				"stx	%4, [%1]\n\t"
				"stx	%5, [%1 + 8]\n\t"
				"bnz	1b\n\t"
				"inc	16, %1\n"
				"2:\n\t"
				"btst	8, %2\n\t"
				"bz	3f\n\t"
				"nop\n\t"
				"ldx	[%0], %4\n\t"
				"inc	8, %0\n\t"
				"stx	%4, [%1]\n\t"
				"inc	8, %1\n"
				"3:\n\t"
				"btst	4, %2\n\t"
				"bz	4f\n\t"
				"nop\n\t"
				"ld	[%0], %4\n\t"
				"inc	4, %0\n\t"
				"st	%4, [%1]\n\t"
				"inc	4, %1\n"
				"4:\n\t"
				"btst	2, %2\n\t"
				"bz	5f\n\t"
				"nop\n\t"
				"lduh	[%0], %4\n\t"
				"inc	2, %0\n\t"
				"sth	%4, [%1]\n\t"
				"inc	2, %1\n"
				"5:\n\t"
				"btst	1, %2\n\t"
				"bz	6f\n\t"
				"nop\n\t"
				"ldub	[%0], %4\n\t"
				"stb	%4, [%1]\n\t"
				"inc	1, %1\n"
				"6:\n\t"
			: /* %0 */ "=r" (src_c),
			  /* %1 */ "=r" (dst_c),
			  /* %2 */ "=r" (len),
			  /* %3 */ "=r" (cnt),
			  /* %4 */ "=r" (t1),
			  /* %5 */ "=r" (t2),
			  /* %6 */ "=m" (*(struct {char ___x___[len];} *)dst_c)
			: /* %7 */ "0" (src_c),
			  /* %8 */ "1" (dst_c),
			  /* %9 */ "2" (len),
			  /* %10 */ "3" (i),
			  /* %11 */ "m" (*(const struct {char ___x___[len];} *)src_c)
			);
			return dst;
		}
	}

	/*
	 * Since it is not possible to bring src and dst into
	 * a sync alignment (either one is always unaligned),
	 * things would be slow in sparc country (byte wise copy).
	 * VIS provides us with a alignment permutate engine to solve
	 * this.
	 * Yes, this sucks cycles and so on, but is faster than
	 * chucking byte after byte.
	 *
	 * This algo is not as anal about the len, since we send
	 * the edge cases already to cpy_rest. So we have at least
	 * 16 byte, which means at least 2 masked writes and prop.
	 * one or several (lot of..) full writes.
	 */
	dst_p = (void *)ALIGN_DOWN(dst_c, SOVV);
	off = SOVV - ALIGN_DOWN_DIFF(dst_c, SOVV);
	dst_end = (dst_c + len - 1);
	src_end = src_c + len;
	dst_end2 = dst_end - SOVV;

	/* gen edge mask */
	emask = edge8(dst_c, dst_end);

	/* prep source and set GSR */
	src_p = alignaddr(src_c, 0);
	/* load first 8 byte */
	s0 = *src_p++;
	s1 = *src_p;
	s = faligndata(s0, s1);

	/* store first 8 bytes according to mask */
	alignaddr(NULL, off);
	s = faligndata(s, s);
	pst8_P(s, dst_p, emask);

	/* prep. loop */
	dst_p++;
	src_p_n = alignaddr(src_c, off);
	if(src_p_n != src_p)
		s1 = *src_p_n;
	src_p = src_p_n + 1;
	s0 = fpsrc1(s1);

	/* copy along */
	for(; (void *)(dst_p + 1) <= (void *)dst_end2; s0 = fpsrc1(s2), src_p += 2, dst_p += 2)
	{
		s1 = src_p[0];
		s2 = src_p[1];
		s0 = faligndata(s0, s1);
		s1 = faligndata(s1, s2);
		dst_p[0] = s0;
		dst_p[1] = s1;
	}
	for(; (void *)dst_p <= (void *)dst_end2; s0 = fpsrc1(s1), src_p++, dst_p++)
	{
		s1 = *src_p;
		s = faligndata(s0, s1);
		*dst_p = s;
	}

	/* generate end mask */
	emask = edge8(dst_p, dst_end);
	/* read last bits */
	if((const void *)src_p < (const void *)src_end)
		s1 = *src_p;
	else
		s1 = fzero();
	/* align last data */
	s = faligndata(s0, s1);
	/* write out tail */
	pst8_P(s, dst_p, emask);
	return dst;
}

void *my_memcpy(void *restrict dst, const void *restrict src, size_t len)
{
	if(likely(len < 16))
		return cpy_rest_o(dst, src, len);

	/* trick gcc to generate lean stack frame and do a tailcail */
	return memcpy_big(dst, src, len);
}

static char const rcsid_mcs[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/memcpy.c"
#endif
/* EOF */
