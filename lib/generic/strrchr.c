/*
 * strrchr.c
 * strrchr, generic implementation
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

/*
 * We could first do a strlen to find the end of the string,
 * then search backwards. But since we have to walk the string
 * to find the end anyway we could pick up the matches on the
 * way while we are at it.
 * And my GCC 4.4 fits it into the register set on x86-32 (wow)
 * and creates a quite nice loop. Yes, the loop has double the
 * instructions, but i guess it's still a win, cheap integer foo.
 */
char *strrchr(const char *s, int c)
{
	const char *p;
	size_t r, m, mask, x;
	struct {
		const char *p;
		size_t m;
	} l_match;
	unsigned shift;

	if(unlikely(!c))
		return (char *)(intptr_t)s + strlen(s);
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
	mask = (c & 0xFF) * MK_C(0x01010101);
	p  = (const char *)ALIGN_DOWN(s, SOST);
	shift = ALIGN_DOWN_DIFF(s, SOST) * BITS_PER_CHAR;
	x  = *(const size_t *)p;
	if(!HOST_IS_BIGENDIAN)
		x >>= shift;
	r  = has_nul_byte(x);
	x ^= mask;
	m  = has_nul_byte(x);
	r <<= shift;
	m <<= shift;
	if(HOST_IS_BIGENDIAN) {
		r >>= shift;
		m >>= shift;
	}
	l_match.p = p;
	l_match.m = m;

	while(!r)
	{
		if(m) {
			l_match.p = p;
			l_match.m = m;
		}
		p += SOST;
		x  = *(const size_t *)p;
		r  = has_nul_byte(x);
		x ^= mask;
		m  = has_nul_byte(x);
	}
	if(unlikely(!(m || l_match.m)))
		return NULL;
	if(m) {
		r = nul_byte_index(r);
		m = nul_byte_index(m);
		if(m < r)
			return (char *)(uintptr_t)p + m;
	}
	m = nul_byte_index(l_match.m);
	return (char *)(uintptr_t)l_match.p + m;
}

static char const rcsid_src[] GCC_ATTR_USED_VAR = "$Id: $";