/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   sparc/sparc64 implementation
 * Copyright (C) 1995-2007 Mark Adler
 * Copyright (C) 2009-2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/*
 * This is not the original adler32.c from the zlib distribution,
 * but a heavily modified version. If you are looking for the
 * original, please go to zlib.net.
 */

#if defined(HAVE_VIS) && (defined(HAVE_REAL_V9) || defined(__sparcv9) || defined(__sparc_v9__))
# define HAVE_ADLER32_VEC
/*
 * SPARC CPUs gained a SIMD extention with the UltraSPARC I, called
 * VIS, implemented in their FPU. We can build adler32 with it.
 *
 * But Note:
 * - Do not use it with Niagara or other CPUs like it. They have a
 *   shared FPU (T1: 1 FPU for all up to 8 cores, T2: 1 FPU for 8 threads)
 *   and to make matters worse, they only implement a hand full of VIS
 *   instructions, the rest is missing.
 *   Linux emulates the missing instructions, but besides beeing slow
 *   (going over an exception here for every insn), the emulation is broken
 *   on older Kernel versions (binary which works on other SPARC creates wrong
 *   result on T1).
 * - There is no clear preprocesor define which tells us if we have VIS.
 *   Often the tool chain even disguises a sparcv9 as a sparcv8
 *   (pre-UltraSPARC) to not confuse the userspace.
 * - The code has a high startup cost
 * - The code only handles big endian
 *
 * For these reasons this code is not automatically enabled and you have
 * to define HAVE_VIS as a switch to enable it. We can not easily provide a
 * dynamic runtime switch. The CPU has make and model encoded in the Processor
 * Status Word (PSW), but reading the PSW is a privilidged instruction (same
 * as PowerPC...).
 */
static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len);
# define MIN_WORK 512
#endif

#include "../generic/adler32.c"

#if defined(HAVE_VIS) && (defined(HAVE_REAL_V9) || defined(__sparcv9) || defined(__sparc_v9__))
# define VNMAX ((5*NMAX)/2)
# include "sparc_vis.h"

/* ========================================================================= */
static inline unsigned long long vector_chop(unsigned long long x)
{
	unsigned long long mask = 0x0000ffff0000ffffull;
	unsigned long long y = fpand(x, mask);
	x = fpandnot2(x, mask);
# if 0 && defined(HAVE_VIS2)
	write_bmask1(0x89abcdef);
	printf("\n%016llx\n", x);
	x = bshuffle(x, 0x123456789abcdef1ull);
	printf("%016llx\n", x);
# else
	x >>= 16;
# endif
	y = fpsub32(y, x);
	x = fpadd32(x, x); /* << 1 */
	x = fpadd32(x, x); /* << 1 */
	x = fpadd32(x, x); /* << 1 */
	x = fpadd32(x, x); /* << 1 */
	return fpadd32(x, y);
}

/* ========================================================================= */
static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1, s2;

	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;

	if(likely(len >= 2 * SOVV))
	{
		unsigned long long vorder_lo = 0x0004000300020001ULL;
		unsigned long long vorder_hi = 0x0008000700060005ULL;
		unsigned long long vs1, vs2;
		unsigned long long v0 = fzero();
		unsigned long long in;
		const unsigned char *o_buf;
		unsigned int k, f, n;

		/* align hard down */
		f = (unsigned int)ALIGN_DOWN_DIFF(buf, SOVV);
		n = SOVV - f;

		/* add n times s1 to s2 for start round */
		s2 += s1 * n;

		/* insert scalar start somehwere */
		vs1 = s1;
		vs2 = s2;

		k = len < VNMAX ? len : VNMAX;
		len -= k;

		/* get input data */
		o_buf = buf;
		buf = alignaddr(buf, 0);
		in = *(const unsigned long long *)buf;
		{
			unsigned long long vs2t;
			/* swizzle data in place */
			in = faligndata(in, v0);
			if(f) {
				alignaddrl(o_buf, 0);
				in = faligndata(v0, in);
			}
			/* add horizontal and acc */
			vs1 = pdist(in, v0, vs1);
			/* extract, apply order and acc */
			vs2t = pextlbh(in);
			vs2 = fpadd32(vs2, fpmul16x16x32_lo(vs2t, vorder_lo));
			vs2 = fpadd32(vs2, fpmul16x16x32_hi(vs2t, vorder_lo));
			vs2t = pexthbh(in);
			vs2 = fpadd32(vs2, fpmul16x16x32_lo(vs2t, vorder_hi));
			vs2 = fpadd32(vs2, fpmul16x16x32_hi(vs2t, vorder_hi));
		}
		buf += SOVV;
		k   -= n;

		if(likely(k >= SOVV)) do
		{
			unsigned long long vs1_r = fzero();
			do
			{
				unsigned long long vs2l = fzero(), vs2h = fzero();
				unsigned j;

				j = (k/SOVV) > 127 ? 127 : k/SOVV;
				k -= j * SOVV;
				do
				{
					/* get input data */
					in = *(const unsigned long long *)buf;
					buf += SOVV;
					/* add vs1 for this round */
					vs1_r = fpadd32(vs1_r, vs1);
					/* add horizontal and acc */
					vs1 = pdist(in, v0, vs1);
					/* extract */
					vs2l = fpadd16(vs2l, pextlbh(in));
					vs2h = fpadd16(vs2h, pexthbh(in));
				} while(--j);
				vs2 = fpadd32(vs2, fpmul16x16x32_lo(vs2l, vorder_lo));
				vs2 = fpadd32(vs2, fpmul16x16x32_hi(vs2l, vorder_lo));
				vs2 = fpadd32(vs2, fpmul16x16x32_lo(vs2h, vorder_hi));
				vs2 = fpadd32(vs2, fpmul16x16x32_hi(vs2h, vorder_hi));
			} while(k >= SOVV);
			/* chop vs1 round sum before multiplying by 8 */
			vs1_r = vector_chop(vs1_r);
			/* add vs1 for this round (8 times) */
			vs1_r = fpadd32(vs1_r, vs1_r); /* *2 */
			vs1_r = fpadd32(vs1_r, vs1_r); /* *4 */
			vs1_r = fpadd32(vs1_r, vs1_r); /* *8 */
			vs2   = fpadd32(vs2, vs1_r);
			/* chop both sums */
			vs2 = vector_chop(vs2);
			vs1 = vector_chop(vs1);
			len += k;
			k = len < VNMAX ? len : VNMAX;
			len -= k;
		} while(likely(k >= SOVV));

		if(likely(k))
		{
			/* handle trailer */
			unsigned long long t, r, vs2t;
			unsigned int k_m;

			/* get input data */
			in = *(const unsigned long long *)buf;

			/* swizzle data in place */
			alignaddr(buf, k);
			in = faligndata(v0, in);

			/* add k times vs1 for this trailer */
			/* since VIS muls are painfull, use the russian peasant method, k is small */
			t = 0;
			r = vs1;
			k_m = k;
			do {
				if(k_m & 1)
					t = fpadd32(t, r); /* add to result if odd */
				r = fpadd32(r, r); /* *2 */
				k_m >>= 1; /* /2 */
			} while(k_m);
			vs2 = fpadd32(vs2, t);

			/* add horizontal and acc */
			vs1 = pdist(in, v0, vs1);
			/* extract, apply order and acc */
			vs2t = pextlbh(in);
			vs2  = fpadd32(vs2, fpmul16x16x32_lo(vs2t, vorder_lo));
			vs2  = fpadd32(vs2, fpmul16x16x32_hi(vs2t, vorder_lo));
			vs2t = pexthbh(in);
			vs2  = fpadd32(vs2, fpmul16x16x32_lo(vs2t, vorder_hi));
			vs2  = fpadd32(vs2, fpmul16x16x32_hi(vs2t, vorder_hi));

			buf += k;
			k   -= k;
		}
		/* vs1 is one giant 64 bit sum, but we only calc to 32 bit */
		s1 = vs1;
		/* add both vs2 sums */
		s2 = (vs2 & 0xFFFFFFFFUL) + (vs2 >> 32);
	}

	if(unlikely(len)) do {
		s1 += *buf++;
		s2 += s1;
	} while(--len);
	/* at this point we should not have so big s1 & s2 */
	MOD28(s1);
	MOD28(s2);
	/* return recombined sums */
	return (s2 << 16) | s1;
}

static char const rcsid_a32sparc[] GCC_ATTR_USED_VAR = "$Id: $";
#endif
/* EOF */
