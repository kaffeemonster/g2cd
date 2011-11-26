/*
 * ansi_prng.c
 * Pseudo random number generator according to ANSI X9.31
 *
 * Copyright (c) 2009-2011 Jan Seiffert
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
#include "my_pthread.h"
#ifndef WIN32
# include <sys/mman.h>
#endif
#include "my_bitopsm.h"
#include "other.h"
#include "aes.h"
#include "ansi_prng.h"

union dvector
{
	size_t s[RAND_BLOCK_BYTE/SOST];
	uint64_t v[RAND_BLOCK_BYTE/sizeof(uint64_t)];
	uint32_t u[RAND_BLOCK_BYTE/SO32];
	unsigned char c[RAND_BLOCK_BYTE];
} GCC_ATTR_ALIGNED(RAND_BLOCK_BYTE);

static struct
{
	union dvector rand_data;
	union dvector DT;
	union dvector I;
	union dvector V;
	struct aes_encrypt_ctx actx;
	pthread_mutex_t lock;
	union dvector ne[4];
	unsigned bytes_used;
} ctx;

/* our memxor is a little over enginered for this... */
static void xor_vectors(const union dvector *in1, const union dvector *in2, union dvector *out)
{
	unsigned i;

	for(i = 0; i < anum(out->s); i++)
		out->s[i] = in1->s[i] ^ in2->s[i];
}

static void increment_counter(union dvector *ctr)
{
#if RAND_BLOCK_BYTE == 16 && defined(I_LIKE_ASM) && defined(__i386__)
	asm(
		"addl	$1,   (%1)\n\t"
		"adcl	$0,  4(%1)\n\t"
		"adcl	$0,  8(%1)\n\t"
		"adcl	$0, 12(%1)\n\t"
		: /* %0 */ "=m" (ctr->u[0])
		: /* %2 */ "r" (ctr->u)
	);
#elif RAND_BLOCK_BYTE == 16 && defined(I_LIKE_ASM) && defined(__x86_64__)
	asm(
		"addq	$1,   (%1)\n\t"
		"adcq	$0,  8(%1)\n\t"
		: /* %0 */ "=m" (ctr->v[0])
		: /* %2 */ "r" (ctr->v)
	);
#else
	unsigned i;
# ifndef HAVE_TIMODE
	uint64_t c = 0;
	c = ctr->u[0] + c + 1;
	ctr->u[0] = c;
	c = c >> 32;
	for(i = 1; i < anum(ctr->u); i++) {
		c = ctr->u[i] + c;
		ctr->u[i] = c;
		c = c >> 32;
	}
# else
	unsigned c __attribute__((mode(TI))) = 0;
	c = ctr->v[0] + c + 1;
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
static noinline void more_random_bytes(void)
{
	union dvector tmp;

	/* encrypt counter, get intermediate I */
	memcpy(&tmp, &ctx.DT, sizeof(tmp));
	aes_ecb_encrypt(&ctx.actx, &ctx.I, &tmp);

	/* xor I with V, encrypt to get output */
	xor_vectors(&ctx.I, &ctx.V, &tmp);
	aes_ecb_encrypt(&ctx.actx, &ctx.rand_data, &tmp);

	/* xor random data with I, encrypt to get new V */
	xor_vectors(&ctx.rand_data, &ctx.I, &tmp);
	aes_ecb_encrypt(&ctx.actx, &ctx.V, &tmp);

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
	increment_counter(&ctx.DT);
	ctx.bytes_used = 0;
}

void noinline random_bytes_get(void *ptr, size_t len)
{
	unsigned char *buf = ptr;

	pthread_mutex_lock(&ctx.lock);

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

	pthread_mutex_unlock(&ctx.lock);
}

void random_bytes_rekey(void)
{
	struct aes_encrypt_ctx tctx;
	unsigned i;
	/* create a new random key */
	aes_encrypt_key128(&tctx, &ctx.ne[0]);
	/* lock other out */
	pthread_mutex_lock(&ctx.lock);
	/* set new key */
	ctx.actx = tctx;
	/* fuzz internal vectors */
	/* set the initial vector state to some random value */
	xor_vectors(&ctx.V, &ctx.ne[1], &ctx.V);
	xor_vectors(&ctx.DT, &ctx.ne[2], &ctx.DT);
	xor_vectors(&ctx.I, &ctx.ne[3], &ctx.I);
	/* get new random data */
	more_random_bytes();
	memcpy(&ctx.ne[0], &ctx.rand_data, RAND_BLOCK_BYTE);
	more_random_bytes();
	memcpy(&ctx.ne[1], &ctx.rand_data, RAND_BLOCK_BYTE);
	more_random_bytes();
	memcpy(&ctx.ne[2], &ctx.rand_data, RAND_BLOCK_BYTE);
	more_random_bytes();
	memcpy(&ctx.ne[3], &ctx.rand_data, RAND_BLOCK_BYTE);
	/*
	 * keep the crng running for ?? times, more iterations make it harder to
	 * get to the initial state, and so to the results of the above calls
	 * and the input for the rekeying in 24 hours.
	 */
	for(i = (ctx.ne[0].c[0] % 32) + 8; i--;)
		more_random_bytes();
	pthread_mutex_unlock(&ctx.lock);
}

static const char *get_text(void)
{
	const char *rval;
	/* get address of some func in text segment */
	asm("" : "=r" (rval) : "0" (more_random_bytes));
	return rval;
}

void __init random_bytes_init(const char data[RAND_BLOCK_BYTE * 2])
{
	union dvector td;
	struct timeval now;
	uint32_t *p, i, t;
	const char *sbox;
	const unsigned magic = 0x5BD1E995;

	/*
	 * <paranoid mode>
	 * This lib func may do who knows what to our mem
	 * (it is "free" to do so, maybe it also needs to
	 * frobnicate with the mapping to get coherent mem
	 * for atomic ops or something like that), in contrast
	 * to the rest of funcs (which are simple or our own).
	 * So to not loose our mlock, do this first.
	 * </paranoid mode>
	 */
	pthread_mutex_init(&ctx.lock, NULL);
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
	t = getpid() | (getppid() << 16);
	ctx.ne[2].u[3] = t;

	gettimeofday(&now, 0);
	/* if gtod fails, we deliberatly pick up stack garbage as fallback */
	memcpy(ctx.ne[2].c, &now.tv_usec, sizeof(now.tv_usec));
	memcpy(ctx.ne[2].c + sizeof(now.tv_usec), &now.tv_sec, sizeof(now.tv_sec));

	/* let the milliseconds decide where we pick the text segment */
	sbox = get_text() + (now.tv_usec / 1000);

	/* fill the I vector */
	memcpy(&ctx.ne[3], sbox - 8192, RAND_BLOCK_BYTE);

	/* mix it baby */
	t = ctx.ne[0].u[0] ^ get_unaligned((const uint32_t *)sbox);
	t ^= ((t >> 13) ^ (t << 7)) * magic;
	for(i = 0, p = &ctx.ne[2].u[0]; i++ < ((RAND_BLOCK_BYTE * 2)/SO32);) {
			uint32_t o_rd = *p;
			*p++ ^= t;
			t ^= ((t >> 13) ^ (t << 7)) * magic;
			t ^= o_rd ^ get_unaligned((const uint32_t *)(sbox + (t & 0xffff)));
			t ^= ((t >> 13) ^ (t << 7)) * magic;
	}

	/* now init the real data */
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
