/*
 * ansi_prng.c
 * Pseudo random number generator according to ANSI X9.31
 *
 * Copyright (c) 2009 Jan Seiffert
 *
 * enlighted by the Linux kernel ansi_cprng.c which is GPL2:
 * (C) Neil Horman <nhorman@tuxdriver.com>
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

void random_bytes_init(const char data[RAND_BLOCK_BYTE * 2])
{
	union dvector td;
	struct timeval now;
	uint32_t t;

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

	/* create a random key */
	aes_encrypt_key128(&ctx.actx, data);

	/* set the initial vector state to some random value */
	memcpy(&ctx.V, data + RAND_BLOCK_BYTE, RAND_BLOCK_BYTE);

	/*
	 * set the counter to some random start value
	 * as long as we do have a random key and vector,
	 * this is not that important
	 */
	t = getpid() | (getppid() << 16);
	ctx.DT.u[3] = t;
/*	old way to create initial vector
	unsigned i;
	for(i = 0; i < anum(ctx.V.u); i++) {
		ctx.V.u[i] = t;
		t = ((t >> 13) ^ (t << 7)) + 65521;
	}*/

	gettimeofday(&now, 0);
	/* if gtod fails, we deliberatly pick up stack garbage as fallback */
	memcpy(ctx.DT.c, &now.tv_usec, sizeof(now.tv_usec));
	memcpy(ctx.DT.c + sizeof(now.tv_usec), &now.tv_sec, sizeof(now.tv_sec));

	/* initialise clib rands */
	random_bytes_get(&td, sizeof(td));
	srand(td.s[0]);
	random_bytes_get(&td, sizeof(td));
	srandom(td.s[1]);
}

/*@unused@*/
static char const rcsid_aprng[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
