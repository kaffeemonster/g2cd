/*
 * memxorcpy.c
 * xor two memory region efficient and cpy to dest, sparc/sparc64 implementation
 *
 * Copyright (c) 2009 Jan Seiffert
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

#if defined(HAVE_REAL_V9) || defined(__sparcv9) || defined(__sparc_v9__)
# define SOVV (sizeof(unsigned long long))
# define ALIGNMENT_WANTED SOVV
/* vec_lvl */
static inline void *alignaddr(const void *ptr, int off)
{
	void *t;
	asm volatile("alignaddr	%1, %2, %0" : "=r" (t) : "r" (ptr), "r" (off));
	return t;
}
static inline unsigned read_gsr(void)
{
	unsigned t;
	asm volatile ("rd %%gsr, %0" : "=r" (t));
	return t;
}
#else
# define ALIGNMENT_WANTED SOST
#endif

void *memxorcpy(void *dst, const void *src1, const void *src2, size_t len)
{
	char *dst_char = dst;
	const char *src_char1 = src1;
	const char *src_char2 = src2;

	if(!dst || !src1 || !src2)
		return dst;

	/* we will kick this mem, fetch it... */
	prefetch(src_char2);
	prefetch(src_char2 + 32);
	prefetch(src_char2 + 64);
	prefetch(src_char2 + 96);
	prefetchw(dst_char);
	prefetchw(dst_char + 32);
	prefetchw(dst_char + 64);
	prefetchw(dst_char + 96);

	if(SYSTEM_MIN_BYTES_WORK > len)
		goto handle_remaining;
	else
	{
		/* blindly align dst ... */
		size_t i = ALIGN_DIFF(dst_char, ALIGNMENT_WANTED);
#if defined(HAVE_REAL_V9) || defined(__sparcv9) || defined(__sparc_v9__)
		i += SOVV; /* make sure src is at least one vector in the memblock */
#endif
		len -= i;
		for(; i; i--)
			*dst_char++ = *src_char1++ ^ *src_char2++;
		/* ... and check the outcome */
		i =  (((intptr_t)dst_char)  & ((ALIGNMENT_WANTED * 2) - 1)) ^
		    ((((intptr_t)src_char1) & ((ALIGNMENT_WANTED * 2) - 1)) |
		     (((intptr_t)src_char2) & ((ALIGNMENT_WANTED * 2) - 1)));
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
#if defined(HAVE_REAL_V9) || defined(__sparcv9) || defined(__sparc_v9__)
	if(len / SOVV)
	{
		register unsigned long long *dst_vec = (unsigned long long *) dst_char;
		register const unsigned long long *src_vec1 = (const unsigned long long *) src_char1;
		register const unsigned long long *src_vec2 = (const unsigned long long *) src_char2;
		size_t small_len = len / SOVV;
		register size_t smaller_len = small_len / 4;
		small_len %= 4;

		asm (
			"tst	%3\n\t"
			"bz	2f\n\t"
			"nop	\n"
			"1:\n\t"
			"ldd	[%0 +  0], %%f0\n\t"
			"ldd	[%0 +  8], %%f2\n\t"
			"ldd	[%0 + 16], %%f4\n\t"
			"ldd	[%0 + 24], %%f6\n\t"
			"inc	4*8, %0\n\t"
			"ldd	[%1 +  0], %%f8\n\t"
			"ldd	[%1 +  8], %%f10\n\t"
			"ldd	[%1 + 16], %%f12\n\t"
			"ldd	[%1 + 24], %%f14\n\t"
			"inc	4*8, %1\n\t"
			"fxor	%%f0, %%f8, %%f0\n\t"
			"fxor	%%f2, %%f10, %%f2\n\t"
			"fxor	%%f4, %%f12, %%f4\n\t"
			"fxor	%%f6, %%f14, %%f6\n\t"
			"std	%%f0, [%2 +  0]\n\t"
			"std	%%f2, [%2 +  8]\n\t"
			"std	%%f4, [%2 + 16]\n\t"
			"std	%%f6, [%2 + 24]\n\t"
			"inc	4*8, %2\n\t"
			"dec	1, %3\n\t"
			"cmp	%3, -1\n\t"
			"bne	1b\n\t"
			"nop\n"
			"2:\n\t"
			"tst	%4\n\t"
			"bz	4f\n\t"
			"nop\n"
			"3:\n\t"
			"ldd	[%0 + 0], %%f0\n\t"
			"inc	8, %0\n\t"
			"ldd	[%1 + 0], %%f8\n\t"
			"inc	8, %1\n\t"
			"fxor	%%f0, %%f8, %%f0\n\t"
			"std	%%f0, [%2 + 0]\n\t"
			"inc	8, %2\n\t"
			"dec	1, %4\n\t"
			"cmp	%4, -1\n\t"
			"bne	3b\n\t"
			"nop\n"
			"4:\n\t"
			: /* %0 */ "=r" (src_vec2),
			  /* %1 */ "=r" (src_vec1),
			  /* %2 */ "=r" (dst_vec),
			  /* %3 */ "=r" (smaller_len),
			  /* %4 */ "=r" (small_len)
			: /* %5 */ "0" (src_vec2),
			  /* %6 */ "1" (src_vec1),
			  /* %7 */ "2" (dst_vec),
			  /* %8 */ "3" (smaller_len),
			  /* %9 */ "4" (small_len)
			: "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8",
			  "f9", "f10", "f11", "f12", "f13", "f14", "f15"
		);

		len %= SOVV;
		dst_char   = (char *) dst_vec;
		src_char1  = (const char *) src_vec1;
		src_char2  = (const char *) src_vec2;
		goto handle_remaining;
	}
alignment_size_t:
no_alignment_wanted:
		{
		unsigned a, b;

		/* check who's unaligned */
		a = (intptr_t)src_char1 & (SOVV - 1);
		b = (intptr_t)src_char2 & (SOVV - 1);
		if(a && b)
			goto both_unaligned;
		else
		{
			if(a && !b) {
				/* swap input operands */
				const char *t = src_char2;
				src_char2 = src_char1;
				src_char1 = t;
			}
			/* fallthrough */
		}
	}
	if(len/(4*SOVV))
	{
		/* dst and src1 is aligned */
		register unsigned long long *dst_vec = (unsigned long long *) dst_char;
		/* only src2 sucks */
		register const unsigned long long *src_vec1;
		register const unsigned long long *src_vec2;
		size_t small_len = (len / SOVV) - 1; /* make shure not to overread */
		register size_t smaller_len = small_len / 4;
		small_len %= 4;

		src_vec1 = (const unsigned long long *)src_char1;
		src_vec2 = alignaddr(src_char2, 0);
		asm (
			"ldd	[%0 +  0], %%f16\n\t"
			"tst	%3\n\t"
			"bz	2f\n\t"
			"nop	\n"
			"fmovd	%%f16, %%f0\n\t"
			"1:\n\t"
			"ldd	[%0 +  8], %%f2\n\t"
			"ldd	[%0 + 16], %%f4\n\t"
			"ldd	[%0 + 24], %%f6\n\t"
			"ldd	[%0 + 32], %%f16\n\t"
			"inc	4*8, %0\n\t"
			"ldd	[%1 +  0], %%f8\n\t"
			"ldd	[%1 +  8], %%f10\n\t"
			"ldd	[%1 + 16], %%f12\n\t"
			"ldd	[%1 + 24], %%f14\n\t"
			"inc	4*8, %1\n\t"
			"faligndata	%%f0, %%f2, %%f0\n\t"
			"faligndata	%%f2, %%f4, %%f2\n\t"
			"faligndata	%%f4, %%f6, %%f4\n\t"
			"faligndata	%%f6, %%f16, %%f6\n\t"
			"fxor	%%f0, %%f8, %%f0\n\t"
			"fxor	%%f2, %%f10, %%f2\n\t"
			"fxor	%%f4, %%f12, %%f4\n\t"
			"fxor	%%f6, %%f14, %%f6\n\t"
			"std	%%f0, [%2 +  0]\n\t"
			"std	%%f2, [%2 +  8]\n\t"
			"std	%%f4, [%2 + 16]\n\t"
			"std	%%f6, [%2 + 24]\n\t"
			"inc	4*8, %2\n\t"
			"dec	1, %3\n\t"
			"cmp	%3, -1\n\t"
			"bne	1b\n\t"
			"fmovd	%%f16, %%f0\n\t"
			"2:\n\t"
			"tst	%4\n\t"
			"bz	4f\n\t"
			"nop\n"
			"fmovd	%%f16, %%f0\n\t"
			"3:\n\t"
			"ldd	[%0 + 8], %%f2\n\t"
			"inc	8, %0\n\t"
			"ldd	[%1 + 0], %%f8\n\t"
			"inc	8, %1\n\t"
			"faligndata	%%f0, %%f16, %%f0\n\t"
			"fxor	%%f0, %%f8, %%f0\n\t"
			"std	%%f0, [%2 + 0]\n\t"
			"inc	8, %2\n\t"
			"dec	1, %4\n\t"
			"cmp	%4, -1\n\t"
			"bne	3b\n\t"
			"fmovd	%%f16, %%f0\n\t"
			"4:\n\t"
			: /* %0 */ "=r" (src_vec2),
			  /* %1 */ "=r" (src_vec1),
			  /* %2 */ "=r" (dst_vec),
			  /* %3 */ "=r" (smaller_len),
			  /* %4 */ "=r" (small_len)
			: /* %5 */ "0" (src_vec2),
			  /* %6 */ "1" (src_vec1),
			  /* %7 */ "2" (dst_vec),
			  /* %8 */ "3" (smaller_len),
			  /* %9 */ "4" (small_len)
			: "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8",
			  "f9", "f10", "f11", "f12", "f13", "f14", "f15", "f16",
			  "f17"
		);

// TODO: Hmmm, how many bytes are left???
		len %= SOVV;
		dst_char  = (char *) dst_vec;
		src_char1  = (const char *) src_vec1;
		src_char2  = (const char *) src_vec2;
		goto handle_remaining;
	}
both_unaligned:
	if(len/(4*SOVV))
	{
		/* dst is aligned */
		register unsigned long long *dst_vec = (unsigned long long *) dst_char;
		/* both src'es suck */
		register const unsigned long long *src_vec1;
		register const unsigned long long *src_vec2;
		unsigned fix_alignment1;
		unsigned fix_alignment2;
		size_t small_len = (len / SOVV) - 1; /* make shure not to overread */
		register size_t smaller_len = small_len / 4;
		small_len %= 4;

		src_vec1 = alignaddr(src_char1, 0);
		fix_alignment1 = read_gsr();
		src_vec2 = alignaddr(src_char2, 0);
		fix_alignment2 = read_gsr();
		asm (
			"ldd	[%0 +  0], %%f16\n\t"
			"ldd	[%1 +  0], %%f18\n\t"
			"tst	%3\n\t"
			"bz	2f\n\t"
			"nop	\n"
			"fmovd	%%f16, %%f0\n\t"
			"fmovd	%%f18, %%f8\n\t"
			"1:\n\t"
			"ldd	[%0 +  8], %%f2\n\t"
			"ldd	[%0 + 16], %%f4\n\t"
			"ldd	[%0 + 24], %%f6\n\t"
			"ldd	[%0 + 32], %%f16\n\t"
			"inc	4*8, %0\n\t"
			"ldd	[%1 +  8], %%f10\n\t"
			"ldd	[%1 + 16], %%f12\n\t"
			"ldd	[%1 + 24], %%f14\n\t"
			"ldd	[%1 + 32], %%f18\n\t"
			"inc	4*8, %1\n\t"
			"wr	%%g0, %5, %%gsr\n\t"
			"faligndata	%%f0, %%f2, %%f0\n\t"
			"faligndata	%%f2, %%f4, %%f2\n\t"
			"faligndata	%%f4, %%f6, %%f4\n\t"
			"faligndata	%%f6, %%f16, %%f6\n\t"
			"wr	%%g0, %6, %%gsr\n\t"
			"faligndata	%%f8, %%f10, %%f8\n\t"
			"faligndata	%%f10, %%f12, %%f10\n\t"
			"faligndata	%%f12, %%f14, %%f12\n\t"
			"faligndata	%%f14, %%f18, %%f14\n\t"
			"fxor	%%f0, %%f8, %%f0\n\t"
			"fxor	%%f2, %%f10, %%f2\n\t"
			"fxor	%%f4, %%f12, %%f4\n\t"
			"fxor	%%f6, %%f14, %%f6\n\t"
			"std	%%f0, [%2 +  0]\n\t"
			"std	%%f2, [%2 +  8]\n\t"
			"std	%%f4, [%2 + 16]\n\t"
			"std	%%f6, [%2 + 24]\n\t"
			"inc	4*8, %2\n\t"
			"dec	1, %3\n\t"
			"cmp	%3, -1\n\t"
			"fmovd	%%f16, %%f0\n\t"
			"bne	1b\n\t"
			"fmovd	%%f18, %%f8\n\t"
			"2:\n\t"
			"tst	%4\n\t"
			"bz	4f\n\t"
			"nop\n"
			"fmovd	%%f16, %%f0\n\t"
			"fmovd	%%f18, %%f8\n\t"
			"3:\n\t"
			"ldd	[%0 + 8], %%f16\n\t"
			"inc	8, %0\n\t"
			"ldd	[%1 + 8], %%f18\n\t"
			"inc	8, %1\n\t"
			"wr	%%g0, %5, %%gsr\n\t"
			"faligndata	%%f0, %%f16, %%f0\n\t"
			"wr	%%g0, %5, %%gsr\n\t"
			"faligndata	%%f8, %%f18, %%f8\n\t"
			"fxor	%%f0, %%f8, %%f0\n\t"
			"std	%%f0, [%2 + 0]\n\t"
			"inc	8, %2\n\t"
			"dec	1, %4\n\t"
			"cmp	%4, -1\n\t"
			"fmovd	%%f16, %%f0\n\t"
			"bne	3b\n\t"
			"fmovd	%%f18, %%f8\n\t"
			"4:\n\t"
			: /*  %0 */ "=r" (src_vec2),
			  /*  %1 */ "=r" (src_vec1),
			  /*  %2 */ "=r" (dst_vec),
			  /*  %3 */ "=r" (smaller_len),
			  /*  %4 */ "=r" (small_len)
			: /*  %5 */ "r" (fix_alignment2),
			  /*  %6 */ "r" (fix_alignment1),
			  /*  %7 */ "0" (src_vec2),
			  /*  %8 */ "1" (src_vec1),
			  /*  %9 */ "2" (dst_vec),
			  /* %10 */ "3" (smaller_len),
			  /* %11 */ "4" (small_len)
			: "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8",
			  "f9", "f10", "f11", "f12", "f13", "f14", "f15", "f16",
			  "f17", "f18", "f19"
		);

// TODO: Hmmm, how many bytes are left???
		len %= SOVV;
		dst_char  = (char *) dst_vec;
		src_char1  = (const char *) src_vec1;
		src_char2  = (const char *) src_vec2;
		goto handle_remaining;
	}
#else
alignment_size_t:
	/*
	 * All archs fallback-code
	 * (hmmm, runs 3 times faster on Sparc)
	 */
	{
		register size_t *dst_sizet = (size_t *)dst_char;
		register const size_t *src_sizet1 = (const size_t *)src_char1;
		register const size_t *src_sizet2 = (const size_t *)src_char2;
		register size_t small_len = len / SOST;
		len %= SOST;

		while(small_len--)
			*dst_sizet = *src_sizet1++ ^ *src_sizet2++;

		dst_char = (char *) dst_sizet;
		src_char1 = (const char *) src_sizet1;
		src_char2 = (const char *) src_sizet2;
		goto handle_remaining;
	}
no_alignment_possible:
no_alignment_wanted:
	/*
	 * try to skim the data in place.
	 * This is expensive, and maybe a loss. Generic CPUs are
	 * no DSPs. Still, fiddling bytewise is expensive on most
	 * CPUs, espec. those RISC which prohibit unaligned access.
	 * So a big read/write and a little swizzle within the
	 * registers (and some help from the compiler, unrolling,
	 * scheduling) is hopefully faster.
	 * If it is not an m68k...
	 * We will see, since this is only the emergency fallback
	 * iff no alignment is possible, which should not happen
	 * on these buffers with a size_t wide stride.
	 */
	if(!UNALIGNED_OK)
	{
		size_t small_len, cycles;
		size_t *dst_sizet = (size_t *)dst_char;
		if(dst_char == src_char1) /* we are actually called for 2 args... */
		{
			const size_t *src_sizet;
			register size_t c, c_ex;
			register unsigned shift1, shift2;

			cycles = small_len = (len / SOST) - 1;
			shift1 = (unsigned) ALIGN_DOWN_DIFF(src_char2, SOST);
			shift2 = SOST - shift1;
			shift1 *= BITS_PER_CHAR;
			shift2 *= BITS_PER_CHAR;
			src_sizet = (const size_t *)ALIGN_DOWN(src_char2, SOST);
			c_ex = *src_sizet++;
			while(small_len--)
			{
				c = c_ex;
				c_ex = *src_sizet++;

				if(HOST_IS_BIGENDIAN) {
					c <<= shift1;
					c  |= c_ex >> shift2;
				} else {
					c >>= shift1;
					c  |= c_ex << shift2;
				}

				*dst_sizet++ ^= c;
			}
		}
		else
		{
			const size_t *src_sizet1, *src_sizet2;
			register size_t c1, c_ex1, c2, c_ex2;
			register unsigned shift11, shift12, shift21, shift22;

			cycles = small_len = (len / SOST) - 1;
			shift11 = (unsigned) ALIGN_DOWN_DIFF(src_char1, SOST);
			shift12 = SOST - shift11;
			shift11 *= BITS_PER_CHAR;
			shift12 *= BITS_PER_CHAR;
			shift21 = (unsigned) ALIGN_DOWN_DIFF(src_char2, SOST);
			shift22 = SOST - shift21;
			shift21 *= BITS_PER_CHAR;
			shift22 *= BITS_PER_CHAR;
			src_sizet1 = (const size_t *)ALIGN_DOWN(src_char1, SOST);
			src_sizet2 = (const size_t *)ALIGN_DOWN(src_char2, SOST);
			c_ex1 = *src_sizet1++;
			c_ex2 = *src_sizet2++;
			while(small_len--)
			{
				c1 = c_ex1;
				c2 = c_ex2;
				c_ex1 = *src_sizet1++;
				c_ex2 = *src_sizet2++;

				if(HOST_IS_BIGENDIAN) {
					c1 <<= shift11;
					c1  |= c_ex1 >> shift12;
					c2 <<= shift21;
					c2  |= c_ex2 >> shift22;
				} else {
					c1 >>= shift11;
					c1  |= c_ex1 << shift12;
					c2 >>= shift21;
					c2  |= c_ex2 << shift22;
				}

				*dst_sizet++ = c1 ^ c2;
			}
		}
		dst_char  = (char *) dst_sizet;
		src_char1 += cycles * SOST;
		src_char2 += cycles * SOST;
		len       -= cycles * SOST;
		goto handle_remaining;
	}
#endif
handle_remaining:
no_alignment_possible:
	/* xor whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ = *src_char1++ ^ *src_char2++;

	return dst;
}

static char const rcsid_mx[] GCC_ATTR_USED_VAR = "$Id:$";
