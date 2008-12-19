/*
 * strlen.c
 * strlen, generic implementation
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

size_t strlen(const char *s)
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
		 * But make shure we do not cross a page boundry
		 * on the source side. (we simply assume 4k pages)
		 */
		if(IS_ALIGN(p, SO32))
			goto DO_LARGE;
		i = ((const char *) ALIGN(p, 4096) - p);
		for(; likely(SO32M1 < i); i -= SO32, p += SO32)
		{
			uint32_t r = has_nul_byte32(*p);
			if(r) {
				p += nul_byte_index32(r);
				goto OUT;
			}
		}
		/* slowly go over the page boundry */
	}
	else /* Unaligned access is not ok. Align it before access. */
		i = (const char *) ALIGN(p, SO32) - p;
	while(i && *p)
		p++, i--;

DO_LARGE:
	if(*p)
	{
		/*
		 * keeping at 32 bit to give 64 bit arches a chance
		 * on short strings
		 */
		register const uint32_t *d = ((const uint32_t *)p);
		uint32_t r;

		while(!(r = has_nul_byte32(*d)))
			d++;
		p  = (const char *)d;
		p += nul_byte_index32(r);
	}

OUT:
	return p - s;
}

static char const rcsid_sl[] GCC_ATTR_USED_VAR = "$Id: $";
