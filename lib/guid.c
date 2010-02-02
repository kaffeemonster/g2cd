/*
 * guid.c
 * little stuff to generate a guid
 *
 * Copyright (c) 2010 Jan Seiffert
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

#include <pthread.h>
#include "other.h"
#include "guid.h"
#include "aes.h"
#include "ansi_prng.h"

static struct aes_encrypt_ctx ae_ctx;
static pthread_mutex_t ctx_lock;

void guid_generate(unsigned char out[GUID_SIZE])
{
	/* get random bytes for our guid */
	random_bytes_get(out, sizeof(out));

	/* encrypt those bytes with our random key */
	pthread_mutex_lock(&ctx_lock);
	aes_ecb_encrypt(&ae_ctx, out, out);
	pthread_mutex_unlock(&ctx_lock);

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
	unsigned char key[16];

	/* get random bytes for a key */
	random_bytes_get(&key, sizeof(key));
	/* create the new key */
	pthread_mutex_lock(&ctx_lock);
	aes_encrypt_key128(&ae_ctx, key);
	pthread_mutex_unlock(&ctx_lock);
}

void guid_init(void)
{
	pthread_mutex_init(&ctx_lock, NULL);
	guid_tick();
}
