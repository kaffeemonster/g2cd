/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   arm implementation
 * Copyright (C) 1995-2004 Mark Adler
 * Copyright (C) 2010-2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/*
 * This is not the original adler32.c from the zlib distribution,
 * but a heavily modified version. If you are looking for the
 * original, please go to zlib.net.
 */

#define HAVE_ADLER32_VEC
#define NO_DIVIDE
static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len);
#define MIN_WORK 16

#include "../generic/adler32.c"
#include "alpha.h"

#define VNMAX (2*NMAX)
#if defined(__alpha_max__)
static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1, s2;
	unsigned k;

	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;

	k    = ALIGN_DIFF(buf, SOUL);
	len -= k;
	if(k) do {
		s1 += *buf++;
		s2 += s1;
	} while (--k);

	if(likely(len >= 2 * SOUL))
	{
		unsigned long vs1 = s1, vs2 = s2;

		k = len < VNMAX ? (unsigned) len : VNMAX;
		len -= k;

		do
		{
			unsigned long vs1_r = 0;
			do
			{
				unsigned long a, b, c, d, e, f, g, h;
				unsigned long vs2l = 0, vs2h = 0;
				unsigned j;

				j = k > 257 * SOUL ? 257 : k/SOUL;
				k -= j * SOUL;
				do
				{
					/* get input data */
					unsigned long in = *(const unsigned long *)buf;
					/* add vs1 for this round */
					vs1_r += vs1;
					/* add horizontal */
					vs1 += perr(in, 0);
					/* extract */
					vs2l += unpkbw(in);
					vs2h += unpkbw(in >> 32);
					buf += SOUL;
				} while(--j);
				if(HOST_IS_BIGENDIAN)
				{
					a = (vs2h >> 48) & 0x0000ffff;
					b = (vs2h >> 32) & 0x0000ffff;
					c = (vs2h >> 16) & 0x0000ffff;
					d = (vs2h      ) & 0x0000ffff;
					e = (vs2l >> 48) & 0x0000ffff;
					f = (vs2l >> 32) & 0x0000ffff;
					g = (vs2l >> 16) & 0x0000ffff;
					h = (vs2l      ) & 0x0000ffff;
				}
				else
				{
					a = (vs2l      ) & 0x0000ffff;
					b = (vs2l >> 16) & 0x0000ffff;
					c = (vs2l >> 32) & 0x0000ffff;
					d = (vs2l >> 48) & 0x0000ffff;
					e = (vs2h      ) & 0x0000ffff;
					f = (vs2h >> 16) & 0x0000ffff;
					g = (vs2h >> 32) & 0x0000ffff;
					h = (vs2h >> 48) & 0x0000ffff;
				}
				vs2 += 8*a + 7*b + 6*c + 5*d + 4*e + 3*f + 2*g + 1*h;
			} while(k >= SOUL);
			/* reduce vs1 round sum before multiplying by 8 */
			vs1_r = reduce(vs1_r);
			/* add vs1 for this round (8 times) */
			vs2 += vs1_r * 8;
			/* reduce both sums to something within 17 bit */
			vs2 = reduce(vs2);
			vs1 = reduce(vs1);
			len += k;
			k = len < VNMAX ? (unsigned) len : VNMAX;
			len -= k;
		} while(likely(k >= SOUL));
		len += k;
		s1 = vs1;
		s2 = vs2;
	}

	if(unlikely(len)) do {
		s1 += *buf++;
		s2 += s1;
	} while (--len);
	/* at this point we should not have so big s1 & s2 */
	s1 = reduce_x(s1);
	s2 = reduce_x(s2);

	return s2 << 16 | s1;
}
static char const rcsid_a32alpha_max[] GCC_ATTR_USED_VAR = "$Id: $";
#else
/* alpha has a hard time with byte access, exploit 64bit-risc-voodoo */
static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1, s2;
	unsigned k;

	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;

	k    = ALIGN_DIFF(buf, SOUL);
	len -= k;
	if(k) do {
		s1 += *buf++;
		s2 += s1;
	} while(--k);

	k = len > VNMAX ? VNMAX : len;
	len -= k;
	if(k >= 2 * SOUL) do
	{
		uint32_t vs1, vs2;
		uint32_t vs1s;

		s2 += s1 * ROUND_TO(k, SOUL);
		vs1s = vs1 = vs2 = 0;
		do
		{
			unsigned long a, b, c, d, e, f, g, h;
			unsigned long vs1l = 0, vs1h = 0, vs1l_s = 0, vs1h_s = 0;
			unsigned j;

			j = k > 23 * SOUL ? 23 : k/SOUL;
			k -= j * SOUL;
			vs1s += j * vs1;
			do
			{
				unsigned long in8 = *(const unsigned long *)buf;
				buf += SOUL;
				vs1l_s += vs1l;
				vs1h_s += vs1h;
				vs1l +=  in8 & 0x00ff00ff00ff00ffull;
				vs1h += (in8 & 0xff00ff00ff00ff00ull) >> 8;
			} while(--j);

			if(HOST_IS_BIGENDIAN)
			{
				a = (vs1h >> 48) & 0x0000ffff;
				b = (vs1l >> 48) & 0x0000ffff;
				c = (vs1h >> 32) & 0x0000ffff;
				d = (vs1l >> 32) & 0x0000ffff;
				e = (vs1h >> 16) & 0x0000ffff;
				f = (vs1l >> 16) & 0x0000ffff;
				g = (vs1h      ) & 0x0000ffff;
				h = (vs1l      ) & 0x0000ffff;
			}
			else
			{
				a = (vs1l      ) & 0x0000ffff;
				b = (vs1h      ) & 0x0000ffff;
				c = (vs1l >> 16) & 0x0000ffff;
				d = (vs1h >> 16) & 0x0000ffff;
				e = (vs1l >> 32) & 0x0000ffff;
				f = (vs1h >> 32) & 0x0000ffff;
				g = (vs1l >> 48) & 0x0000ffff;
				h = (vs1h >> 48) & 0x0000ffff;
			}

			vs2 += 8*a + 7*b + 6*c + 5*d + 4*e + 3*f + 2*g + 1*h;
			vs1 += a + b + c + d + e + f + g + h;

			vs1l_s = ((vs1l_s      ) & 0x0000ffff0000ffffull) +
			         ((vs1l_s >> 16) & 0x0000ffff0000ffffull);
			vs1h_s = ((vs1h_s      ) & 0x0000ffff0000ffffull) +
			         ((vs1h_s >> 16) & 0x0000ffff0000ffffull);
			vs1l_s += vs1h_s;
			vs1s += ((vs1l_s      ) & 0x00000000ffffffffull) +
			        ((vs1l_s >> 32) & 0x00000000ffffffffull);
		} while(k >= SOUL);
		vs1s = reduce(vs1s);
		s2 = reduce(s2 + vs1s * 8 + vs2);
		s1 = reduce(s1 + vs1);
		len += k;
		k = len > VNMAX ? VNMAX : len;
		len -= k;
	} while(k >= SOUL);

	if(k) do {
		s1 += *buf++;
		s2 += s1;
	} while (--k);
	s1 = reduce_4(s1);
	s2 = reduce_4(s2);

	return s2 << 16 | s1;
}
static char const rcsid_a32alpha[] GCC_ATTR_USED_VAR = "$Id: $";
#endif
/* EOF */
