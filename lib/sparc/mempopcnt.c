/*
 * mempopcnt.c
 * popcount a mem region, sparc/sparc64 implementation
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

/*
 * gcc sets __sparcv8 even if you say "gimme v9" to not confuse solaris
 * tools and signal "32 bit mode". So how to detect a real v9 to do
 * v9ish stuff, mister sun? Great tennis! This will Bomb on a real v8...
 */
#if defined(__sparcv8) || defined(__sparc_v8__) || defined(__sparcv9) || defined(__sparc_v9__)
# ifdef HAVE_VIS2
#  include "sparc_vis.h"

size_t mempopcnt(const void *s, size_t len)
{
	static const unsigned long long m[] =
		{0x0001010201020203ULL, 0x0102020302030304ULL};
	unsigned long long m0 = m[0], m1 = m[1], v0 = fzero();
	unsigned long long sum = v0;
	const uint32_t *p;
	uint32_t r;
	unsigned shift = ALIGN_DOWN_DIFF(s, SO32);
	prefetch(s);

	p = (const uint32_t *)ALIGN_DOWN(s, SO32);
	r = *p;
	if(!HOST_IS_BIGENDIAN)
		r >>= shift * BITS_PER_CHAR;
	else
		r <<= shift * BITS_PER_CHAR;
	if(len >= SO32 || len + shift >= SO32)
	{
		p++;
		len -= SO32 - shift;
		write_bmask1(r);
		sum = pdist(bshuffle(m0, m1), v0, sum);

		while(len >= SO32 * 4)
		{
			unsigned long long sumb = v0;
			size_t i;

			i    = len / (SO32 * 4);
			i    = i > 7 ? 7 : i;
			len -= i * SO32 * 4;
			for(; i; i--, p += 4) {
				uint32_t a = p[0], b = p[1], c = p[2], d = p[3];
				unsigned long long w, x, y, z;
				write_bmask1(a);
				w = bshuffle(m0, m1);
				write_bmask1(b);
				x = bshuffle(m0, m1);
				w = fpadd32(w, x);
				write_bmask1(c);
				y = bshuffle(m0, m1);
				sumb = fpadd32(w, sumb);
				write_bmask1(d);
				z = bshuffle(m0, m1);
				y = fpadd32(y, z);
				sumb = fpadd32(y, sumb);
			}
			sum = pdist(sumb, v0, sum);
		}
		if(len >= SO32 * 2) {
			write_bmask1(p[0]);
			sum = pdist(bshuffle(m0, m1), v0, sum);
			write_bmask1(p[1]);
			sum = pdist(bshuffle(m0, m1), v0, sum);
			p += 2;
			len -= SO32 * 2;
		}
		if(len >= SO32) {
			write_bmask1(p[0]);
			sum = pdist(bshuffle(m0, m1), v0, sum);
			p++;
			len -= SO32;
		}
		if(len)
			r =*p;
	}
	if(len) {
		if(!HOST_IS_BIGENDIAN)
			r <<= (SO32 - len) * BITS_PER_CHAR;
		else
			r >>= (SO32 - len) * BITS_PER_CHAR;
		write_bmask1(r);
		sum = pdist(bshuffle(m0, m1), v0, sum);
	}
	return sum;
}
# else
#  if 0
/* =================================================================
 * do NOT use these sparc instructions, they are glacialy slow, only
 * for reference. Generic is 50 times faster.
 * =================================================================
 *
 * Fujitsu promised for the new UltraSPARC IIIfx that popcount will
 * be in hardware, we will see...
 */
static inline size_t popcountst_int1(size_t n)
{
	size_t tmp;
	__asm__ ("popc\t%1, %0\n" : "=r" (tmp) : "r" (n));
	return tmp;
}

static inline size_t popcountst_int2(size_t n, size_t m)
{
	return popcountst_int1(n) +
	       popcountst_int1(m);
}

static inline size_t popcountst_int4(size_t n, size_t m, size_t o, size_t p)
{
	return popcountst_int1(n) +
	       popcountst_int1(m) +
	       popcountst_int1(o) +
	       popcountst_int1(p);
}

#  define NO_GEN_POPER
# endif
# include "../generic/mempopcnt.c"
static char const rcsid_mps[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/mempopcnt.c"
#endif
