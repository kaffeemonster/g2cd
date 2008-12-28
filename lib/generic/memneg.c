/*
 * memneg.c
 * neg a memory region efficient, generic implementation
 *
 * Copyright (c) 2006 Jan Seiffert
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

void *memneg(void *dst, const void *src, size_t len)
{
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
	else
	{
		/* blindly align dst ... */
		size_t i = ALIGN_DIFF(dst_char, SOST);
		len -= i;
		for(; i; i--)
			*dst_char++ = ~(*src_char++);
		/* ... and check the outcome */
		i = (((intptr_t)dst_char) & ((SOST * 2) - 1)) ^
		    (((intptr_t)src_char) & ((SOST * 2) - 1));
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
			goto alignment_size_t;
		}
		else
		{
			/* uhoh... */
			if(!(i & SOSTM1))
				goto alignment_size_t;
		}
		goto no_alignment_possible
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
			*dst_sizet++ = ~(*src_sizet++);

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

			if(HOST_IS_BIGENDIAN) {
				c <<= shift1;
				c  |= c_ex >> shift2;
			} else {
				c >>= shift1;
				c  |= c_ex << shift2;
			}

			*dst_sizet++ = ~c;
		}

		dst_char  = (char *) dst_sizet;
		src_char += cycles * SOST;
		len      -= cycles * SOST;
		goto handle_remaining;
	}

no_alignment_wanted:
handle_remaining:
	/* neg whats left to do from alignment and other datatype */
	while(len--)
		*dst_char++ = ~(*src_char++);

	return dst;
}

static char const rcsid_mn[] GCC_ATTR_USED_VAR = "$Id:$";
