/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   ppc implementation
 * Copyright (C) 1995-2004 Mark Adler
 * Copyright (C) 2009-2010 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/*
 * This is not the original adler32.c from the zlib distribution,
 * but a heavily modified version. If you are looking for the
 * original, please go to zlib.net.
 */
#if defined(__ALTIVEC__) && defined(__GNUC__)
# define HAVE_ADLER32_VEC
static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len);
# define MIN_WORK 16
#endif

#include "../generic/adler32.c"

#if defined(__ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <altivec.h>
# include "ppc_altivec.h"

/* can be propably more, since we do not have the x86 psadbw 64 bit sum */
#define VNMAX (6*NMAX)

static inline vector unsigned int vector_reduce(vector unsigned int x)
{
	vector unsigned int y;
	vector unsigned int vsh;

	vsh = vec_splat_u32(1);
	vsh = vec_sl(vsh, vec_splat_u32(4));

	y = vec_sl(x, vsh);
	y = vec_sr(y, vsh);
	x = vec_sr(x, vsh);
	y = vec_sub(y, x);
	x = vec_sl(x, vec_splat_u32(4));
	x = vec_add(x, y);
	return x;
}

static noinline uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1, s2;

	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;

	if(likely(len >= 2*SOVUC))
	{
		vector unsigned int v0_32 = vec_splat_u32(0);
		vector unsigned int   vsh = vec_splat_u32(4);
		vector unsigned char   v1 = vec_splat_u8(1);
		vector unsigned char vord = vec_ident_rev() + v1;
		vector unsigned char   v0 = vec_splat_u8(0);
		vector unsigned int vs1, vs2;
		vector unsigned char in16, vord_a, v1_a, vperm;
		unsigned f, n;
		unsigned k;

		/*
		 * Add stuff to achieve alignment
		 */
		/* swizzle masks in place */
		vperm  = vec_lvsl(0, buf);
		vord_a = vec_perm(vord, v0, vperm);
		v1_a   = vec_perm(v1, v0, vperm);
		vperm  = vec_lvsr(0, buf);
		vord_a = vec_perm(v0, vord_a, vperm);
		v1_a   = vec_perm(v0, v1_a, vperm);

		/* align hard down */
		f = (unsigned) ALIGN_DOWN_DIFF(buf, SOVUC);
		n = SOVUC - f;
		buf = (const unsigned char *)ALIGN_DOWN(buf, SOVUC);

		/* add n times s1 to s2 for start round */
		s2 += s1 * n;

		/* set sums 0 */
		vs1 = v0_32;
		vs2 = v0_32;

		k = len < VNMAX ? (unsigned)len : VNMAX;
		len -= k;

		/* insert scalar start somewhere */
		vs1 = vec_lde(0, &s1);
		vs2 = vec_lde(0, &s2);

		/* get input data */
		in16 = vec_ldl(0, buf);

		/* mask out excess data, add 4 byte horizontal and add to old dword */
		vs1 = vec_msum(in16, v1_a, vs1);

		/* apply order, masking out excess data, add 4 byte horizontal and add to old dword */
		vs2 = vec_msum(in16, vord_a, vs2);

		buf += SOVUC;
		k -= n;

		if(likely(k >= SOVUC)) do
		{
			vector unsigned int vs1_r = v0_32;
			do
			{
				/* add vs1 for this round */
				vs1_r += vs1;

				/* get input data */
				in16 = vec_ldl(0, buf);

				/* add 4 byte horizontal and add to old dword */
				vs1 = vec_sum4s(in16, vs1);
				/* apply order, add 4 byte horizontal and add to old dword */
				vs2 = vec_msum(in16, vord, vs2);

				buf += SOVUC;
				k -= SOVUC;
			} while (k >= SOVUC);
			/* reduce vs1 round sum before multiplying by 16 */
			vs1_r = vector_reduce(vs1_r);
			/* add all vs1 for 16 times */
			vs2 += vec_sl(vs1_r, vsh);
			/* reduce the vectors to something in the range of BASE */
			vs2 = vector_reduce(vs2);
			vs1 = vector_reduce(vs1);
			len += k;
			k = len < VNMAX ? (unsigned)len : VNMAX;
			len -= k;
		} while(likely(k >= SOVUC));

		if(likely(k))
		{
			vector unsigned int vk;
			/*
			 * handle trailer
			 */
			f = SOVUC - k;
			/* swizzle masks in place */
			vperm  = vec_identl(f);
			vord_a = vec_perm(vord, v0, vperm);
			v1_a   = vec_perm(v1, v0, vperm);

			/* add k times vs1 for this trailer */
			vk = (vector unsigned int)vec_lvsl(0, (unsigned *)(uintptr_t)k);
			vk = (vector unsigned)vec_mergeh(v0, (vector unsigned char)vk);
			vk = (vector unsigned)vec_mergeh((vector unsigned short)v0, (vector unsigned short)vk);
			vk = vec_splat(vk, 0);
			vs2 += vec_mullw(vs1, vk);

			/* get input data */
			in16 = vec_ldl(0, buf);

			/* mask out excess data, add 4 byte horizontal and add to old dword */
			vs1 = vec_msum(in16, v1_a, vs1);
			/* apply order, masking out excess data, add 4 byte horizontal and add to old dword */
			vs2 = vec_msum(in16, vord_a, vs2);

			buf += k;
			k -= k;
		}

#if 0
		/*
		 * OBSOLETE:
		 * We know have a "cheap" reduce to get the vectors in a range
		 * around BASE, they still need the final proper mod, but we can
		 * stay on the vector unit and do not overflow
		 *
		 * we could stay more than NMAX in the loops above and
		 * fill the first uint32_t near overflow.
		 * Then we have to make the modulus on all vector elements.
		 * there is only one problem:
		 * No divide.
		 *
		 * This is not really a problem, there is the trick to
		 * multiply by the "magic reciprocal". GCC does the same even
		 * in scalar code to prevent a divide (modern CPUs:
		 * mul 8 cycles, div still 44 cycles).
		 *
		 * But there we bump into the next problem:
		 * no 32 Bit mul and esp. no 64 Bit results...
		 *
		 * We can also work around this, but it is expensive.
		 * So to be a win the input array better is really big,
		 * that transfers over the stack hurt more than all this:
		 */
		v15_32 = vec_splat_u32(15);
		vbase_recp = 4 times 0x080078071; /* a mem load hides here */
		vbase  = 4 times 0xFFF1; /* and propably here */
		/* divde vectors by BASE */
		/* multiply vectors by BASE reciprocal, get high 32 bit */
		vres1 = vec_mullwh(vs1, vbase_recp); /* 9 operations */
		vres2 = vec_mullwh(vs2, vbase_recp);
		/* adjust */
		vres1 = vec_sr(vres1, v15_32);
		vres2 = vec_sr(vres2, v15_32);
		/* multiply result again by BASE */
		vres1 = vec_mullw(vbase, vres1); /* 7 oprations */
		vres2 = vec_mullw(vbase, vres2);
		/* substract result */
		vs1 -= vres1;
		vs2 -= vres2;
		/*
		 * Nearly 19 vector ops per vector ~ 40 in total
		 *
		 * Hint: after building the horizontal sum, you have to
		 * do the mod dance again, then in scalar code.
		 *
		 * on x86, which is similar challanged (has 64 bit mul,
		 * but you need to unpack, load constants, reshuffle)
		 * but has faster simd <-> cpu transport, it's only a
		 * whiff of a win, and a loss on medium lengths.
		 *
		 * Just noted down it doesn't get lost.
		 */
#endif

		/* add horizontal */
// TODO: uff, shit, does saturation and signed harm here?
		vs1 = (vector unsigned)vec_sums((vector int)vs1, (vector int)v0_32);
		vs2 = (vector unsigned)vec_sums((vector int)vs2, (vector int)v0_32);
		/* shake and roll */
		vs1 = vec_splat(vs1, 3);
		vs2 = vec_splat(vs2, 3);
		vec_ste(vs1, 0, &s1);
		vec_ste(vs2, 0, &s2);
	}

	if(unlikely(len)) do {
		s1 += *buf++;
		s2 += s1;
	} while (--len);
	MOD(s1);
	MOD(s2);

	return s2 << 16 | s1;
}

static char const rcsid_a32ppc[] GCC_ATTR_USED_VAR = "$Id: $";
#endif
/* EOF */
