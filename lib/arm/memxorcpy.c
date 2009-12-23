/*
 * memxorcpy.c
 * xor two memory region efficient and cpy to dest, arm implementation
 *
 * Copyright (c) 2005-2009 Jan Seiffert
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

#ifdef __ARM_NEON__
# include <arm_neon.h>
# include "my_neon.h"
# define ARCH_NAME_SUFFIX _neon
# define ALIGNMENT_WANTED SOVUC
#else
# define ARCH_NAME_SUFFIX _arm
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
		/* align dst, we will see what src looks like afterwards */
		size_t i = ALIGN_DIFF(dst_char, ALIGNMENT_WANTED);
#ifdef __ARM_NEON__
		i += SOVUC; /* make sure src is at least one vector in the memblock */
#endif
		len -= i;
		for(; i; i--)
			*dst_char++ = *src_char1++ ^ *src_char2++;
		i =  ALIGN_DOWN_DIFF(dst_char,  ALIGNMENT_WANTED * 2) ^
		    (ALIGN_DOWN_DIFF(src_char1, ALIGNMENT_WANTED * 2) |
		     ALIGN_DOWN_DIFF(src_char2, ALIGNMENT_WANTED * 2));
		/*
		 * We simply align the write side, and "don't care" for
		 * the read side.
		 * Either its also aligned by accident, or working
		 * unaligned dwordwise is still faster than bytewise.
		 */
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
		/* make label used */
		goto handle_remaining;
	}

	/* fall throuh if alignment fails */
	goto no_alignment_possible;
/* Special implementations below here */

	/*
	 * xor it with a hopefully bigger and
	 * maschine-native datatype
	 */
#ifdef __ARM_NEON__
alignment_16:
alignment_8:
	if(len / SOVUC)
	{
		register uint8x8_t *dst_vec = (uint8x8_t *) dst_char;
		register const uint8x8_t *src_vec1 = (const uint8x8_t *) src_char1;
		register const uint8x8_t *src_vec2 = (const uint8x8_t *) src_char2;
		size_t small_len = len / SOVUC;
		register size_t smaller_len = small_len / 8;
		small_len %= 8;

		asm (
			"cmp	%2, #0\n\t"
			"beq	4f\n"
			"1:\n\t"
			"sub	%2, %2, #1\n\t"
			"vldmia.i64	%1!, {d0-d7}\n\t"
			"vldmia.i64	%4!, {d8-d15}\n\t"
			"pld	[%1, #64]\n\t"
			"pld	[%4, #64]\n\t"
			"veor	q4, q4, q0\n\t"
			"veor	q5, q5, q1\n\t"
			"veor	q6, q6, q2\n\t"
			"veor	q7, q7, q3\n\t"
			"vstmia.i64	%0!, {d8-d15}\n\t"
			"pld	[%0, #64]\n\t"
			"bne	1b\n"
			"4:\n\t"
			"cmp	%3, #0\n\t"
			"beq	3f\n"
			"2:\n\t"
			"sub	%3, %3, #1\n\t"
			"vldmia.i64	%1!, {d0}\n\t"
			"vldmia.i64	%4!, {d1}\n\t"
			"veor	d1, d1, d0\n\t"
			"vstmia.i64	%0!, {d1}\n\t"
			"bne	2b\n"
			"3:"
			: /* %0 */ "=r" (dst_vec),
			  /* %1 */ "=r" (src_vec1),
			  /* %2 */ "=r" (smaller_len),
			  /* %3 */ "=r" (small_len),
			  /* %4 */ "=r" (src_vec2)
			: /* %5 */ "0" (dst_vec),
			  /* %6 */ "1" (src_vec1),
			  /* %7 */ "2" (smaller_len),
			  /* %8 */ "3" (small_len),
			  /* %9 */ "4" (src_vec2)
			: "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7"
		);

		len %= SOVUC;
		dst_char  = (char *) dst_vec;
		src_char1 = (const char *) src_vec1;
		src_char2 = (const char *) src_vec2;
		goto handle_remaining;
	}
alignment_size_t:
no_alignment_wanted:
	{
		unsigned a, b;

		/* check who's unaligned */
		a = ALIGN_DOWN_DIFF(src_char1, ALIGNMENT_WANTED);
		b = ALIGN_DOWN_DIFF(src_char2, ALIGNMENT_WANTED);
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
	if(len/(4*SOVUC))
	{
		/* dst and src1 is aligned */
		register uint8x8_t *dst_vec = (uint8x8_t *)dst_char;
		register const uint8x8_t *src_vec1 = (const uint8x8_t *)src_char1;
		/* only src2 sucks */
		register const uint8x8_t *src_vec2;
		size_t small_len = (len / SOVUC) - 1; /* make shure not to overread */
		register size_t smaller_len = small_len / 8;
		unsigned a_fix;
		small_len %= 8;

		a_fix = (unsigned)ALIGN_DOWN_DIFF(src_char2, SOVUC);
		src_vec2 = (const uint8x8_t *)ALIGN_DOWN(src_char2, SOVUC);
#define SW_BODY(a_fix) \
	asm ( \
		"vldr	d8, [%1, #-8]\n\t" \
		"cmp	%2, #0\n\t" \
		"beq	4f\n" \
		"1:\n\t" \
		"sub	%2, %2, #1\n\t" \
		"vorr	d0, d8, d8\n\t" \
		"vldmia.i64	%1!, {d1-d8}\n\t" \
		"vldmia.i64	%4!, {d10-d17}\n\t" \
		"pld	[%1, #64]\n\t" \
		"pld	[%4, #64]\n\t" \
		"vext.8	d0, d0, d1, #"str_it(a_fix)"\n\t" \
		"vext.8	d1, d1, d2, #"str_it(a_fix)"\n\t" \
		"vext.8	d2, d2, d3, #"str_it(a_fix)"\n\t" \
		"vext.8	d3, d3, d4, #"str_it(a_fix)"\n\t" \
		"vext.8	d4, d4, d5, #"str_it(a_fix)"\n\t" \
		"vext.8	d5, d5, d6, #"str_it(a_fix)"\n\t" \
		"vext.8	d6, d6, d7, #"str_it(a_fix)"\n\t" \
		"vext.8	d7, d7, d8, #"str_it(a_fix)"\n\t" \
		"veor	q0, q5\n\t" \
		"veor	q1, q6\n\t" \
		"veor	q2, q7\n\t" \
		"veor	q3, q8\n\t" \
		"vstmia.i64	%0!, {d0-d7}\n\t" \
		"pld	[%0, #64]\n\t" \
		"bne	1b\n" \
		"4:\n\t" \
		"cmp	%3, #0\n\t" \
		"beq	3f\n" \
		"2:\n\t" \
		"sub	%3, %3, #1\n\t" \
		"vorr	d0, d8, d8\n\t" \
		"vldmia.i64	%1!, {d8}\n\t" \
		"vldmia.i64	%4!, {d1}\n\t" \
		"vext.8	d0, d0, d8, #"str_it(a_fix)"\n\t" \
		"veor	d0, d1\n\t" \
		"vstmia.i64	%0!, {d0}\n\t" \
		"bne	2b\n" \
		"3:\n\t" \
		"sub	%1, %1, #8\n\t" \
		: /* %0 */ "=r" (dst_vec), \
		  /* %1 */ "=r" (src_vec2), \
		  /* %2 */ "=r" (smaller_len), \
		  /* %3 */ "=r" (small_len), \
		  /* %4 */ "=r" (src_vec1) \
		: /* %5 */ "0" (dst_vec), \
		  /* %6 */ "1" (src_vec2 + 1), \
		  /* %7 */ "2" (smaller_len), \
		  /* %8 */ "3" (small_len), \
		  /* %9 */ "4" (src_vec1) \
		: "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "q8", "q9" \
	);
		switch(a_fix)
		{
		case 1:
			SW_BODY(1)
			break;
		case 2:
			SW_BODY(2)
			break;
		case 3:
			SW_BODY(3)
			break;
		case 4:
			SW_BODY(4)
			break;
		case 5:
			SW_BODY(5)
			break;
		case 6:
			SW_BODY(6)
			break;
		case 7:
			SW_BODY(7)
			break;
		}
#undef SW_BODY
// TODO: Hmmm, how many bytes are left???
		len %= SOVUC;
		dst_char  = (char *)dst_vec;
		src_char1 = (const char *)src_vec1;
		src_char2 = (const char *)src_vec2;
		goto handle_remaining;
	}
both_unaligned:
	if(len/(4*SOVUC))
	{
		/* dst is aligned */
		register uint8x8_t *dst_vec = (uint8x8_t *)dst_char;
		/* src1 && src2 sucks */
		register const uint8x8_t *src_vec1;
		register const uint8x8_t *src_vec2;
		size_t small_len = (len / SOVUC) - 1; /* make shure not to overread */
		register size_t smaller_len = small_len / 8;
		unsigned a_fix, b_fix;
		small_len %= 8;

		b_fix = (unsigned)ALIGN_DOWN_DIFF(src_char1, SOVUC);
		a_fix = (unsigned)ALIGN_DOWN_DIFF(src_char2, SOVUC);
		src_vec1 = (const uint8x8_t *)ALIGN_DOWN(src_char1, SOVUC);
		src_vec2 = (const uint8x8_t *)ALIGN_DOWN(src_char2, SOVUC);

#define INNER_SW_BODY(a_fix, b_fix) \
	asm ( \
		"vldr	d8, [%1, #-8]\n\t" \
		"vldr	d18, [%4, #-8]\n\t" \
		"cmp	%2, #0\n\t" \
		"beq	4f\n" \
		"1:\n\t" \
		"sub	%2, %2, #1\n\t" \
		"vorr	d0, d8, d8\n\t" \
		"vorr	d10, d18, d18\n\t" \
		"vldmia.i64	%1!, {d1-d8}\n\t" \
		"vldmia.i64	%4!, {d11-d18}\n\t" \
		"pld	[%1, #64]\n\t" \
		"pld	[%4, #64]\n\t" \
		"vext.8	d0, d0, d1, #"str_it(a_fix)"\n\t" \
		"vext.8	d1, d1, d2, #"str_it(a_fix)"\n\t" \
		"vext.8	d2, d2, d3, #"str_it(a_fix)"\n\t" \
		"vext.8	d3, d3, d4, #"str_it(a_fix)"\n\t" \
		"vext.8	d4, d4, d5, #"str_it(a_fix)"\n\t" \
		"vext.8	d5, d5, d6, #"str_it(a_fix)"\n\t" \
		"vext.8	d6, d6, d7, #"str_it(a_fix)"\n\t" \
		"vext.8	d7, d7, d8, #"str_it(a_fix)"\n\t" \
		"vext.8	d10, d10, d11, #"str_it(b_fix)"\n\t" \
		"vext.8	d11, d11, d12, #"str_it(b_fix)"\n\t" \
		"vext.8	d12, d12, d13, #"str_it(b_fix)"\n\t" \
		"vext.8	d13, d13, d14, #"str_it(b_fix)"\n\t" \
		"vext.8	d14, d14, d15, #"str_it(b_fix)"\n\t" \
		"vext.8	d15, d15, d16, #"str_it(b_fix)"\n\t" \
		"vext.8	d16, d16, d17, #"str_it(b_fix)"\n\t" \
		"vext.8	d17, d17, d18, #"str_it(b_fix)"\n\t" \
		"veor	q0, q5\n\t" \
		"veor	q1, q6\n\t" \
		"veor	q2, q7\n\t" \
		"veor	q3, q8\n\t" \
		"vstmia.i64	%0!, {d0-d7}\n\t" \
		"pld	[%0, #64]\n\t" \
		"bne	1b\n" \
		"4:\n\t" \
		"cmp	%3, #0\n\t" \
		"beq	3f\n" \
		"2:\n\t" \
		"sub	%3, %3, #1\n\t" \
		"vorr	d0, d8, d8\n\t" \
		"vorr	d10, d18, d18\n\t" \
		"vldmia.i64	%1!, {d8}\n\t" \
		"vldmia.i64	%4!, {d18}\n\t" \
		"vext.8	d0, d0, d8, #"str_it(a_fix)"\n\t" \
		"vext.8	d10, d10, d18, #"str_it(b_fix)"\n\t" \
		"veor	d0, d10\n\t" \
		"vstmia.i64	%0!, {d0}\n\t" \
		"bne	2b\n" \
		"3:\n\t" \
		"sub	%1, %1, #8\n\t" \
		: /* %0 */ "=r" (dst_vec), \
		  /* %1 */ "=r" (src_vec2), \
		  /* %2 */ "=r" (smaller_len), \
		  /* %3 */ "=r" (small_len), \
		  /* %4 */ "=r" (src_vec1) \
		: /* %5 */ "0" (dst_vec), \
		  /* %6 */ "1" (src_vec2 + 1), \
		  /* %7 */ "2" (smaller_len), \
		  /* %8 */ "3" (small_len), \
		  /* %9 */ "4" (src_vec1 + 1) \
		: "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "q8", "q9" \
	);
#define SW_BODY(a_fix, b_fix) \
		switch(b_fix) { \
		case 1: INNER_SW_BODY(a_fix, 1) break; \
		case 2: INNER_SW_BODY(a_fix, 2) break; \
		case 3: INNER_SW_BODY(a_fix, 3) break; \
		case 4: INNER_SW_BODY(a_fix, 4) break; \
		case 5: INNER_SW_BODY(a_fix, 5) break; \
		case 6: INNER_SW_BODY(a_fix, 6) break; \
		case 7: INNER_SW_BODY(a_fix, 7) break; \
		}
		switch(a_fix)
		{
		case 1:
			SW_BODY(1, b_fix)
			break;
		case 2:
			SW_BODY(2, b_fix)
			break;
		case 3:
			SW_BODY(3, b_fix)
			break;
		case 4:
			SW_BODY(4, b_fix)
			break;
		case 5:
			SW_BODY(5, b_fix)
			break;
		case 6:
			SW_BODY(6, b_fix)
			break;
		case 7:
			SW_BODY(7, b_fix)
			break;
		}
#undef INNER_SW_BODY
#undef SW_BODY

// TODO: Hmmm, how many bytes are left???
		len %= SOVUC;
		dst_char  = (char *) dst_vec;
		src_char1  = (const char *) src_vec1;
		src_char2  = (const char *) src_vec2;
		goto handle_remaining;
	}
#else
alignment_16:
alignment_8:
alignment_size_t:
/* All archs fallback-code
 * (hmmm, runs 3 times faster on Sparc)
 */
	if(len/SOST)
	{
		register size_t *dst_sizet = (size_t *)dst_char;
		register const size_t *src_sizet1 = (const size_t *)src_char1;
		register const size_t *src_sizet2 = (const size_t *)src_char2;
		register size_t small_len = len / SOST;
		len %= SOST;

		while(small_len--)
			*dst_sizet++ = *src_sizet1++ ^ *src_sizet2++;

		dst_char  = (char *) dst_sizet;
		src_char1 = (const char *) src_sizet1;
		src_char2 = (const char *) src_sizet2;
		goto handle_remaining;
	}
no_alignment_wanted:
	if(len/SOST)
	{
		size_t small_len, cycles;
		size_t *dst_sizet = (size_t *)dst_char;

		cycles = small_len = (len / SOST) - 1;
		if(dst_char == src_char1) /* we are actually called for 2 args... */
		{
			const size_t *src_sizet;
			register size_t c, c_ex;
			register unsigned shift1, shift2;

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

				if(HOST_IS_BIGENDIAN)
					c = c << shift1 | c_ex >> shift2;
				else
					c = c >> shift1 | c_ex << shift2;

				*dst_sizet++ ^= c;
			}
		}
		else
		{
			const size_t *src_sizet1, *src_sizet2;
			register size_t c1, c_ex1, c2, c_ex2;
			register unsigned shift11, shift12, shift21, shift22;

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
					c1 = c1 << shift11 | c_ex1 >> shift12;
					c2 = c2 << shift21 | c_ex2 >> shift22;
				} else {
					c1 = c1 >> shift11 | c_ex1 << shift12;
					c2 = c2 >> shift21 | c_ex2 << shift22;
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
no_alignment_possible:
handle_remaining:
	/* xor whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ = *src_char1++ ^ *src_char2++;

	return dst;
}

static char const rcsid_mx[] GCC_ATTR_USED_VAR = "$Id:$";
