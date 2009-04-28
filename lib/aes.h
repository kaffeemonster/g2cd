/*
 * aes.h
 * header for AES functions
 *
 * Copyright (c) 2009 Jan Seiffert
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

#ifndef LIB_AES_H
#define LIB_AES_H

/*
 * list of AES key lengths
 * key length in bit | num 16 byte blocks
 * ------------------+-------------------
 *     128           |       11
 *     192           |       13
 *     256           |       15
 *
 * so for 128-bit atm we only need 11x16 byte
 * blocks
 */
#define AES_MAX_KEY_LEN (11 * 16)

struct aes_encrypt_ctx
{
	uint32_t k[AES_MAX_KEY_LEN/4];
};

void aes_encrypt_key128(struct aes_encrypt_ctx *, const void *);
void aes_ecb_encrypt(const struct aes_encrypt_ctx *, void *out, const void *in);

#endif
