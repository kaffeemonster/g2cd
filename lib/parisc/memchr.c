/*
 * memchr.c
 * memchr, parisc implementation
 *
 * Copyright (c) 2011 Jan Seiffert
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

void *my_memchr(const void *s, int c, size_t n)
{
	const unsigned char *p;
	unsigned long r, t, mask;
	long f, k;
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
	f = ALIGN_DOWN_DIFF(s, SOUL);
	k = SOUL - f - (long)n;
	k = k > 0 ? k  : 0;
	mask = (((size_t)c) & 0xFF) * 0x0101010101010101ULL;

	p  = (const unsigned char *)ALIGN_DOWN(s, SOUL);
	r  = *(const unsigned long long *)p;
	r ^= mask;
	if(!HOST_IS_BIGENDIAN) {
		r |= (~0ULL) >> ((SOUL - f) * BITS_PER_CHAR);
		r |= (~0ULL) << ((SOUL - k) * BITS_PER_CHAR);
	} else {
		r |= (~0ULL) << ((SOUL - f) * BITS_PER_CHAR);
		r |= (~0ULL) >> ((SOUL - k) * BITS_PER_CHAR);
	}
	t = pa_is_z(r);
	if(t) {
		t = pa_find_z(r);
		return (char *)(uintptr_t)p + t;
	} else if(k)
		return NULL;

	n -= SOUL - f;
	asm (
		"1:\n\t"
		PA_LD",ma	"PA_TZ"(%0), %1\n\t"
		"cmpib,>>=	"PA_TZ"-1, %2, 2f\n\t"
		"uxor,"PA_NBZ"	%1, %3, %%r0\n\t"
		"b,n	2f\n\t"
		"b	1b\n\t"
		"addi	-"PA_TZ", %2, %2\n"
		"2:"
		: "=&r" (p),
		  "=&r" (r),
		  "=&r" (n)
		: "r" (mask),
		  "0" (p),
		  "2" (n)
	);
	r ^= mask;
	k = SOUL - (long)n;
	k = k > 0 ? k : 0;
	if(k)
	{
		if(!HOST_IS_BIGENDIAN)
			r |= (~0ULL) << ((SOUL - k) * BITS_PER_CHAR);
		else
			r |= (~0ULL) >> ((SOUL - k) * BITS_PER_CHAR);
	}
	t = pa_is_z(r);
	if(!t)
		return NULL;
	t = pa_find_z(r);
	return (char *)(uintptr_t)p + t;
}

static char const rcsid_mcia64[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
