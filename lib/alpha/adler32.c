/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   alpha implementation
 * Copyright (C) 1995-2007 Mark Adler
 * Copyright (C) 2010-2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/*
 * This is not the original adler32.c from the zlib distribution,
 * but a heavily modified version. If you are looking for the
 * original, please go to zlib.net.
 */

#define NO_DIVIDE

#if defined(__GNUC__) &&  defined(__alpha_max__)
# define HAVE_ADLER32_VEC
static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len);
# define MIN_WORK 32
#endif

#include "../generic/adler32.c"

#if defined(__GNUC__) && defined(__alpha_max__)
# include "alpha.h"
# define VNMAX (2*NMAX+((9*NMAX)/10))

/* ========================================================================= */
static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1, s2;
	unsigned k;

	/* split Adler-32 into component sums */
	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;

	/* align input */
	k    = ALIGN_DIFF(buf, SOUL);
	len -= k;
	if(k) do {
		s1 += *buf++;
		s2 += s1;
	} while(--k);

	k = len < VNMAX ? len : VNMAX;
	len -= k;
	if(likely(k >= 2 * SOUL))
	{
		unsigned long vs1 = s1, vs2 = s2;

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
				/* split vs2 */
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
				/* mull&add vs2 horiz. */
				vs2 += 8*a + 7*b + 6*c + 5*d + 4*e + 3*f + 2*g + 1*h;
			} while(k >= SOUL);
			/* chop vs1 round sum before multiplying by 8 */
			CHOP(vs1_r);
			/* add vs1 for this round (8 times) */
			vs2 += vs1_r * 8;
			/* CHOP both sums */
			CHOP(vs2);
			CHOP(vs1);
			len += k;
			k = len < VNMAX ? len : VNMAX;
			len -= k;
		} while(likely(k >= SOUL));
		s1 = vs1;
		s2 = vs2;
	}

	/* handle trailer */
	if(unlikely(k)) do {
		s1 += *buf++;
		s2 += s1;
	} while(--k);
	/* at this point we should not have so big s1 & s2 */
	MOD28(s1);
	MOD28(s2);

	/* return recombined sums */
	return (s2 << 16) | s1;
}
static char const rcsid_a32alpha_max[] GCC_ATTR_USED_VAR = "$Id: $";
#endif
/* EOF */
