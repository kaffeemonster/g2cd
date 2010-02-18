/*
 * tstrchrnul.c
 * tstrchrnul, ia64 implementation
 *
 * Copyright (c) 2010 Jan Seiffert
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

#include "ia64.h"
#define TCINSLL (SOULL/sizeof(tchar_t))

tchar_t *tstrchrnul(const tchar_t *s, tchar_t c)
{
	const char *p;
	unsigned long long r1, r2, mask, x1, x2;
	unsigned shift;
	prefetch(s);

	/*
	 * Sometimes you need a new perspective, like the altivec
	 * way of handling things.
	 * Lower address bits? Totaly overestimated.
	 *
	 * We don't precheck for alignment.
	 * Instead we "align hard", do one load "under the address",
	 * mask the excess info out and afterwards we are fine to go.
	 */
	mask = (((size_t)c) & 0xFFFF) * 0x0001000100010001ULL;
	p  = (const char *)ALIGN_DOWN(s, SOULL);
	shift = ALIGN_DOWN_DIFF(s, SOULL) * BITS_PER_CHAR;
	x1 = *(const unsigned long long *)p;
	x2 = x1 ^ mask;
	if(!HOST_IS_BIGENDIAN) {
		x1 |= (~0ULL) >> ((SOULL - shift) * BITS_PER_CHAR);
		x2 |= (~0ULL) >> ((SOULL - shift) * BITS_PER_CHAR);
	} else {
		x1 |= (~0ULL) << ((SOULL - shift) * BITS_PER_CHAR);
		x2 |= (~0ULL) << ((SOULL - shift) * BITS_PER_CHAR);
	}
	r1 = czx2(x1);
	r2 = czx2(x2);
	if(r1 < TCINSLL)
		return ((tchar_t *)(uintptr_t)p) + r1;
	if(r2 < TCINSLL)
		return ((tchar_t *)(uintptr_t)p) + r2;

	do
	{
		p += SOULL;
		x1 = *(const size_t *)p;
		x2 = x1 ^ mask;
		r1 = czx2(x1);
		r2 = czx2(x2);
	} while(r1 == TCINSLL && r2 == TCINSLL);
	r1 = r1 < r2 ? r1 : r2;
	return ((tchar_t *)(uintptr_t)p) + r1;
}

static char const rcsid_tscnia64[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
