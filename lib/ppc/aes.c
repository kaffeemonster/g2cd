/*
 * aes.c
 * AES routines, ppc implementation
 *
 * Copyright (c) 2010-2025 Jan Seiffert
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
 * $Id:$
 */

#if defined(__ALTIVEC__)
# include <altivec.h>
# include "ppc_altivec.h"
# include "../my_bitopsm.h"

/*
 * original SSSE3 code by:
 * By Mike Hamburg (Stanford University), 2009
 * Public domain
 *
 * adapted by me
 */

/*
 * This is untested
 * surely broken as hell
 * and endian dependend, this is only tested on little endian, and not ppc,
 * so will also not work on little endian ppc
 */

// TODO: this big endian version prop. is still wrong
/* luckly we are only creating random numbers, not actually encrypting... */
static const struct
{
	vector unsigned char s0f;
	vector unsigned char s63;
	vector unsigned char iptlo;
	vector unsigned char ipthi;
	vector unsigned char sbolo;
	vector unsigned char sbohi;
	vector unsigned char invlo;
	vector unsigned char invhi;
	vector unsigned char sb1lo;
	vector unsigned char sb1hi;
	vector unsigned char sb2lo;
	vector unsigned char sb2hi;
	vector unsigned char mcf[4];
	vector unsigned char mcb[4];
	vector unsigned char sr[4];
	vector unsigned char rcon;
	vector unsigned char optlo;
	vector unsigned char opthi;
} aes_consts =
{
# if HOST_IS_BIGENDIAN == 1
	/*
	 * yes, these are the little endian numbers:
	 * big endian does not rotate the vectors, but the byte numbering within
	 * the register (for perm) for AltiVec goes from 0->MSB to 16->LSB
	 */
	.s0f   = {15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
	.s63   = {91,91,91,91,91,91,91,91,91,91,91,91,91,91,91,91},
	.iptlo = {0x00,0x70,0x2A,0x5A,0x98,0xE8,0xB2,0xC2,0x08,0x78,0x22,0x52,0x90,0xE0,0xBA,0xCA},
	.ipthi = {0x00,0x4D,0x7C,0x31,0x7D,0x30,0x01,0x4C,0x81,0xCC,0xFD,0xB0,0xFC,0xB1,0x80,0xCD},
	.sbolo = {0x00,0xC7,0xBD,0x6F,0x17,0x6D,0xD2,0xD0,0x78,0xA8,0x02,0xC5,0x7A,0xBF,0xAA,0x15},
	.sbohi = {0x00,0x6A,0xBB,0x5F,0xA5,0x74,0xE4,0xCF,0xFA,0x35,0x2B,0x41,0xD1,0x90,0x1E,0x8E},
	.invlo = {0x80,0x01,0x08,0x0D,0x0F,0x06,0x05,0x0E,0x02,0x0C,0x0B,0x0A,0x09,0x03,0x07,0x04},
	.invhi = {0x80,0x07,0x0B,0x0F,0x06,0x0A,0x04,0x01,0x09,0x08,0x05,0x02,0x0C,0x0E,0x0D,0x03},
	.sb1lo = {0x00,0x3E,0x50,0xCB,0x8F,0xE1,0x9B,0xB1,0x44,0xF5,0x2A,0x14,0x6E,0x7A,0xDF,0xA5},
	.sb1hi = {0x00,0x23,0xE2,0xFA,0x15,0xD4,0x18,0x36,0xEF,0xD9,0x2E,0x0D,0xC1,0xCC,0xF7,0x3B},
	.sb2lo = {0x00,0x24,0x71,0x0B,0xC6,0x93,0x7A,0xE2,0xCD,0x2F,0x98,0xBC,0x55,0xE9,0xB7,0x5E},
	.sb2hi = {0x00,0x29,0xE1,0x0A,0x40,0x88,0xEB,0x69,0x4A,0x23,0x82,0xAB,0xC8,0x63,0xA1,0xC2},
	.mcf   = {{0x01,0x02,0x03,0x00,0x05,0x06,0x07,0x04,0x09,0x0A,0x0B,0x08,0x0D,0x0E,0x0F,0x0C},
	          {0x05,0x06,0x07,0x04,0x09,0x0A,0x0B,0x08,0x0D,0x0E,0x0F,0x0C,0x01,0x02,0x03,0x00},
	          {0x09,0x0A,0x0B,0x08,0x0D,0x0E,0x0F,0x0C,0x01,0x02,0x03,0x00,0x05,0x06,0x07,0x04},
	          {0x0D,0x0E,0x0F,0x0C,0x01,0x02,0x03,0x00,0x05,0x06,0x07,0x04,0x09,0x0A,0x0B,0x08}},
	.mcb   = {{0x03,0x00,0x01,0x02,0x07,0x04,0x05,0x06,0x0B,0x08,0x09,0x0A,0x0F,0x0C,0x0D,0x0E},
	          {0x0F,0x0C,0x0D,0x0E,0x03,0x00,0x01,0x02,0x07,0x04,0x05,0x06,0x0B,0x08,0x09,0x0A},
	          {0x0B,0x08,0x09,0x0A,0x0F,0x0C,0x0D,0x0E,0x03,0x00,0x01,0x02,0x07,0x04,0x05,0x06},
	          {0x07,0x04,0x05,0x06,0x0B,0x08,0x09,0x0A,0x0F,0x0C,0x0D,0x0E,0x03,0x00,0x01,0x02}},
	.sr    = {{0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F},
	          {0x00,0x05,0x0A,0x0F,0x04,0x09,0x0E,0x03,0x08,0x0D,0x02,0x07,0x0C,0x01,0x06,0x0B},
	          {0x00,0x09,0x02,0x0B,0x04,0x0D,0x06,0x0F,0x08,0x01,0x0A,0x03,0x0C,0x05,0x0E,0x07},
	          {0x00,0x0D,0x0A,0x07,0x04,0x01,0x0E,0x0B,0x08,0x05,0x02,0x0F,0x0C,0x09,0x06,0x03}},
	.rcon  = {0xB6,0xEE,0x9D,0xAF,0xB9,0x91,0x83,0x1F,0x81,0x7D,0x7C,0x4D,0x08,0x98,0x2A,0x70},
	.optlo = {0x00,0x60,0xB6,0xD6,0x29,0x49,0x9F,0xFF,0x08,0x68,0xBE,0xDE,0x21,0x41,0x97,0xF7},
	.opthi = {0x00,0xEC,0xBC,0x50,0x51,0xBD,0xED,0x01,0xE0,0x0C,0x5C,0xB0,0xB1,0x5D,0x0D,0xE1},
# else
// TODO: does this represent little endian AltiVec?
	/* DWORDS are munged (proper swapped?), and the two DWORDS are swapped to QWORDs... */
	.s0f   = {15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
	.s63   = {91,91,91,91,91,91,91,91,91,91,91,91,91,91,91,91},
	.iptlo = {0xCA,0xBA,0xE0,0x90,0x52,0x22,0x78,0x08,0xC2,0xB2,0xE8,0x98,0x5A,0x2A,0x70,0x00},
	.ipthi = {0xCD,0x80,0xB1,0xFC,0xB0,0xFD,0xCC,0x81,0x4C,0x01,0x30,0x7D,0x31,0x7C,0x4D,0x00},
	.sbolo = {0x15,0xAA,0xBF,0x7A,0xC5,0x02,0xA8,0x78,0xD0,0xD2,0x6D,0x17,0x6F,0xBD,0xC7,0x00},
	.sbohi = {0x8E,0x1E,0x90,0xD1,0x41,0x2B,0x35,0xFA,0xCF,0xE4,0x74,0xA5,0x5F,0xBB,0x6A,0x00},
	.invlo = {0x04,0x07,0x03,0x09,0x0A,0x0B,0x0C,0x02,0x0E,0x05,0x06,0x0F,0x0D,0x08,0x01,0x80},
	.invhi = {0x03,0x0D,0x0E,0x0C,0x02,0x05,0x08,0x09,0x01,0x04,0x0A,0x06,0x0F,0x0B,0x07,0x80},
	.sb1lo = {0xA5,0xDF,0x7A,0x6E,0x14,0x2A,0xF5,0x44,0xB1,0x9B,0xE1,0x8F,0xCB,0x50,0x3E,0x00},
	.sb1hi = {0x3B,0xF7,0xCC,0xC1,0x0D,0x2E,0xD9,0xEF,0x36,0x18,0xD4,0x15,0xFA,0xE2,0x23,0x00},
	.sb2lo = {0x5E,0xB7,0xE9,0x55,0xBC,0x98,0x2F,0xCD,0xE2,0x7A,0x93,0xC6,0x0B,0x71,0x24,0x00},
	.sb2hi = {0xC2,0xA1,0x63,0xC8,0xAB,0x82,0x23,0x4A,0x69,0xEB,0x88,0x40,0x0A,0xE1,0x29,0x00},

	.mcf   = {{0x0C,0x0F,0x0E,0x0D,0x08,0x0B,0x0A,0x09,0x04,0x07,0x06,0x05,0x00,0x03,0x02,0x01}
	          {0x00,0x03,0x02,0x01,0x0C,0x0F,0x0E,0x0D,0x08,0x0B,0x0A,0x09,0x04,0x07,0x06,0x05}
	          {0x04,0x07,0x06,0x05,0x00,0x03,0x02,0x01,0x0C,0x0F,0x0E,0x0D,0x08,0x0B,0x0A,0x09}
	          {0x08,0x0B,0x0A,0x09,0x04,0x07,0x06,0x05,0x00,0x03,0x02,0x01,0x0C,0x0F,0x0E,0x0D}},
	.mcb   = {{0x0E,0x0D,0x0C,0x0F,0x0A,0x09,0x08,0x0B,0x06,0x05,0x04,0x07,0x02,0x01,0x00,0x03}
	          {0x0A,0x09,0x08,0x0B,0x06,0x05,0x04,0x07,0x02,0x01,0x00,0x03,0x0E,0x0D,0x0C,0x0F}
	          {0x06,0x05,0x04,0x07,0x02,0x01,0x00,0x03,0x0E,0x0D,0x0C,0x0F,0x0A,0x09,0x08,0x0B}
	          {0x02,0x01,0x00,0x03,0x0E,0x0D,0x0C,0x0F,0x0A,0x09,0x08,0x0B,0x06,0x05,0x04,0x07}},
	.sr    = {{0x0F,0x0E,0x0D,0x0C,0x0B,0x0A,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00}
	          {0x0B,0x06,0x01,0x0C,0x07,0x02,0x0D,0x08,0x03,0x0E,0x09,0x04,0x0F,0x0A,0x05,0x00}
	          {0x07,0x0E,0x05,0x0C,0x03,0x0A,0x01,0x08,0x0F,0x06,0x0D,0x04,0x0B,0x02,0x09,0x00}
	          {0x03,0x06,0x09,0x0C,0x0F,0x02,0x05,0x08,0x0B,0x0E,0x01,0x04,0x07,0x0A,0x0D,0x00}},
	.rcon  = {0x70,0x2A,0x98,0x08,0x4D,0x7C,0x7D,0x81,0x1F,0x83,0x91,0xB9,0xAF,0x9D,0xEE,0xB6},
	.optlo = {0xF7,0x97,0x41,0x21,0xDE,0xBE,0x68,0x08,0xFF,0x9F,0x49,0x29,0xD6,0xB6,0x60,0x00},
	.opthi = {0xE1,0x0D,0x5D,0xB1,0xB0,0x5C,0x0C,0xE0,0x01,0xED,0xBD,0x51,0x50,0xBC,0xEC,0x00},
# endif
};

static inline vector unsigned char aes_schedule_round_low(vector unsigned char key, vector unsigned char kt)
{
	vector unsigned char v_0 = vec_splat_u8(0), v_4 = vec_splat_u8(4);
	vector unsigned char l_4 = vec_lvsl( 4, (unsigned char *)NULL);
	vector unsigned char l_8 = vec_lvsl( 8, (unsigned char *)NULL);
	vector unsigned char klo, khi, klinv, khinv, khlinv;

	/* smear kt */
// TODO: left shift for big endian?
	kt     = vec_xor(vec_perm(kt, v_0, l_4), kt);
	kt     = vec_xor(vec_perm(kt, v_0, l_8), kt);
	kt     = vec_xor(kt, aes_consts.s63);
	/* subbytes */
	khi    = vec_sr(key, v_4);
	klo    = vec_and(key, aes_consts.s0f);
	klinv  = vec_perm(aes_consts.invhi, v_0, klo);
	klo    = vec_xor(klo, khi);
	khinv  = vec_perm(aes_consts.invlo, v_0, khi);
	khinv  = vec_xor(khinv, klinv);
	khlinv = vec_perm(aes_consts.invlo, v_0, klo);
	khlinv = vec_xor(khlinv, klinv);
	khinv  = vec_perm(aes_consts.invlo, v_0, khinv);
	khinv  = vec_xor(khinv, klo);
	khlinv = vec_perm(aes_consts.invlo, v_0, khlinv);
	khlinv = vec_xor(khlinv, khi);
	khinv  = vec_perm(aes_consts.sb1lo, v_0, khinv);
	khlinv = vec_perm(aes_consts.sb1hi, v_0, khlinv);
	key    = vec_xor(khinv, khlinv);
	/* add in smeared stuff */
	return   vec_xor(key, kt);
}

static inline vector unsigned char aes_schedule_round(vector unsigned char key, vector unsigned char kt, vector unsigned char *rcon)
{
	vector unsigned char v_0  = vec_splat_u8(0);
	vector unsigned char r_1  = vec_lvsr( 1, (unsigned char *)NULL);
	vector unsigned char r_15 = vec_lvsr(15, (unsigned char *)NULL);

// TODO: what's endian dependent in all this extract/rotate/foo?
	/* extract rcon */
// TODO: the highest element for big endian?
	kt     = vec_xor(vec_perm(v_0, *rcon, r_15), kt);
// TODO: rotating right for big endian?
	*rcon  = vec_perm(*rcon, *rcon, r_15);
	/* rotate */
// TODO: is this the element to splat?
	/* endianess + funny counting by powerpc... */
	key    = (vector unsigned char)vec_splat((vector unsigned int)key, 0);
// TODO: rotating right for big endian?
	key    = vec_perm(key, key, r_1);
	return aes_schedule_round_low(key, kt);
}

static inline vector unsigned char aes_schedule_transform(vector unsigned char key)
{
	vector unsigned char khi, klo;
	vector unsigned char v_0 = vec_splat_u8(0), v_4 = vec_splat_u8(4);

	/* shedule transform */
	khi  = vec_sr(key, v_4);
	klo  = vec_and(key, aes_consts.s0f);
	klo  = vec_perm(aes_consts.optlo, v_0, klo);
	khi  = vec_perm(aes_consts.opthi, v_0, khi);
	return vec_xor(khi, klo);
}

static inline vector unsigned char aes_schedule_mangle(vector unsigned char key, unsigned *n)
{
	vector unsigned char khi, klo;
	vector unsigned char v_0 = vec_splat_u8(0);

	/* write output */
	klo = key;
	klo = vec_xor(aes_consts.s63, klo);
	klo = vec_perm(aes_consts.mcf[0], v_0, klo);
	khi = klo;
	klo = vec_perm(aes_consts.mcf[0], v_0, klo);
	khi = vec_xor(khi, klo);
	klo = vec_perm(aes_consts.mcf[0], v_0, klo);
	khi = vec_xor(khi, klo);
	khi = vec_perm(khi, v_0, aes_consts.sr[(*n)--]);
	*n  &= 2;
	return khi;
}

static inline vector unsigned char aes_schedule_mangle_last(vector unsigned char key, unsigned n)
{
	vector unsigned char v_0 = vec_splat_u8(0);

	/* schedule last round key */
	key = vec_perm(key, v_0, aes_consts.sr[n]);
	key = vec_xor(aes_consts.s63, key);

	return aes_schedule_transform(key);
}

void aes_encrypt_key128(struct aes_encrypt_ctx *ctx, const void *in)
{
	vector unsigned char key, *key_target, rcon;
	int rounds = 10;
	unsigned n = 1;

	key_target = (vector unsigned char *)ctx->k;
	prefetchw(key_target);
	prefetch(&aes_consts);
	prefetch(&aes_consts.iptlo);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	rcon = aes_consts.rcon;
	if(IS_ALIGN(in, SOVUC))
		key = vec_ldl(0, (const unsigned char *)in);
	else
	{
		vector unsigned char vperm, klo, khi;
		if(HOST_IS_BIGENDIAN)
			vperm = vec_lvsl(0, (const unsigned char *)in);
		else
			vperm = vec_lvsr(0, (const unsigned char *)in);
		klo = vec_ldl(    0, (const unsigned char *)in);
		khi = vec_ldl(SOVUC, (const unsigned char *)in);
		if(HOST_IS_BIGENDIAN)
			key = vec_perm(klo, khi, vperm);
		else
			key = vec_perm(khi, klo, vperm);
	}
	/* input transform */
	key = aes_schedule_transform(key);
	/* output zeroth round key */
	*key_target = key;
	do
	{
		key = aes_schedule_round(key, key, &rcon);
		if(--rounds == 0)
			break;
		*++key_target = aes_schedule_mangle(key, &n);
	} while(1);
	*++key_target = aes_schedule_mangle_last(key, n);
}

void aes_encrypt_key256(struct aes_encrypt_ctx *ctx, const void *in)
{
	vector unsigned char key_lo, key_hi, *key_target, rcon;
	int rounds = 10;
	unsigned n = 1;

	key_target = (vector unsigned char *)ctx->k;
	prefetchw(key_target);
	prefetch(&aes_consts);
	prefetch(&aes_consts.iptlo);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	rcon = aes_consts.rcon;
	if(IS_ALIGN(in, SOVUC)) {
		key_hi = vec_ldl(0, (const unsigned char *)in);
		key_lo = vec_ldl(16, (const unsigned char *)in);
	}
	else
	{
		vector unsigned char vperm, kl, km, kh;
		if(HOST_IS_BIGENDIAN)
			vperm = vec_lvsl(0, (const unsigned char *)in);
		else
			vperm = vec_lvsr(0, (const unsigned char *)in);
		kl = vec_ldl(      0, (const unsigned char *)in);
		km = vec_ldl(  SOVUC, (const unsigned char *)in);
		kh = vec_ldl(2*SOVUC, (const unsigned char *)in);
		if(HOST_IS_BIGENDIAN) {
			key_hi = vec_perm(kl, km, vperm);
			key_lo = vec_perm(km, kh, vperm);
		} else {
			key_hi = vec_perm(kh, km, vperm);
			key_lo = vec_perm(km, kl, vperm);
		}
	}
	/* input transform */
	key_hi = aes_schedule_transform(key_hi);
	key_lo = aes_schedule_transform(key_lo);
	do
	{
		*++key_target = aes_schedule_mangle(key_lo, &n);
		key_hi = aes_schedule_round(key_lo, key_hi, &rcon);
		if(--rounds == 0)
			break;
		*++key_target = aes_schedule_mangle(key_hi, &n);
		key_lo = aes_schedule_round_low((vector unsigned char)vec_splat((vector unsigned int)key_hi, 0), key_lo);
	} while(1);
	*++key_target = aes_schedule_mangle_last(key_hi, n);
}

#if !(defined(__CRYPTO) || defined(_ARCH_PWR8) || defined(_ARCH_PWR9))
static void aes_ecb_encrypt(const struct aes_encrypt_ctx *ctx, void *dout, const void *din, int mrounds)
{
	vector unsigned char v_0, v_4;
	const vector unsigned char *key_store;
	vector unsigned char in, ilo, ihi, ilhinv, ihlinv, ihllinv;
	int rounds = mrounds, n = 1;

	key_store = (const vector unsigned char *)ctx->k;

	prefetch(key_store);
	prefetch(&aes_consts);
	prefetch(&aes_consts.iptlo);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	v_0 = vec_splat_u8(0);
	v_4 = vec_splat_u8(4);
	if(IS_ALIGN(din, SOVUC))
		in = vec_ldl(0, (const unsigned char *)din);
	else
	{
		vector unsigned char vperm;
		if(HOST_IS_BIGENDIAN)
			vperm = vec_lvsl(0, (const unsigned char *)din);
		else
			vperm = vec_lvsr(0, (const unsigned char *)din);
		ilo = vec_ldl(    0, (const unsigned char *)din);
		ihi = vec_ldl(SOVUC, (const unsigned char *)din);
		if(HOST_IS_BIGENDIAN)
			in = vec_perm(ilo, ihi, vperm);
		else
			in = vec_perm(ihi, ilo, vperm);
	}

	ihi = vec_sr(in, v_4);
	ilo = vec_and(in, aes_consts.s0f);
	ilo = vec_perm(aes_consts.iptlo, v_0, ilo);
	ihi = vec_perm(aes_consts.ipthi, v_0, ihi);
	ilo = vec_xor(*++key_store, ilo);
	in  = vec_xor(ihi, ilo);
	goto start_round;
	do
	{
		/* middle of middle round */
		ihllinv = vec_perm(aes_consts.sb1lo, v_0, ilhinv);
		ihllinv = vec_xor(*++key_store, ihllinv);
		ilo     = vec_perm(aes_consts.sb1hi, v_0, ihlinv);
		ilo     = vec_xor(ilo, ihllinv);
		ihllinv = vec_perm(aes_consts.sb2lo, v_0, ilhinv);
		ilhinv  = vec_perm(aes_consts.sb2hi, v_0, ihlinv);
		ilhinv  = vec_xor(ilhinv, ihllinv);
		ihlinv  = vec_perm(ilo, v_0, aes_consts.mcb[n]);
		ilo     = vec_perm(ilo, v_0, aes_consts.mcf[n]);
		ilo     = vec_xor(ilo, ilhinv);
		ihlinv  = vec_xor(ihlinv, ilo);
		ilo     = vec_perm(ilo, v_0, aes_consts.mcf[n]);
		in      = vec_xor(ilo, ihlinv);
		n = (n + 1) % 4;
start_round:
		/* top of round */
		ihi     = vec_sr(in, v_4);
		ilo     = vec_and(in, aes_consts.s0f);
		ilhinv  = vec_perm(aes_consts.invhi, v_0, ilo);
		ilo     = vec_xor(ilo, ihi);
		ihlinv  = vec_perm(aes_consts.invlo, v_0, ihi);
		ihlinv  = vec_xor(ilhinv, ihlinv);
		ihllinv = vec_perm(aes_consts.invlo, v_0, ilo);
		ihllinv = vec_xor(ilhinv, ihllinv);
		ilhinv  = vec_perm(aes_consts.invlo, v_0, ihlinv);
		ilhinv  = vec_xor(ilo, ilhinv);
		ihlinv  = vec_perm(aes_consts.invlo, v_0, ihllinv);
		ihlinv  = vec_xor(ilo, ihlinv);
	} while(--rounds);
	prefetchw(dout);
	/* middle of last round */
	ihllinv = vec_perm(aes_consts.sbolo, v_0, ilhinv);
	ihllinv = vec_xor(*key_store, ihllinv);
	ilo     = vec_perm(aes_consts.sbohi, v_0, ihlinv);
	ilo     = vec_xor(ilo, ihllinv);
	in      = vec_perm(ilo, v_0, aes_consts.sr[n]);
	if(IS_ALIGN(dout, SOVUC))
		vec_st(in, 0, (unsigned char *)dout);
	else
	{
// TODO: unaligned vector store
		memcpy(dout, &in, sizeof(in));
	}
}
#else
static inline vector unsigned char vec_vcipher(vector unsigned char state, vector unsigned char key)
{
// TODO: inline asm fallback if compiler does not support the builtin
	return (vector unsigned char) __builtin_crypto_vcipher((vector unsigned long long)state, (vector unsigned long long)key);
}

static inline vector unsigned char vec_vcipherlast(vector unsigned char state, vector unsigned char key)
{
	return (vector unsigned char) __builtin_crypto_vcipherlast((vector unsigned long long)state, (vector unsigned long long)key);
}

static void aes_ecb_encrypt(const struct aes_encrypt_ctx *ctx, void *dout, const void *din, int mrounds)
{
	const vector unsigned char *key_store;
	vector unsigned char in;
	int rounds = mrounds;

	key_store = (const vector unsigned char *)ctx->k;

	prefetch(key_store);
	if(IS_ALIGN(din, SOVUC))
		in = vec_ldl(0, (const unsigned char *)din);
	else
	{
		vector unsigned char vperm, ilo, ihi;
		if(HOST_IS_BIGENDIAN)
			vperm = vec_lvsl(0, (const unsigned char *)din);
		else
			vperm = vec_lvsr(0, (const unsigned char *)din);
		ilo = vec_ldl(    0, (const unsigned char *)din);
		ihi = vec_ldl(SOVUC, (const unsigned char *)din);
		if(HOST_IS_BIGENDIAN)
			in = vec_perm(ilo, ihi, vperm);
		else
			in = vec_perm(ihi, ilo, vperm);
	}

	in = vec_xor(*++key_store, in);
// TODO: number of rounds correct?
	do {
		in = vec_vcipher(in, *++key_store);
	} while(--rounds);
	prefetchw(dout);
	in = vec_vcipherlast(in, *key_store);
	if(IS_ALIGN(dout, SOVUC))
		vec_st(in, 0, (unsigned char *)dout);
	else
	{
// TODO: unaligned vector store
		memcpy(dout, &in, sizeof(in));
	}
}
#endif

void aes_ecb_encrypt128(const struct aes_encrypt_ctx *ctx, void *dout, const void *din)
{
	aes_ecb_encrypt(ctx, dout, din, 10-1);
}

void aes_ecb_encrypt256(const struct aes_encrypt_ctx *ctx, void *dout, const void *din)
{
	aes_ecb_encrypt(ctx, dout, din, 14-1);
}

/*@unused@*/
static char const rcsid_aesp[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/aes.c"
#endif

