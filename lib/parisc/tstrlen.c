/*
 * tstrlen.c
 * tstrlen, parisc implementation
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

size_t tstrlen(const tchar_t *s)
{
	const char *p;
	unsigned long r;
	unsigned shift;
	int t;
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
	p = (const char *)ALIGN_DOWN(s, SOUL);
	shift = ALIGN_DOWN_DIFF(s, SOUL);
	r = *(const unsigned long *)p;
	if(!HOST_IS_BIGENDIAN)
		r |= (~0UL) >> ((SOUL - shift) * BITS_PER_CHAR);
	else
		r |= (~0UL) << ((SOUL - shift) * BITS_PER_CHAR);
	t = pa_is_zw(r);
	if(t)
		goto OUT;

	asm(
		"1:\n\t"
		PA_LD",ma	"PA_TZ"(%0), %1\n\t"
		"uxor,"PA_SHZ"	%1, %%r0, %%r0\n\t"
		"b	1b\n\t"
		PREFETCH("32(%0)")
		: /* %0 */ "=r" (p),
		  /* %1 */ "=r" (r)
		: /* %2 */ "0" (p)
	);
OUT:
	t = pa_find_zw(r);
	return (const tchar_t *)p - s + t;
}

static char const rcsid_tslpa[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
