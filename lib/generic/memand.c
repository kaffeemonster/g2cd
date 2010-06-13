/*
 * memand.c
 * and two memory region efficient, generic implementation
 *
 * Copyright (c) 2006-2010 Jan Seiffert
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

void *memand(void *dst, const void *src, size_t len)
{
	char *dst_char = dst;
	const char *src_char = src;

	if(!dst || !src)
		return dst;
	
	if(SYSTEM_MIN_BYTES_WORK > len)
		goto no_alignment_wanted;
	else
	{
		/* blindly align dst ... */
		size_t i = ALIGN_DIFF(dst_char, SOST);
		len -= i;
		for(; i; i--)
			*dst_char++ &= *src_char++;
		/* ... and check the outcome */
		i = ALIGN_DOWN_DIFF(dst_char, SOST * 2) ^ ALIGN_DOWN_DIFF(src_char, SOST * 2);
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
		register const size_t *src_sizet = (const size_t *)src_char;
		register size_t small_len = len / SOST;
		len %= SOST;

		while(small_len--)
			*dst_sizet++ &= *src_sizet++;

		dst_char = (char *) dst_sizet;
		src_char = (const char *) src_sizet;
		goto handle_remaining;
	}
unaligned_ok:
	if(UNALIGNED_OK)
	{
		register size_t *dst_sizet = (size_t *)dst_char;
		register const size_t *src_sizet = (const size_t *)src_char;
		register size_t small_len = len / SOST, c;
		len %= SOST;

		while(small_len--) {
			c = get_unaligned(src_sizet);
			src_sizet++;
			*dst_sizet++ &= c;
		}

		dst_char = (char *) dst_sizet;
		src_char = (const char *) src_sizet;
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

handle_remaining:
no_alignment_wanted:
	/* and whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ &= *src_char++;

	return dst;
}

static char const rcsid_ma[] GCC_ATTR_USED_VAR = "$Id:$";
