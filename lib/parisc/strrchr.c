/*
 * strrchr.c
 * strrchr, parisc implementation
 *
 * Copyright (c) 2010-2012 Jan Seiffert
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
/*
 * We could first do a strlen to find the end of the string,
 * then search backwards. But since we have to walk the string
 * to find the end anyway we could pick up the matches on the
 * way while we are at it.
 */
char *strrchr(const char *s, int c)
{
	const char *p, *l_match;
	unsigned long mask, x1, x2;
	unsigned shift;
	int l_m;
	prefetch(s);

	if(unlikely(!c)) /* make sure we do not compare with 0 */
		return (char *)(intptr_t)s + strlen(s);

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
	x2 = x1;
	if(shift)
	{
		if(!HOST_IS_BIGENDIAN) {
			x1 |= (~0UL) >> ((SOUL - shift) * BITS_PER_CHAR);
			x2 &= (~0UL) << (shift * BITS_PER_CHAR);
		} else {
			x1 |= (~0UL) << ((SOUL - shift) * BITS_PER_CHAR);
			x2 &= (~0UL) >> (shift * BITS_PER_CHAR);
		}
	}
	x2 ^= mask;
	l_match = p;
	l_m = ~0UL;

	if(!pa_is_z(x1))
	{
		asm(
			PA_LD",ma	"PA_TZ"(%0), %1\n\t"
			"1:\n\t"
			"uxor,"PA_SBZ"	%2, %%r0, %%r0\n\t"
			"b,n	3f\n\t"
			"copy	%2, %3\n\t"
			"addi	-"PA_TZ", %0, %4\n"
			"3:\n\t"
			"xor	%1, %6, %2\n\t"
			"uxor,"PA_NBZ"	%1, %%r0, %%r0\n\t"
			"b,n	2f\n\t"
			"b	1b\n\t"
			PA_LD",ma	"PA_TZ"(%0), %1\n"
			"2:"
			: /* %0 */ "=&r" (p),
			  /* %1 */ "=&r" (x1),
			  /* %2 */ "=&r" (x2),
			  /* %3 */ "=&r" (l_m),
			  /* %4 */ "=&r" (l_match)
			: /* %5 */ "0" (p),
			  /* %6 */ "r" (mask),
			  /* %7 */ "3" (l_m),
			  /* %8 */ "4" (l_match),
			  /* %9 */ "2" (x2)
		);
	}
	if(pa_is_z(x2))
	{
		int t1 = pa_find_z(x1);
		if(!HOST_IS_BIGENDIAN) {
			t1  = 1u << ((t1 * BITS_PER_CHAR)+BITS_PER_CHAR-1u);
			t1 |= t1 - 1u;
		} else {
			t1  = 1u << (((SOSTM1-t1) * BITS_PER_CHAR)+BITS_PER_CHAR-1u);
			t1 |= t1 - 1u;
			t1  = ~t1;
		}
		x2 |= ~t1;
		if(pa_is_z(x2))
			return (char *)(uintptr_t)p + pa_find_z_last(x2);
	}
	if(pa_is_z(l_m))
		return (char *)(uintptr_t)l_match + pa_find_z_last(l_m);
	return NULL;
}

static char const rcsid_srcpa[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
