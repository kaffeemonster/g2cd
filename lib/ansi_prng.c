/*
 * ansi_prng.c
 * Pseudo random number generator according to ANSI X9.31
 *
 * Copyright (c) 2009-2012 Jan Seiffert
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
 * Pseudo random number generator according to ANSI X9.31
 * Appendix A.2.4 Using AES
 * http://csrc.nist.gov/groups/STM/cavp/documents/rng/931rngext.pdf
 *
 * Recrypt something with it self with AES in CTR mode to
 * get pseudo random bytes.
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

static void increment_counter(union dvector *ctr, uint32_t incr)
{
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
	asm(
		"addq	%q2,   %1\n\t"
		"adcq	$0,  8+%1\n\t"
		: /* %0 */ "=m" (ctr->v[0])
		: /* %1 */ "m" (*ctr->v),
		  /* %2 */ "r" (incr)
	);
#else
	unsigned i;
# ifndef HAVE_TIMODE
	uint64_t c = incr;
	c = ctr->u[0] + c;
	ctr->u[0] = c;
	c = c >> 32;
	for(i = 1; i < anum(ctr->u); i++) {
		c = ctr->u[i] + c;
		ctr->u[i] = c;
		c = c >> 32;
	}
# else
	unsigned c __attribute__((mode(TI))) = incr;
	c = ctr->v[0] + c;
	ctr->v[0] = c;
	c = c >> 64;
	for(i = 1; i < anum(ctr->v); i++) {
		c += ctr->v[i];
		ctr->v[i] = c;
		c = c >> 64;
	}
# endif
#endif
}

/* generate more random bytes */
static void more_random_bytes_rctx_int(struct rctx *rctx)
{
	union dvector tmp, I;

	/* encrypt counter, get intermediate I */
	aes_ecb_encrypt(&rctx->actx, &I, &rctx->DT);

	/* xor I with V, encrypt to get output */
	xor_vectors(&I, &rctx->V, &tmp);
	aes_ecb_encrypt(&rctx->actx, &rctx->rand_data, &tmp);

	/* xor random data with I, encrypt to get new V */
	xor_vectors(&rctx->rand_data, &I, &tmp);
	aes_ecb_encrypt(&rctx->actx, &rctx->V, &tmp);

	/*
	 * update counter
	 *
	 * This counter is endian dependent!
	 *
	 * ! ! ! ------------------------------------------------ ! ! !
	 * ! Don't use this to encrypt communication with other hosts !
	 * ! ! ! ------------------------------------------------ ! ! !
	 *
	 * This is no problem here, since it is internal to
	 * our prng and not part of a world facing lib function
	 * for AES-CTR mode.
	 * Its also no problem for the correctness, counter
	 * mode only wants a counter which "advances", it's
	 * not said which bit pattern the counter has to have
	 * (can be seen as a f(x) with a long period).
	 */
	increment_counter(&rctx->DT, rctx->adler);

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

void random_bytes_rekey(void)
{
	char tmp_store[sizeof(struct rctx) + sizeof(union dvector) + 16];
	struct rctx *rctx;
	union dvector *ni;
	unsigned i;

	rctx = (struct rctx *)ALIGN(tmp_store, 16);
	ni   = rctx->ne;

	/* seed adler from a little bit sbox data */
	rctx->adler = adler32(1, get_text() - (ctx.ne[2].u[1] % 8192), 4096);
	/* create a new random key */
	aes_encrypt_key128(&rctx->actx, &ctx.ne[0]);

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
	 * keep the crng running for ?? times, more iterations make it harder to
	 * get to the initial state, and so to the results of the above calls
	 * and the input for the rekeying in 24 hours.
	 */
	for(i = (ctx.ne[0].c[ctx.ne[0].c[1] % 16] % 32) + 8; i--;)
		more_random_bytes_rctx(rctx);

	/*
	 * Again recreate the key, so if the key is reconstructed
	 * from the random output, the random data entropy
	 * for the next rekeying is not simply guessable
	 */
	aes_encrypt_key128(&rctx->actx, &rctx->rand_data);

	memset(&ni[0], 0, sizeof(*ni) * 4);

	/* move state forward */
	for(i = (ctx.ne[1].c[ctx.ne[0].c[2] % 16] % 32) + 8; i--;) {
		more_random_bytes_rctx(rctx);
		xor_vectors(&ni[i % 4], &rctx->rand_data, &ni[i % 4]);
	}

	/* again scramble the state */
	ctx.adler = adler32(rctx->adler, ni[0].c, sizeof(*ni) * 4);
	xor_vectors(&rctx->V, &ni[1], &rctx->V);
	xor_vectors(&rctx->DT, &ni[2], &rctx->DT);

	/* move state forward */
	for(i = (ctx.ne[2].c[ctx.ne[0].c[3] % 16] % 32) + 8; i--;)
		more_random_bytes_rctx(rctx);

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
	sbox = get_text() + (now.tv_usec / 1000);

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

	/* now init the real data */
	random_bytes_rekey();

	/* for the first rekeying, make sure its all mixed well */
	for(i = (ctx.rand_data.c[0] % 4)+1; i--;)
		random_bytes_rekey();

	/* initialise clib rands */
	random_bytes_get(&td, sizeof(td));
	srand(td.s[0]);
	random_bytes_get(&td, sizeof(td));
	srandom(td.s[1]);
}

/*@unused@*/
static char const rcsid_aprng[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
