/*
 * guid.c
 * little stuff to generate a guid
 *
 * Copyright (c) 2010-2012 Jan Seiffert
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
 * $Id: $
 */

#include "my_pthread.h"
#include "other.h"
#include "guid.h"
#include "aes.h"
#include "ansi_prng.h"
#include "hthash.h"

static struct aes_encrypt_ctx ae_ctx;
static mutex_t ctx_lock;
static union guid_fast l_res;

void guid_generate(unsigned char out[GUID_SIZE])
{
	/* get random bytes for our guid */
	random_bytes_get(out, GUID_SIZE);

	mutex_lock(&ctx_lock);
	/* encrypt IV with our random key */
	aes_ecb_encrypt256(&ae_ctx, &l_res, &l_res);
	/* xor bytes with result, creating next IV */
	l_res.d[0] ^= get_unaligned(((uint32_t *)out)+0);
	l_res.d[1] ^= get_unaligned(((uint32_t *)out)+1);
	l_res.d[2] ^= get_unaligned(((uint32_t *)out)+2);
	l_res.d[3] ^= get_unaligned(((uint32_t *)out)+3);
	/* create output */
	memcpy(out, &l_res, GUID_SIZE);
	mutex_unlock(&ctx_lock);

	/*
	 * fix up the bytes to be a valid guid
	 *
	 * guids have some tiny rules for their format and
	 * guids (opposing to uuids?) are endian dependent.
	 * Prop. MS f'up, let's make them always little endian.
	 * Maybe we have to change that, we set the endianess
	 * on packets containing GUIDs...
// TODO: endianess of GUIDs ??
	 *
	 * The first 2 byte of the last 8 byte are big endian
	 * and start with either 8, 9, a, b for random number
	 * guid like ours.
	 */
	out[8] = (out[8] & 0x3f) | 0x80;

	/* now set the version to 4 - random guid */
#if 0
	if(HOST_IS_BIGENDIAN)
		out[6] = (out[6] & 0x0f) | 0x40;
	else
#endif
		out[7] = (out[7] & 0x0f) | 0x40;
}

void guid_tick(void)
{
	unsigned char key[RAND_BLOCK_BYTE*2 + sizeof(l_res)];
	struct aes_encrypt_ctx t_ctx;

	/* get random bytes for a key */
	random_bytes_get(&key, sizeof(key));
	/* create the new key */
	aes_encrypt_key256(&t_ctx, key);
	mutex_lock(&ctx_lock);
	/* put key in place */
	ae_ctx = t_ctx;
	/* and a fresh IV */
	memcpy(&l_res, key + RAND_BLOCK_BYTE, sizeof(l_res));
	mutex_unlock(&ctx_lock);
}

void guid_init(void)
{
	mutex_init(&ctx_lock);
	guid_tick();
}

uint32_t guid_hash(const union guid_fast *g, uint32_t seed)
{
	return hthash_4words(g->d[0], g->d[1], g->d[2], g->d[3], seed);
}

static char const rcsid_gu[] GCC_ATTR_USED_VAR = "$Id:$";
