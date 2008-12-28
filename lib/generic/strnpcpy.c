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

#define ALIGN_WANTED SOST

char *strnpcpy(char *dst, const char *src, size_t maxlen)
{
	size_t i;
	size_t r;

	prefetch(src);
	prefetchw(dst);
	if(unlikely(!src || !dst || !maxlen))
		return dst;

	if(unlikely(maxlen < SOST))
		goto OUT;

	if(UNALIGNED_OK)
	{
		/*
		 * We can do unaligned access, busily start doing
		 * something.
		 * But make shure we do not cross a page boundry
		 * on the source side. (we simply assume 4k pages)
		 */
		if(IS_ALIGN(src, SOST))
			goto DO_LARGE;

		/* quickly go to the page boundry */
		i = ((char *)ALIGN(src, 4096) - src);
		i = i < maxlen ? i : maxlen;
		for(; likely(SOSTM1 < i); i -= SOST, maxlen -= SOST, src += SOST, dst += SOST)
		{
			size_t c = *(const size_t *)src;
			r = has_nul_byte(c);
			if(r)
				return cpy_rest0(dst, src, nul_byte_index(r));
			*(size_t *)dst = c;
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
		if(SOSTM1 & i)
			goto OUT;
		/* fallthrough */
	}

	/* Everything's aligned, life is good... */
DO_LARGE:
	if(likely(*src))
	{
		size_t *dst_b = (size_t *)dst;
		const size_t *src_b = (const size_t *)src;

		i = maxlen / SOST;
		for(; likely(i); i--, src_b++, dst_b++)
		{
			size_t c = *src_b;
			r = has_nul_byte(c);
			if(r)
				return cpy_rest0(dst, src, nul_byte_index(r));
			*dst_b = c;
		}

		maxlen = i * SOST + (maxlen % SOST);
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
