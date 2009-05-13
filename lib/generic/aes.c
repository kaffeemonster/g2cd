/*
 * aes.c
 * AES routines, generic implementation
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
 * derived from the Linux kernel aes_generic.c which is GPL2:
 *
 * Based on Brian Gladman's code.
 *
 * Linux developers:
 *  Alexander Kjeldaas <astor@fast.no>
 *  Herbert Valerio Riedel <hvr@hvrlab.org>
 *  Kyle McMartin <kyle@debian.org>
 *  Adam J. Richter <adam@yggdrasil.com> (conversion to 2.5 API).
 *
 * ---------------------------------------------------------------------------
 * Copyright (c) 2002, Dr Brian Gladman <brg@gladman.me.uk>, Worcester, UK.
 * All rights reserved.
 *
 * LICENSE TERMS
 *
 * The free distribution and use of this software in both source and binary
 * form is allowed (with or without changes) provided that:
 *
 *   1. distributions of this source code include the above copyright
 *      notice, this list of conditions and the following disclaimer;
 *
 *   2. distributions in binary form include the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other associated materials;
 *
 *   3. the copyright holder's name is not used to endorse products
 *      built using this software without specific written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this product
 * may be distributed under the terms of the GNU General Public License (GPL),
 * in which case the provisions of the GPL apply INSTEAD OF those given above.
 *
 * DISCLAIMER
 *
 * This software is provided 'as is' with no explicit or implied warranties
 * in respect of its properties, including, but not limited to, correctness
 * and/or fitness for purpose.
 * ---------------------------------------------------------------------------
 *
 * $Id$
 */

/*
 * WARNING
 * since we do not intend to encrypt/decrypt anything with this
 * atm, we ignore endianess.
 *
 * This is also very minimal...
 */
#ifndef ARCH_NAME_SUFFIX
# define F_NAME(z, x, y) z x
#else
# define F_NAME(z, x, y) static z x##y
#endif

/* names in aes_tab.o */
#define aes_ft_tab aes_ft_tab_base_data
#define aes_fl_tab aes_fl_tab_base_data
#define aes_it_tab aes_it_tab_base_data
#define aes_il_tab aes_il_tab_base_data

static const uint32_t rco_tab[10] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36};
extern const uint32_t aes_ft_tab[4][256];
extern const uint32_t aes_fl_tab[4][256];
extern const uint32_t aes_it_tab[4][256];
extern const uint32_t aes_il_tab[4][256];

static inline uint32_t ror32(uint32_t word, unsigned int shift)
{
	return (word >> shift) | (word << (32 - shift));
}

static inline uint8_t byte(const uint32_t x, const unsigned n)
{
	return x >> (n << 3);
}

#define ls_box(x) \
	aes_fl_tab[0][byte(x, 0)] ^ \
	aes_fl_tab[1][byte(x, 1)] ^ \
	aes_fl_tab[2][byte(x, 2)] ^ \
	aes_fl_tab[3][byte(x, 3)]

F_NAME(void, aes_encrypt_key128, _generic) (struct aes_encrypt_ctx *ctx, const void *in)
{
	const uint32_t *key = in;
	unsigned i;
	uint32_t t;

	memcpy(ctx->k, key, 16);

	t = ctx->k[3];
	for (i = 0; i < 10; ++i)
	{
		t = ror32(t, 8);
		t = ls_box(t) ^ rco_tab[i];
		t ^= ctx->k[4 * i];
		ctx->k[4 * i + 4] = t;
		t ^= ctx->k[4 * i + 1];
		ctx->k[4 * i + 5] = t;
		t ^= ctx->k[4 * i + 2];
		ctx->k[4 * i + 6] = t;
		t ^= ctx->k[4 * i + 3];
		ctx->k[4 * i + 7] = t;
	}
}

#define f_rn(bo, bi, n, k) \
	do { \
		bo[n] = aes_ft_tab[0][byte(bi[n], 0)] ^ \
		        aes_ft_tab[1][byte(bi[(n + 1) & 3], 1)] ^ \
		        aes_ft_tab[2][byte(bi[(n + 2) & 3], 2)] ^ \
		        aes_ft_tab[3][byte(bi[(n + 3) & 3], 3)] ^ *(k + n); \
	} while (0)

#define f_nround(bo, bi, k) \
	do { \
		f_rn(bo, bi, 0, k); \
		f_rn(bo, bi, 1, k); \
		f_rn(bo, bi, 2, k); \
		f_rn(bo, bi, 3, k); \
		k += 4; \
	} while (0)

#define f_rl(bo, bi, n, k) \
	do { \
		bo[n] = aes_fl_tab[0][byte(bi[n], 0)] ^ \
		        aes_fl_tab[1][byte(bi[(n + 1) & 3], 1)] ^ \
		        aes_fl_tab[2][byte(bi[(n + 2) & 3], 2)] ^ \
		        aes_fl_tab[3][byte(bi[(n + 3) & 3], 3)] ^ *(k + n); \
	} while (0)

#define f_lround(bo, bi, k) \
	do { \
		f_rl(bo, bi, 0, k); \
		f_rl(bo, bi, 1, k); \
		f_rl(bo, bi, 2, k); \
		f_rl(bo, bi, 3, k); \
	} while (0)

F_NAME(void, aes_ecb_encrypt, _generic) (const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	uint32_t b0[4], b1[4];
	const uint32_t *kp = ctx->k + 4;

	memcpy(b0, in, 16);
	b0[0] ^= ctx->k[0];
	b0[1] ^= ctx->k[1];
	b0[2] ^= ctx->k[2];
	b0[3] ^= ctx->k[3];

	f_nround(b1, b0, kp);
	f_nround(b0, b1, kp);
	f_nround(b1, b0, kp);
	f_nround(b0, b1, kp);
	f_nround(b1, b0, kp);
	f_nround(b0, b1, kp);
	f_nround(b1, b0, kp);
	f_nround(b0, b1, kp);
	f_nround(b1, b0, kp);
	f_lround(b0, b1, kp);

	memcpy(out, b0, 16);
}

/*@unused@*/
static char const rcsid_aesg[] GCC_ATTR_USED_VAR = "$Id$";
/* EOF */
