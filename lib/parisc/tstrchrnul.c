/*
 * tstrchrnul.c
 * tstrchrnul for non-GNU platforms, parisc implementation
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

tchar_t *tstrchrnul(const tchar_t *s, tchar_t c)
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
	mask = (((size_t)c) & 0xFFFF) * MK_C(0x00010001);
	p  = (const char *)ALIGN_DOWN(s, SOUL);
	shift = ALIGN_DOWN_DIFF(s, SOUL);
	x1 = *(const unsigned long *)p;
	x2 = x1 ^ mask;
	if(!HOST_IS_BIGENDIAN) {
		x1 |= (~0UL) >> ((SOUL - shift) * BITS_PER_CHAR);
		x2 |= (~0UL) >> ((SOUL - shift) * BITS_PER_CHAR);
	} else {
		x1 |= (~0UL) << ((SOUL - shift) * BITS_PER_CHAR);
		x2 |= (~0UL) << ((SOUL - shift) * BITS_PER_CHAR);
	}
	t1 = pa_is_zw(x1);
	t2 = pa_is_zw(x2);
	if(t1 || t2)
		goto OUT;

	asm(
		PA_LD",ma	"PA_TZ"(%0), %1\n\t"
		"1:\n\t"
		"uxor,"PA_NHZ"	%1, %4, %2\n\t"
		"b	2f\n\t"
		"uxor,"PA_NHZ"	%1, %%r0, %%r0\n\t"
		"b,n	2f\n\t"
		"b	1b\n\t"
		PA_LD",ma	"PA_TZ"(%0), %1\n\t"
		"2:"
		: /* %0 */ "=&r" (p),
		  /* %1 */ "=&r" (x1),
		  /* %2 */ "=&r" (x2)
		: /* %3 */ "0" (p),
		  /* %4 */ "r" (mask)
	);
	t1 = pa_is_zw(x1);
	t2 = pa_is_zw(x2);
OUT:
	t1 = t1 ? pa_find_zw(x1) : (int)(SOUL/SOTC);
	t2 = t2 ? pa_find_zw(x2) : (int)(SOUL/SOTC);
	t1 = t1 < t2 ? t1 : t2;
	return (tchar_t *)(uintptr_t)p + t1;
}

static char const rcsid_tscnpa[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
