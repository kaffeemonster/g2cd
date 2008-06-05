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

#define has_nul_byte(x) \
	(((x) -  MK_C(0x01010101)) & ~(x) &  MK_C(0x80808080))

size_t strnlen(const char *s, size_t maxlen)
{
	const char *p = s;

	if(!s)
		return 0;

	if(!IS_ALIGN(p, SOST))
	{
		size_t i = (const char *) ALIGN(p, SOST) - p;
		while(maxlen && *p && i)
			maxlen--, p++, i--;
	}
	if(SOST > maxlen || !*p)
		goto OUT;

	{
	register const size_t *d = ((const size_t *)p);
	while(!has_nul_byte(*d) && SOSTM1 < maxlen)
		d++, maxlen -= SOST;
	p = (const char *)d;
	}

OUT:
	while(maxlen && *p)
		maxlen--, p++;

	return p - s;
}

static char const rcsid_snl[] GCC_ATTR_USED_VAR = "$Id: $";
