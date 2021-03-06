/*
 * aes.h
 * header for AES functions
 *
 * Copyright (c) 2009-2015 Jan Seiffert
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

#ifndef LIB_AES_H
# define LIB_AES_H

# include "other.h"

# define LIB_AES_EXTRN(x) x GCC_ATTR_VIS("hidden")
# define LIB_AES_EXTRN_I(x) x

/*
 * list of AES key lengths
 * key length in bit | num 16 byte blocks
 * ------------------+-------------------
 *     128           |       11
 *     192           |       13
 *     256           |       15
 *
 */
# define AES_MAX_KEY_LEN (15 * 16)

struct aes_encrypt_ctx
{
	uint32_t k[AES_MAX_KEY_LEN/4];
} GCC_ATTR_ALIGNED(16);

LIB_AES_EXTRN(void aes_encrypt_key128(struct aes_encrypt_ctx *, const void *));
LIB_AES_EXTRN(void aes_encrypt_key256(struct aes_encrypt_ctx *, const void *));
LIB_AES_EXTRN(void aes_ecb_encrypt128(const struct aes_encrypt_ctx *restrict, void *restrict out, const void *restrict in));
LIB_AES_EXTRN(void aes_ecb_encrypt256(const struct aes_encrypt_ctx *restrict, void *restrict out, const void *restrict in));
#endif
