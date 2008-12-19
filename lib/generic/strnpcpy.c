/*
 * strnpcpy.c
 * strnpcpy for efficient concatination, generic implementation
 *
 * Copyright (c) 2008 Jan Seiffert
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
 * $Id: $
 */

/*
 * keep it 32 Bit, so even 64Bit arches have a chance to use
 * a broader datatype and not immediatly baile out
 */
#define ALIGN_WANTED SO32

char *strnpcpy(char *dst, const char *src, size_t maxlen)
{
	size_t i = 0;

	prefetch(src);
	prefetchw(dst);
	if(unlikely(!src || !dst || !maxlen))
		return dst;

	if(unlikely(maxlen < SO32))
		goto OUT;

	if(UNALIGNED_OK)
	{
		/*
		 * We can do unaligned access, busily start doing
		 * something.
		 * But make shure we do not cross a page boundry
		 * on the source side. (we simply assume 4k pages)
		 */
		if(IS_ALIGN(src, SO32))
			goto DO_LARGE;

		/* quickly go to the page boundry */
		i = ((char *)ALIGN(src, 4096) - src);
		i = i < maxlen ? i : maxlen;
		for(; likely(SO32M1 < i); i -= SO32, maxlen -= SO32, src += SO32, dst += SO32)
		{
			uint32_t c = *(const uint32_t *)src;
			if(has_nul_byte32(c))
				goto OUT;
			*(uint32_t *)dst = c;
		}

		/* slowly go over the page boundry */
		for(; i && *src; i--, maxlen--)
			*dst++ = *src++;
	}
	else
	{
		/*
		 * Unaligned access is not ok.
		 * Blindly try to align dst and check the outcome
		 */
		i = (char *)ALIGN(dst, ALIGN_WANTED) - dst;
		for(; maxlen && i && *src; maxlen--, i--)
			*dst++ = *src++;
		/* Now check which alignment we achieved */
		i = (((intptr_t)dst) & ((ALIGN_WANTED * 2) - 1)) ^
		    (((intptr_t)src) & ((ALIGN_WANTED * 2) - 1));
		if(1 & i)
			goto OUT;
		if(2 & i)
			goto OUT;
		/* fallthrough */
	}

	/* Everything's aligned, life is good... */
DO_LARGE:
	if(likely(*src))
	{
		uint32_t *dst_b = (uint32_t *)dst;
		const uint32_t *src_b = (const uint32_t *)src;

		i = maxlen / SO32;
		for(; likely(i) && !has_nul_byte32(*src_b); i--)
			*dst_b++ = *src_b++;

		maxlen = i * SO32 + (maxlen % SO32);
		dst = (char *)dst_b;
		src = (const char *)src_b;
	}

OUT:
	for(; maxlen && *src; maxlen--)
		*dst++ = *src++;
	if(likely(maxlen))
		*dst = '\0';
	return dst;
}

static char const rcsid_snpcg[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
