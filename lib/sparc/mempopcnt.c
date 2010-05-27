/*
 * mempopcnt.c
 * popcount a mem region, sparc/sparc64 implementation
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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
	const size_t *p;
	size_t r;
	unsigned shift = ALIGN_DOWN_DIFF(s, SOST);
	prefetch(s);

	p = (const size_t *)ALIGN_DOWN(s, SOST);
	r = *p;
	if(!HOST_IS_BIGENDIAN)
		r >>= shift * BITS_PER_CHAR;
	else
		r <<= shift * BITS_PER_CHAR;
	if(len >= SOST || len + shift >= SOST)
	{
		p++;
		len -= SOST - shift;
		write_bmask1(r);
		sum = pdist(bshuffle(m0, m1), v0, sum);
		if(SOST > SO32) {
			write_bmask1(r >> 32);
			sum = pdist(bshuffle(m0, m1), v0, sum);
		}

		if(len >= 8 * SOST)
		{
			size_t ones, twos, fours, eights;
			unsigned long long sum_t;

			sum_t = v0;
			fours = twos = ones = 0;
			while(len >= 8 * SO32)
			{
				unsigned long long sumb = v0;

				r    = len / (8 * SOST);
// TODO: 31 rounds till sumb overflows is a guess...
				r    = r > 31 ? 31 : r;
				len -= r * 8 * SOST;
				for(; r; r--, p += 8)
				{
					size_t twos_l, twos_h, fours_l, fours_h;
					unsigned long long w;

#define CSA(h,l, a,b,c) \
	{size_t u = a ^ b; size_t v = c; \
	 h = (a & b) | (u & v); l = u ^ v;}
					CSA(twos_l, ones, ones, p[0], p[1])
					CSA(twos_h, ones, ones, p[2], p[3])
					CSA(fours_l, twos, twos, twos_l, twos_h)
					CSA(twos_l, ones, ones, p[4], p[5])
					CSA(twos_h, ones, ones, p[6], p[7])
					CSA(fours_h, twos, twos, twos_l, twos_h)
					CSA(eights, fours, fours, fours_l, fours_h)
#undef CSA

					write_bmask1(eights);
					w = bshuffle(m0, m1);
					if(SOST > SO32) {
						write_bmask1(eights >> 32);
						w = fpadd32(w, bshuffle(m0, m1));
					}
					sumb = fpadd32(w, sumb);
				}
				sum_t = pdist(sumb, v0, sum_t);
			}
			sum += 8 * sum_t;
			{
				unsigned long long u, w, v;

				write_bmask1(ones);
				u = bshuffle(m0, m1);

				write_bmask1(twos);
				v = bshuffle(m0, m1);
				v = fpadd32(v, v); /* * 2 */

				write_bmask1(fours);
				w = bshuffle(m0, m1);
				w = fpadd32(w, w); /* * 2 */
				w = fpadd32(w, w); /* * 2 */

				sum = pdist(u, v0, sum);
				sum = pdist(v, v0, sum);
				sum = pdist(w, v0, sum);
			}
		}

		if(len >= SOST)
		{
			unsigned long long sumb = v0;

			r    = len / SOST; /* len is now at most 64 byte, ok for sumb */
			len %= SOST;
			for(; r; r--, p++)
			{
				unsigned long long w;
				size_t c = p[0];

				write_bmask1(c);
				w = bshuffle(m0, m1);
				if(SOST > SO32) {
					write_bmask1(c >> 32);
					w = fpadd32(w, bshuffle(m0, m1));
				}
				sumb = fpadd32(w, sumb);
			}
			sum = pdist(sumb, v0, sum);
		}
		if(len)
			r =*p;
	}
	if(len) {
		if(!HOST_IS_BIGENDIAN)
			r <<= (SOST - len) * BITS_PER_CHAR;
		else
			r >>= (SOST - len) * BITS_PER_CHAR;
		write_bmask1(r);
		sum = pdist(bshuffle(m0, m1), v0, sum);
		if(SOST > SO32) {
			write_bmask1(r >> 32);
			sum = pdist(bshuffle(m0, m1), v0, sum);
		}
	}
	return sum;
}
# else
#  if 0
/* =================================================================
 * do NOT use these sparc instructions, they are glacialy slow, only
 * for reference. Generic is 50 times faster (and now +33%).
 * =================================================================
 *
 * Really, no kidding, the problem is NOT that they are NOT in
 * hardware, they seem to use the most DUMB way to popcnt (it's sooo
 * slow, that's the only way to explain it, it must be some word size
 * times [shift + test bit -> inc popcnt]).
 * Since they are in microcode, they would be fixable (at least in my book),
 * but there is no indication of that, (not that i know of, maybe it's
 * like "it has an instruction, don't bother, it MUST be fast", and
 * none ever mesured it (maybe there is some new microcode in the
 * package for the NSA...)).
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
# endif
#else
# include "../generic/mempopcnt.c"
#endif
