/*
 * memand.c
 * and two memory region efficient, arm implementation
 *
 * Copyright (c) 2005-2012 Jan Seiffert
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

#ifdef __ARM_NEON__
# include <arm_neon.h>
# include "my_neon.h"
# define ARCH_NAME_SUFFIX _neon
# define ALIGNMENT_WANTED SOVUC
#else
# define ARCH_NAME_SUFFIX _arm
# define ALIGNMENT_WANTED SOST
#endif

void *memand(void *dst, const void *src, size_t len)
{
	char *dst_char = dst;
	const char *src_char = src;

	if(!dst || !src)
		return dst;

	/* we will kick this mem, fetch it... */
	prefetchw(dst_char);
	prefetchw(dst_char + 32);
	prefetchw(dst_char + 64);
	prefetchw(dst_char + 96);

	prefetch(src_char);
	prefetch(src_char + 32);
	prefetch(src_char + 64);
	prefetch(src_char + 96);


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
			*dst_char++ &= (*src_char++);
		i = ALIGN_DOWN_DIFF(dst_char, ALIGNMENT_WANTED * 2) ^
		    ALIGN_DOWN_DIFF(src_char, ALIGNMENT_WANTED * 2);
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
		/* fall through */
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
		register const uint8x8_t *src_vec = (const uint8x8_t *) src_char;
		size_t small_len = len / SOVUC;
		register size_t smaller_len = small_len / 8;
		small_len %= 8;

		asm (
			"cmp	%2, #0\n\t"
			"beq	4f\n"
			"1:\n\t"
			"sub	%2, %2, #1\n\t"
			"vldmia.i64	%1!, {d0-d7}\n\t"
			"vldm.i64	%0, {d8-d15}\n\t"
			"pld	[%1, #64]\n\t"
			"vand	q4, q4, q0\n\t"
			"vand	q5, q5, q1\n\t"
			"vand	q6, q6, q2\n\t"
			"vand	q7, q7, q3\n\t"
			"vstmia.i64	%0!, {d8-d15}\n\t"
			"pld	[%0, #64]\n\t"
			"bne	1b\n"
			"4:\n\t"
			"cmp	%3, #0\n\t"
			"beq	3f\n"
			"2:\n\t"
			"sub	%3, %3, #1\n\t"
			"vldmia.i64	%1!, {d0}\n\t"
			"vldm.i64	%0, {d1}\n\t"
			"vand	d1, d1, d0\n\t"
			"vstmia.i64	%0!, {d1}\n\t"
			"bne	2b\n"
			"3:"
			: /* %0 */ "=r" (dst_vec),
			  /* %1 */ "=r" (src_vec),
			  /* %2 */ "=r" (smaller_len),
			  /* %3 */ "=r" (small_len)
			: /* %4 */ "0" (dst_vec),
			  /* %5 */ "1" (src_vec),
			  /* %6 */ "2" (smaller_len),
			  /* &7 */ "3" (small_len)
			: "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7"
		);

		len %= SOVUC;
		dst_char  = (char *) dst_vec;
		src_char  = (const char *) src_vec;
		goto handle_remaining;
	}
alignment_size_t:
no_alignment_wanted:

#define SW_BODY(a_fix) \
	asm ( \
		"vldr	d8, [%1, #-8]\n\t" \
		"cmp	%2, #0\n\t" \
		"beq	4f\n" \
		"1:\n\t" \
		"sub	%2, %2, #1\n\t" \
		"vorr	d0, d8, d8\n\t" \
		"vldmia.i64	%1!, {d1-d8}\n\t" \
		"vldm.i64	%0, {d10-d17}\n\t" \
		"pld	[%1, #64]\n\t" \
		"vext.8	d0, d0, d1, #"str_it(a_fix)"\n\t" \
		"vext.8	d1, d1, d2, #"str_it(a_fix)"\n\t" \
		"vext.8	d2, d2, d3, #"str_it(a_fix)"\n\t" \
		"vext.8	d3, d3, d4, #"str_it(a_fix)"\n\t" \
		"vext.8	d4, d4, d5, #"str_it(a_fix)"\n\t" \
		"vext.8	d5, d5, d6, #"str_it(a_fix)"\n\t" \
		"vext.8	d6, d6, d7, #"str_it(a_fix)"\n\t" \
		"vext.8	d7, d7, d8, #"str_it(a_fix)"\n\t" \
		"vand	q0, q5\n\t" \
		"vand	q1, q6\n\t" \
		"vand	q2, q7\n\t" \
		"vand	q3, q8\n\t" \
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
		"vldm.i64	%0, {d1}\n\t" \
		"vext.8	d0, d0, d8, #"str_it(a_fix)"\n\t" \
		"vand	d0, d1\n\t" \
		"vstmia.i64	%0!, {d0}\n\t" \
		"bne	2b\n" \
		"3:\n\t" \
		"sub	%1, %1, #8\n\t" \
		: /* %0 */ "=r" (dst_vec), \
		  /* %1 */ "=r" (src_vec), \
		  /* %2 */ "=r" (smaller_len), \
		  /* %3 */ "=r" (small_len) \
		: /* %4 */ "0" (dst_vec), \
		  /* %5 */ "1" (src_vec + 1), \
		  /* %6 */ "2" (smaller_len), \
		  /* &7 */ "3" (small_len) \
		: "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "q8", "q9" \
	);

	if(len/(4*SOVUC))
	{
		/* dst is aligned */
		register uint8x8_t *dst_vec = (uint8x8_t *) dst_char;
		/* only src sucks */
		register const uint8x8_t *src_vec;
		size_t small_len = (len / SOVUC) - 1; /* make sure not to overread */
		register size_t smaller_len = small_len / 8;
		unsigned a_fix;
		small_len %= 8;

		a_fix = (unsigned)ALIGN_DOWN_DIFF(src_char, SOVUC);
		src_vec = (const uint8x8_t *)ALIGN_DOWN(src_char, SOVUC);
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
		dst_char  = (char *) dst_vec;
		src_char  = (const char *) src_vec;
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
		register const size_t *src_sizet = (const size_t *)src_char;
		register size_t small_len = len / SOST;
		len %= SOST;

		while(small_len--)
			*dst_sizet++ &= (*src_sizet++);

		dst_char = (char *) dst_sizet;
		src_char = (const char *) src_sizet;
		goto handle_remaining;
	}
no_alignment_wanted:
	if(len/(4*SOST))
	{
		size_t *dst_sizet = (size_t *)dst_char;
		const size_t *src_sizet;
		register size_t c, c_ex, small_len, cycles;
		register unsigned shift1, shift2;

		cycles = small_len = (len / SOST) - 1;
		shift1 = (unsigned) ALIGN_DOWN_DIFF(src_char, SOST);
		shift2 = SOST - shift1;
		shift1 *= BITS_PER_CHAR;
		shift2 *= BITS_PER_CHAR;
		src_sizet = (const size_t *)ALIGN_DOWN(src_char, SOST);
		c_ex = *src_sizet++;
		while(small_len--)
		{
			c = c_ex;
			c_ex = *src_sizet++;

			if(HOST_IS_BIGENDIAN)
				c = c << shift1 | c_ex >> shift2;
			else
				c = c >> shift1 | c_ex << shift2;

			*dst_sizet++ &= c;
		}

		dst_char  = (char *) dst_sizet;
		src_char += cycles * SOST;
		len      -= cycles * SOST;
		goto handle_remaining;
	}
#endif
no_alignment_possible:
handle_remaining:
	/* and whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ &= (*src_char++);

	return dst;
}

static char const rcsid_mx[] GCC_ATTR_USED_VAR = "$Id:$";
