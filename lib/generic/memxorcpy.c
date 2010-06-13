/*
 * memxorcpy.c
 * xor two memory region efficient and cpy to dest, generic implementation
 *
 * Copyright (c) 2004-2010 Jan Seiffert
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

void *memxorcpy(void *dst, const void *src1, const void *src2, size_t len)
{
	char *dst_char = dst;
	const char *src_char1 = src1;
	const char *src_char2 = src2;

	if(!dst || !src1 || !src2)
		return dst;

	if(SYSTEM_MIN_BYTES_WORK > len)
		goto no_alignment_wanted;
	else
	{
		/* blindly align dst ... */
		size_t i = ALIGN_DIFF(dst_char, SOST);
		len -= i;
		for(; i; i--)
			*dst_char++ = *src_char1++ ^ *src_char2++;
		/* ... and check the outcome */
		i =  ALIGN_DOWN_DIFF(dst_char, SOST * 2) ^
		    (ALIGN_DOWN_DIFF(src_char1, SOST * 2) |
		     ALIGN_DOWN_DIFF(src_char2, SOST * 2));
		if(!(i & SOSTM1))
			goto alignment_size_t;
		if(UNALIGNED_OK)
		{
			/*
			 * we can do unaligned access quite ok.
			 * Since dst, the write side, is now aligned
			 * we simply do it with a bigger size,
			 * which should still be beneficial.
			 *
			 * !! BUT BEWARE OF BAD IMITATION !!
			 * Software fixup (exeption handler etc.) will kill
			 * the performance!
			 * Don't set UNALIGNED_OK on such an architecture.
			 */
			goto unaligned_ok;
		}
		/* uhoh... */
		goto no_alignment_possible;
	}

	/* fall throuh if alignment fails */
	goto no_alignment_possible;

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
unaligned_ok:
	if(UNALIGNED_OK)
	{
		register size_t small_len, c1, c2;
		register size_t *dst_sizet = (size_t *)dst_char;
		register const size_t *src_sizet2 = (const size_t *)src_char2;

		small_len = len / SOST;
		if(dst_char == src_char1) /* we are actually called for 2 args... */
		{
			len %= SOST;

			while(small_len--) {
				c1 = get_unaligned(src_sizet2);
				src_sizet2++;
				*dst_sizet ^= c1;
			}
			src_char1 = (const char *) dst_sizet;
		}
		else
		{
			register const size_t *src_sizet1 = (const size_t *)src_char1;
			len %= SOST;

			while(small_len--) {
				c1 = get_unaligned(src_sizet1);
				src_sizet1++;
				c2 = get_unaligned(src_sizet2);
				src_sizet2++;
				*dst_sizet = c1 ^ c2;
			}
			src_char1 = (const char *) src_sizet1;
		}
		dst_char = (char *) dst_sizet;
		src_char2 = (const char *) src_sizet2;
		goto handle_remaining;
	}
no_alignment_possible:
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

no_alignment_wanted:
handle_remaining:
	/* xor whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ = *src_char1++ ^ *src_char2++;

	return dst;
}

static char const rcsid_mxc[] GCC_ATTR_USED_VAR = "$Id:$";
