/*
 * strnlen.c
 * strnlen for non-GNU platforms, generic implementation
 *
 * Copyright (c) 2005-2008 Jan Seiffert
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

size_t strnlen(const char *s, size_t maxlen)
{
	const char *p = s;
	size_t i;
	prefetch(s);

	if(unlikely(!s))
		return 0;

	if(UNALIGNED_OK)
	{
		/*
		 * We can do unaligned access, busily start doing
		 * something.
		 * We stop at a page boundry to get in a aligned
		 * swing on the source. (we simply assume 4k pages)
		 */
		if(IS_ALIGN(p, SO32))
			goto DO_LARGE;
		i = (const char *) ALIGN(p, 4096) - p;
		i = i < maxlen ? i : maxlen;
		for(; likely(SO32M1 < i); i -= SO32, maxlen -= SO32, p += SO32)
		{
			uint32_t r = has_nul_byte32(*(const uint32_t*)p);
			if(r) {
				p += nul_byte_index32(r);
				goto OUT;
			}
		}
		/* slowly go over the page boundry */
	}
	else /* Unaligned access is not ok. Align it before access. */
		i = (const char *) ALIGN(p, SO32) - p;

	while(i && maxlen && *p)
		maxlen--, p++, i--;
	if(!*p)
		goto OUT;

DO_LARGE:
	if(SO32M1 < maxlen)
	{
		/*
		 * keeping at 32 bit to give 64 bit arches a chance
		 * on short strings
		 */
		register const uint32_t *d = ((const uint32_t *)p);
		uint32_t r;

		while(!(r = has_nul_byte32(*d)) && SO32M1 < maxlen)
			d++, maxlen -= SO32;
		p = (const char *)d;
		if(r) {
			p += nul_byte_index32(r);
			goto OUT;
		}
	}

	while(maxlen && *p)
		maxlen--, p++;
OUT:
	return p - s;
}

static char const rcsid_snl[] GCC_ATTR_USED_VAR = "$Id: $";
