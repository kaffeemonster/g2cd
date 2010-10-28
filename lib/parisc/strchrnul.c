/*
 * strchrnul.c
 * strchrnul for non-GNU platforms, parisc implementation
 *
 * Copyright (c) 2010 Jan Seiffert
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
 * $Id: $
 */

#include "parisc.h"

char *strchrnul(const char *s, int c)
{
	const char *p;
	unsigned long mask, x1, x2;
	unsigned shift;
	int t1, t2;
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
	mask = (((size_t)c) & 0xFF) * MK_C(0x01010101);
	p  = (const char *)ALIGN_DOWN(s, SOUL);
	shift = ALIGN_DOWN_DIFF(s, SOUL);
	x1 = *(const unsigned long *)p;
	x2 = x1 ^ mask;
	if(!HOST_IS_BIGENDIAN) {
		x1 |= (~0ULL) >> ((SOUL - shift) * BITS_PER_CHAR);
		x2 |= (~0ULL) >> ((SOUL - shift) * BITS_PER_CHAR);
	} else {
		x1 |= (~0ULL) << ((SOUL - shift) * BITS_PER_CHAR);
		x2 |= (~0ULL) << ((SOUL - shift) * BITS_PER_CHAR);
	}
	t1 = pa_is_z(x1);
	t2 = pa_is_z(x2);
	if(t1 || t2)
		goto OUT;

	asm(
		PA_LD",ma	"PA_TZ"(%0), %1\n\t"
		"1:\n\t"
		"uxor,"PA_NBZ"	%1, %4, %2\n\t"
		"b	2f\n\t"
		"uxor,"PA_NBZ"	%1, %%r0, %%r0\n\t"
		"b,n	2f\n\t"
		"b	1b\n\t"
		PA_LD",ma	"PA_TZ"(%0), %1\n\t"
		"2:"
		: "=&r" (p),
		  "=&r" (x1),
		  "=&r" (x2)
		: "0" (p),
		  "r" (mask)
	);
	t1 = pa_is_z(x1);
	t2 = pa_is_z(x2);
OUT:
	t1 = t1 ? pa_find_z(x1) : (int)SOUL;
	t2 = t2 ? pa_find_z(x2) : (int)SOUL;
	t1 = t1 < t2 ? t1 : t2;
	return (char *)(uintptr_t)p + t1;
}

static char const rcsid_scnpa[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
