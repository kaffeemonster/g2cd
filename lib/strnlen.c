/*
 * strnlen.c
 * strnlen for non-GU platforms
 *
 * Copyright (c) 2005,2006 Jan Seiffert
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
 * strnlen - strlen with a n
 * s: string you need the length
 * maxlen: the maximum length
 *
 * return value: the string length or maxlen, what comes first
 *
 * since strnlen is a GNU extension we have to provide our own.
 * We always provide one, since this implementation is only
 * marginaly slower than glibc one.
 *
 * Timing on Athlon X2 2.5GHz, 3944416 bytes, 1000 runs:
 * glibc: 5230ms
 *   our: 5350ms
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
