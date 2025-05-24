/*
 * ansi_prng.c
 * Pseudo random number generator in spirit to ANSI X9.31
 *
 * Copyright (c) 2009-2019 Jan Seiffert
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
 * enlighted by the Linux kernel ansi_cprng.c which is GPL2:
 * (C) Neil Horman <nhorman@tuxdriver.com>
 *
 * $Id: $
 */

/*
 * Update 2017:
 *
 * ! ! ! DO NOT USE THIS CODE FOR ANYTHING ! ! !
 *
 * X9.31 was deprecated by NIST in 2011, is no longer FIPS compliant
 * since 2016.
 *
 * So yes, this may not be the best PNRG, because of a long known
 * (1998) weakness of state recovery which results in prediction of
 * future output (and a long track record of mindbogingly bad
 * implementations (e.g. static seed key, even sanctioned by the
 * standard) and propably even malicius subversion in the whole X9.xx +
 * NIST/FIPS process (Dual EC DRBG anyone?)).
 *
 * But for that several conditions must be met.
 *
 * In the scope of g2cd these conditions are not met:
 * - no clear text direct prng output is leaked to the world
 * - dynamically seeded on statup
 * - constantly reseeded during operation
 *
 * Besides: no "real" cryptography is made with the prng output.
 * Instead it is used for tasks normally much weaker algos are used
 * (see rand(), random(), rand48()).
 *
 * Instead we can reap the percived advantages:
 * - "strong" randomness with a, in this day and age, fast
 *   building block (AES, aes instructions)
 * - the concept is so simple, even i can grasp it
 * - and from what i understand, at least the "time-back"
 *   direction of this prng should be as strong as AES itself
 *   (baring major compromise: seed, internal state)
 *
 * So for the usage warning: Do not copy this code and go like
 * "Yay, i got a X9.31 PNGR, it's cryptographically strong,
 * let's do crypto with it, done, ship it". That's a BAD idea.
 *
 * This code is only a PRNG, not a CPRNG!
 *
 * And yes, if you gain remote access to the memory comprising
 * the internal state, game over. But in that case you are
 * in a world of hurt anyways...
 * And i really want to see a software (C)PRNG which saves
 * your ass in that case...
 */


/*
 * Pseudo random number generator according to ANSI X9.31
 * Appendix A.2.4 Using AES
 * http://csrc.nist.gov/groups/STM/cavp/documents/rng/931rngext.pdf
 *
 * Recrypt something with it self with AES in CTR mode to
 * get pseudo random bytes.
 *
 * Except that we tweak that a little bit....
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <zlib.h>
#include "my_pthread.h"
#ifndef WIN32
# include <sys/mman.h>
#endif
#include "my_bitopsm.h"
#include "other.h"
#include "aes.h"
#include "ansi_prng.h"
#if RAND_BLOCK_BYTE == 16 && defined(I_LIKE_ASM) && defined(__x86_64__)
# include "x86/x86.h"
#endif

union dvector
{
	size_t s[RAND_BLOCK_BYTE/SOST];
	uint64_t v[RAND_BLOCK_BYTE/sizeof(uint64_t)];
	unsigned long l[RAND_BLOCK_BYTE/sizeof(unsigned long)];
	uint32_t u[RAND_BLOCK_BYTE/SO32];
	unsigned char c[RAND_BLOCK_BYTE];
} GCC_ATTR_ALIGNED(16);

static struct rctx
{
	union dvector rand_data;
	union dvector V;
	union dvector DT;
	struct aes_encrypt_ctx actx;
	mutex_t lock;
	uint32_t adler;
	unsigned bytes_used;
	union dvector ne[3];
} ctx GCC_ATTR_ALIGNED(16);

/* our memxor is a little over enginered for this... */
static void xor_vectors(const union dvector *in1, const union dvector *in2, union dvector *out)
{
	unsigned i;

	for(i = 0; i < anum(out->s); i++)
		out->s[i] = in1->s[i] ^ in2->s[i];
}

static uint32_t get_timestamp(void)
{
	/*
	 * We only care for a changing bit pattern, some entropy.
	 * We do NOT care for:
	 * - correct time keeping
	 * - jumps
	 * - DST, leap years and seconds
	 * - limited datatype range (if it overflow in 2038, pfff...)
	 * - frequency stability/PM
	 * - stability across cores
	 * - cycle stealing
	 * - exact cycle mesurement
	 * and what ever jazz you have.
	 *
	 * Linux luckily has a very optimized gettimeofday as a fallback,
	 * but we actually want something simple like RDTSC.
	 * Unfortunatly most RISC cpus make similar funtionality hard
	 * to use, ring0 only and stuff...
	 */
//TODO: since RDTSC can be flacky, check against CPUID
#if defined(I_LIKE_ASM)
#if  defined(__i386__)
	uint32_t a, d;
	asm(
		"rdtsc\n"
		: /*  */ "=a" (a),
		  /*  */ "=d" (d)
	);
	return a;
#elif  defined(__x86_64__)
	uint64_t a, d;
	asm(
		"rdtsc\n"
		: /*  */ "=a" (a),
		  /*  */ "=d" (d)
	);
	return a;
#else
	struct timeval t;
	gettimeofday(&t, NULL);
	return (uint32_t)t.tv_usec;
#endif
#else
	struct timeval t;
	gettimeofday(&t, NULL);
	return (uint32_t)t.tv_usec;
#endif
}

static void increment_counter(union dvector *ctr, uint32_t incr)
{
	uint32_t t;
	/* any increment value is "good", except 0 */
	incr = incr != 0 ? incr : 1;
#if RAND_BLOCK_BYTE == 16 && defined(I_LIKE_ASM) && defined(__i386__)
	asm(
		"addl	%2,    %1\n\t"
		"adcl	$0,  4+%1\n\t"
		"adcl	$0,  8+%1\n\t"
		"adcl	$0, 12+%1\n\t"
		: /* %0 */ "=m" (ctr->u[0])
		: /* %1 */ "m" (*ctr->u),
		  /* %2 */ "r" (incr)
	);
#elif RAND_BLOCK_BYTE == 16 && defined(I_LIKE_ASM) && defined(__x86_64__)
	uint64_t l = ctr->v[0], h = ctr->v[1];
	asm(
		"addq	%q4, %0\n\t"
		"adcq	$0,  %1\n\t"
		: /* %0 */ "=r" (l),
		  /* %1 */ "=r" (h)
		: /* %2 */ "0" (ctr->v[0]),
		  /* %3 */ "1" (ctr->v[1]),
		  /* %4 */ "r" (incr)
	);
	ctr->v[0] = l; ctr->v[1] = h;
#if 0
	asm(
		"addq	%q2,   %1\n\t"
		"adcq	$0,  8+%1\n\t"
		: /* %0 */ "=m" (ctr->v[0])
		: /* %1 */ "m" (*ctr->v),
		  /* %2 */ "r" (incr)
	);
#endif
#else
	unsigned i;
# ifndef HAVE_TIMODE
	uint64_t c = incr;
	for(i = 0; i < anum(ctr->u); i++) {
		c = ctr->u[i] + c;
		ctr->u[i] = c;
		c = c >> 32;
	}
# else
	unsigned c __attribute__((mode(TI))) = incr;
	for(i = 0; i < anum(ctr->v); i++) {
		c += ctr->v[i];
		ctr->v[i] = c;
		c = c >> 64;
	}
# endif
#endif
	/* fancy ROR the counter, smeering the lowest bits into the high bits */
	t = ctr->u[0] ^ (ctr->u[0] << 11);
	ctr->u[0] = ctr->u[1]; ctr->u[1] = ctr->u[2]; ctr->u[2] = ctr->u[3];
	ctr->u[3] ^= (ctr->u[3] >> 19) ^ t ^ (t >> 8);
}

static always_inline void butterfly_one(uint32_t *d, unsigned idx)
{
	unsigned idx_o = (idx + 1) % (RAND_BLOCK_BYTE/SO32);
	uint32_t t = d[idx];
	d[idx] = d[idx_o];
	d[idx_o] = t;
}

static always_inline void butterfly_two(uint32_t *d, unsigned idx)
{
	unsigned idx_o = (idx + 2) % (RAND_BLOCK_BYTE/SO32);
	uint32_t t = d[idx];
	d[idx] = d[idx_o];
	d[idx_o] = t;
}

static void butterfly(union dvector *d, uint32_t transf)
{
	switch(transf % 16)
	{
	case 0:
		break;
	case 1:
		butterfly_one(d->u, 0);
		break;
	case 2:
		butterfly_one(d->u, 1);
		break;
	case 3:
		butterfly_one(d->u, 2);
		break;
	case 4:
		butterfly_one(d->u, 3);
		break;
	case 5:
		butterfly_one(d->u, 0);
		butterfly_one(d->u, 2);
		break;
	case 6:
		butterfly_one(d->u, 1);
		butterfly_one(d->u, 3);
		break;
	case 7:
		butterfly_two(d->u, 0);
		break;
	case 8:
		butterfly_two(d->u, 1);
		break;
	case 9:
		butterfly_two(d->u, 0);
		butterfly_two(d->u, 1);
		break;
	case 10:
		butterfly_two(d->u, 0);
		butterfly_one(d->u, 0);
		break;
	case 11:
		butterfly_two(d->u, 1);
		butterfly_one(d->u, 0);
		break;
	case 12:
		butterfly_two(d->u, 0);
		butterfly_one(d->u, 1);
		break;
	case 13:
		butterfly_two(d->u, 1);
		butterfly_one(d->u, 1);
		break;
	case 14:
		butterfly_two(d->u, 0);
		butterfly_one(d->u, 2);
		break;
	case 15:
		butterfly_two(d->u, 1);
		butterfly_one(d->u, 2);
		break;
	}
}

/* generate more random bytes */
static void more_random_bytes_rctx_int(struct rctx *rctx)
{
	union dvector tmp, I;

	/* encrypt counter, get intermediate I */
	aes_ecb_encrypt256(&rctx->actx, &I, &rctx->DT);

	/* xor I with V, encrypt to get output */
	xor_vectors(&I, &rctx->V, &tmp);
	aes_ecb_encrypt256(&rctx->actx, &rctx->rand_data, &tmp);

	butterfly(&rctx->rand_data, rctx->V.u[0]);
	butterfly(&rctx->DT, rctx->V.u[1]);
	/* xor random data with I, encrypt to get new V */
	xor_vectors(&rctx->rand_data, &I, &tmp);
	aes_ecb_encrypt256(&rctx->actx, &rctx->V, &tmp);

	/*
	 * update counter
	 *
	 * This counter is endian dependent!
	 *
	 * ! ! ! ------------------------------------------------ ! ! !
	 * ! Don't use this to encrypt communication with other hosts !
	 * ! ! ! ------------------------------------------------ ! ! !
	 *
	 * This is no problem here, since CTR-mode here is internal to
	 * our prng and not part of a world facing lib function
	 * for AES-CTR mode.
	 * Its also no problem for the correctness, counter
	 * mode only wants a counter which "advances", it's
	 * not said which bit pattern the counter has to have
	 * (can be seen as a f(x) with a long period).
	 */
	increment_counter(&rctx->DT, rctx->adler ^ get_timestamp());

	/* checksum I */
	rctx->adler = adler32(rctx->adler, I.c, sizeof(I));
}

static noinline void more_random_bytes(void)
{
	more_random_bytes_rctx_int(&ctx);
	ctx.bytes_used = 0;
}

static noinline void more_random_bytes_rctx(struct rctx *rctx)
{
	more_random_bytes_rctx_int(rctx);
}

#if 0
/*
 * xorshift1024* PRNG, quite good and has a 2^1024-1 period length
 * could be used as an inner generator, regulary re-seeded from
 * from the big CPRNG engine
 */
uint64_t s[16] = {
	0xcd03624df3d6f7ef, 0x34b30a483d346d12, 0xa76bfba260ea2923, 0xb2688dadc602732d,
	0x14f112abfae03338, 0x8c0d322317bc6f95, 0x99c6439600bd7977, 0x255e96b46d499571,
	0x44b5b0a63638bc2c, 0xbeff2ef6f4911c7a, 0x588bd86c03216764, 0x7c012d317844dcff,
	0xd2ab8569a350bdc3, 0xfffcd6e0071781eb, 0x12210b17caadfb9e, 0x3320a6007f9d8d3e
};
int p;

uint64_t next(void) {
	uint64_t s0 = s[p];
	uint64_t s1 = s[p = (p + 1) & 15];
	s1 ^= s1 << 31; // a
	s1 ^= s1 >> 11; // b
	s0 ^= s0 >> 30; // c
	return (s[p] = s0 ^ s1) * 1181783497276652981LL;
}

// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;

uint32_t pcg32_random_r(pcg32_random_t* rng)
{
    uint64_t oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}
#endif

void noinline random_bytes_get(void *ptr, size_t len)
{
	unsigned char *buf = ptr;

	mutex_lock(&ctx.lock);

	do
	{
		if(ctx.bytes_used >= RAND_BLOCK_BYTE)
			more_random_bytes();

		if(len < RAND_BLOCK_BYTE) {
			for(; ctx.bytes_used < RAND_BLOCK_BYTE && len; ctx.bytes_used++, len--) {
				*buf++ = ctx.rand_data.c[ctx.bytes_used];
			}
		}

		/* consume whole blocks */
		for(; len >= RAND_BLOCK_BYTE; len -= RAND_BLOCK_BYTE)
		{
			more_random_bytes();
			memcpy(buf, &ctx.rand_data, RAND_BLOCK_BYTE);
			ctx.bytes_used += RAND_BLOCK_BYTE;
			buf += RAND_BLOCK_BYTE;
		}
	} while(len);

	mutex_unlock(&ctx.lock);
}

static const unsigned char *get_text(void)
{
	const unsigned char *rval;
	/* get address of some func in text segment */
	asm("" : "=r" (rval) : "0" (more_random_bytes));
	return rval;
}

void random_bytes_rekey(const char data[RAND_BLOCK_BYTE * 2])
{
	char tmp_store[sizeof(struct rctx) + sizeof(union dvector) + 16];
	struct rctx *rctx;
	union dvector *ni;
	unsigned i;

	rctx = (struct rctx *)ALIGN(tmp_store, 16);
	ni   = rctx->ne;

	memcpy(ni, data, RAND_BLOCK_BYTE * 2);
	/* mix in possible fresh entropy */
	xor_vectors(&ni[0], &ctx.ne[0], &ctx.ne[0]);
	xor_vectors(&ni[1], &ctx.ne[1], &ctx.ne[1]);
	/* seed adler from a little bit sbox data */
	rctx->adler = adler32(1, get_text() - (ctx.ne[2].u[1] % 8192), 4096);
	/* create a new random key */
	aes_encrypt_key256(&rctx->actx, &ctx.ne[0]);

	/* fuzz internal vectors */
	/* set the initial vector state to some random value */
	xor_vectors(&ctx.V, &ctx.ne[1], &rctx->V);
	xor_vectors(&ctx.DT, &ctx.ne[2], &rctx->DT);
	/* get new random data */
	more_random_bytes_rctx(rctx);
	memcpy(&ctx.ne[0], &rctx->rand_data, RAND_BLOCK_BYTE);
	more_random_bytes_rctx(rctx);
	memcpy(&ctx.ne[1], &rctx->rand_data, RAND_BLOCK_BYTE);
	more_random_bytes_rctx(rctx);
	memcpy(&ctx.ne[2], &rctx->rand_data, RAND_BLOCK_BYTE);
	/*
	 * keep the crypto powered prng running for ?? times, more
	 * iterations make it harder to get to the initial state, and
	 * so to the results of the above calls and the input for the
	 * rekeying in 24 hours.
	 */
	for(i = (ctx.ne[0].c[ctx.ne[0].c[1] % 16] % 32) + 8; i--;)
		more_random_bytes_rctx(rctx);

	/*
	 * Again recreate the key, so if the key is reconstructed
	 * from the random output, the random data entropy
	 * for the next rekeying is not simply guessable
	 */
	memcpy(&ni[0], rctx->rand_data.c, sizeof(ni[0]));
	more_random_bytes_rctx(rctx);
	memcpy(&ni[1], rctx->rand_data.c, sizeof(ni[1]));
	aes_encrypt_key256(&rctx->actx, ni);

	memset(&ni[0], 0, sizeof(*ni) * 4);

	/* move state forward */
	for(i = (ctx.ne[1].c[ctx.ne[0].c[2] % 16] % 32) + 8; i--;) {
		more_random_bytes_rctx(rctx);
		xor_vectors(&ni[i % 4], &rctx->rand_data, &ni[i % 4]);
	}

	/* again scramble the state */
	rctx->adler = adler32(rctx->adler, ni[0].c, sizeof(*ni) * 4);
	xor_vectors(&rctx->V, &ni[1], &rctx->V);
	xor_vectors(&rctx->DT, &ni[2], &rctx->DT);

	/* move state forward */
	for(i = (ctx.ne[2].c[ctx.ne[0].c[3] % 16] % 32) + 8; i--;)
		more_random_bytes_rctx(rctx);

/* // no, do not fold new rand data back into entropy for next key
	xor_vetors(&ni[0], &ctx.ne[0], &ctx.ne[0]);
	xor_vetors(&ni[1], &ctx.ne[1], &ctx.ne[1]);
	xor_vetors(&ni[2], &ctx.ne[2], &ctx.ne[2]);
	*/

	/* lock other out */
	mutex_lock(&ctx.lock);
	/* set the new key */
	memcpy(&ctx.rand_data, &rctx->rand_data, offsetof(struct rctx, lock));
	ctx.adler = rctx->adler;
	ctx.bytes_used = RAND_BLOCK_BYTE;
	mutex_unlock(&ctx.lock);
}

void __init random_bytes_init(const char data[RAND_BLOCK_BYTE * 2])
{
	union dvector td, st[2];
	struct timeval now;
	uint32_t *p, i, t;
	const unsigned char *sbox;
	unsigned char *wptr;
	const unsigned magic = 0x5BD1E995;

	/*
	 * <paranoid mode>
	 * mutex_init first!
	 *
	 * This lib func may do who knows what to our mem
	 * (it is "free" to do so, maybe it needs to
	 * frobnicate with the mapping to get coherent mem
	 * for atomic ops or something like that), in contrast
	 * to the rest of funcs (which are simple or our own).
	 * So to not loose our mlock, do this first.
	 * </paranoid mode>
	 */
	mutex_init(&ctx.lock);

#ifdef _POSIX_MEMLOCK_RANGE
	/* Now try to lock our ctx into mem for fun and profit */
	{
		void *addr;
		size_t len = sysconf(_SC_PAGESIZE);

		len  = (size_t)-1 == len ? 4096 : len;
		addr = (void *)ALIGN_DOWN(&ctx, len);
		len *= (size_t)ALIGN_DIFF(&ctx, len) < sizeof(ctx) ? 2 : 1;
		if(-1 == mlock(addr, len)) {
			/* we are only a prng, if it fails, the world will continue to turn... */ ;
		}
	}
#endif
	ctx.bytes_used = RAND_BLOCK_BYTE;
	memcpy(&ctx.ne[0], data, RAND_BLOCK_BYTE * 2);

	/*
	 * set the counter to some random start value
	 * as long as we do have a random key and vector,
	 * this is not that important
	 */
	/* bit reverse the input vector to the other fields */
	for(i = 0, wptr = &st[1].c[RAND_BLOCK_BYTE-1]; i < RAND_BLOCK_BYTE * 2; i++)
	{
		unsigned char a = data[i];
		a = ((a & 0x55u) << 1) | ((a & 0xAAu) >> 1);
		a = ((a & 0x33u) << 2) | ((a & 0xCCu) >> 2);
		a = ((a & 0x0Fu) << 4) | ((a & 0xF0u) >> 4);
		*wptr-- = a;
	}

	/* get some more foo */
	t = getpid() | (getppid() << 16);
	st[1].u[3] ^= t;

	gettimeofday(&now, 0);
	/* if gtod fails, we deliberatly pick up stack garbage as fallback */
	st[0].l[0] ^= now.tv_sec;
	st[0].l[1] ^= now.tv_usec;

	/* let the milliseconds decide where we pick the text segment */
	sbox = get_text() + (now.tv_usec / 1000) - data[0];

	/* fill the I vector */
	memcpy(&td, sbox - 8192, sizeof(td));
	xor_vectors(&st[1], &td, &st[1]);

	/* seed adler from a little bit sbox data */
	ctx.adler = adler32(1, sbox - (st[1].u[1] % 8192) , 4096);

	/* mix it baby */
	t = st[1].u[0] ^ get_unaligned((const uint32_t *)sbox) ^ ctx.adler;
	t ^= ((t >> 13) ^ (t << 7)) * magic;
	for(i = 0, p = &st[0].u[0]; i++ < ((RAND_BLOCK_BYTE * 2)/SO32);) {
			uint32_t o_rd = *p;
			*p++ ^= t;
			t ^= ((t >> 13) ^ (t << 7)) * magic;
			t ^= o_rd ^ get_unaligned((const uint32_t *)(sbox + (t & 0xffff)));
			t ^= ((t >> 13) ^ (t << 7)) * magic;
	}
	memcpy(&ctx.ne[2], st, sizeof(ctx.ne[2]));

	/* mix in all rand data */
	ctx.adler = adler32(ctx.adler, ctx.ne[0].c, sizeof(ctx.ne[0])*2);
	ctx.adler = adler32(ctx.adler, st[0].c, sizeof(st));

	/* clear space for additional entropy, we don't have any */
	memset(&st[0], 0, sizeof(st));

	sbox -= 16384;
	/* now init the real data */
	random_bytes_rekey((const char *)sbox);

	/* for the first rekeying, make sure its all mixed well */
	for(i = (ctx.rand_data.c[0] % 4)+1; i--;)
		random_bytes_rekey((const char *)sbox + ((i+1)*RAND_BLOCK_BYTE*2));

	/* initialise clib rands */
	random_bytes_get(&td, sizeof(td));
	srand(td.s[0]);
	random_bytes_get(&td, sizeof(td));
	srandom(td.s[1]);
}

/*@unused@*/
static char const rcsid_aprng[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
