/*
 * strnlen.c
 * strnlen for non-GU platforms
 *
 * Copyright (c) 2005,2006 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2
 * of the License, or any later version.
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
 * strnlen - strlen with a n
 * s: string you need the length
 * maxlen: the maximum length
 *
 * return value: the string length or maxlen, what comes first
 *
 * since strnlen is a GNU extension we have to provide our own
 * on systems which do not provide one.
 */

#include "../config.h"
#include "../other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifndef STRNLEN_DEFINED
size_t strnlen(const char *s, size_t maxlen) GCC_ATTR_VIS("hidden");
#define STRNLEN_DEFINED
#endif

#define has_nul_byte(x) \
	(((x) -  MK_C(0x01010101)) & ~(x) &  MK_C(0x80808080))
inline size_t strnlen(const char *s, size_t maxlen)
{
	const char *p = s - 1;

	if(!s)
		return 0;
	maxlen++;

	do
	{
		p++;
		maxlen--;
		if(IS_ALIGN(p, SOST) && SOSTM1 < (maxlen - SOST))
		{
			register const size_t *d = ((const size_t *)p) - 1;
			maxlen += SOST;
			do
			{
				d++;
				maxlen -= SOST;
			} while(!has_nul_byte(*d) && SOSTM1 < maxlen);
			p = (const char *)d;
		}
	} while(maxlen && *p);

	return p - s;
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id: $";
