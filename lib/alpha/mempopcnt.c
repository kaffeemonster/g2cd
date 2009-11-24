/*
 * mempopcnt.c
 * popcount a mem region, alpha implementation
 *
 * Copyright (c) 2009 Jan Seiffert
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


static inline size_t popcountst_int(size_t n)
{
	size_t tmp;
	__asm__ ("ctpop\t%1, %0\n" : "=r" (tmp) : "r" (n));
	return tmp;
}

size_t mempopcnt(const void *s, size_t len)
{
	const unsigned char *p;
	size_t r;
	size_t sum = 0;
	unsigned shift = ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR;
	prefetch(s);

	p = (const unsigned char *)ALIGN_DOWN(s, SOST);
	r = *(const size_t *)p;
	r >>= shift;
	if(len >= SOST || len + shift >= SOST)
	{
		/*
		 * Sometimes you need a new perspective, like the altivec
		 * way of handling things.
		 * Lower address bits? Totaly overestimated.
		 *
		 * We don't precheck for alignment.
		 * Instead we "align hard", do one load "under the address",
		 * mask the excess info out and afterwards we are fine to go.
		 */
		p += SOST;
		len -= SOST - shift;
		sum += popcountst_int(r);

		r = len / SOST;
		for(; r; r--, p += SOST)
			sum += popcountst_int(*(const size_t *)p);
		len %= SOST;
		if(len)
			r = *(const size_t *)p;
	}
	if(len) {
		r <<= SOST - len;
		sum += popcountst_int(r);
	}
	return sum;
}

static char const rcsid_mpa[] GCC_ATTR_USED_VAR = "$Id: $";
