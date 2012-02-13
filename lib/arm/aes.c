/*
 * aes.c
 * AES routines, arm implementation
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
 * $Id:$
 */

// TODO: write big endian version
// TODO: be arm NEON has some kind of PDP11 endianess??
/*
 * NEON is more a 64 bit SIMD, one 64 Bit unit has nativ endianess,
 * but two 64 Bit have a fixed endianess, so on big endian they are wrong
 * way round (only little endian is "normal")
 */

#include "my_neon.h"
#if defined(ARM_NEON_SANE)
# include <arm_neon.h>

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
 * and endian dependend, this only works with little endian
 */

static const struct
{
	uint8x16_t s0f;
	uint8x16_t s63;
	uint8x16_t iptlo;
	uint8x16_t ipthi;
	uint8x16_t sbolo;
	uint8x16_t sbohi;
	uint8x16_t invlo;
	uint8x16_t invhi;
	uint8x16_t sb1lo;
	uint8x16_t sb1hi;
	uint8x16_t sb2lo;
	uint8x16_t sb2hi;
	uint8x16_t mcf[4];
	uint8x16_t mcb[4];
	uint8x16_t sr[4];
	uint8x16_t rcon;
	uint8x16_t optlo;
	uint8x16_t opthi;
} aes_consts =
{
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
};

static inline uint8x16_t pshufb(uint8x16_t x, uint8x16_t idx)
{
	uint8x16_t ret;
	asm ("vtbl.8 %e0, %h1, %e2"
		: /* %0 */ "=w" (ret)
		: /* %1 */ "w" (x),
		  /* %2 */ "w" (idx)
	);
	/*
	 * give the compiler a chance to schedule the two vtbl
	 * independently by putting them in two asms
	 *
	 * scheduling still sucks, because the compiler does not know
	 * that this are expensive instructions (2 cycles, 3 to result,
	 * 7 to writeback (whatever that means...)), but otherwise the
	 * compiler would totaly f*** it up:
	 * gcc is to stupid to exploit that d & q regs alias,
	 * this vget_* and vcombine_* stuff generates aweful
	 * instruction sequences...
	 *
	 * return vcombine_u8(vtbl2_u8(x, vget_low_u8(idx)), vtbl2_u8(x, vget_high_u8(idx)));
	 */
	asm ("vtbl.8 %f0, %h1, %f2"
		: /* %0 */ "=w" (ret)
		: /* %1 */ "w" (x),
		  /* %2 */ "w" (idx),
		  /* %3 */ "0" (ret)
	);
	return ret;
}

void aes_encrypt_key128(struct aes_encrypt_ctx *ctx, const void *in)
{
	uint8x16_t v_0;
	uint8x16_t key, klo, khi, kt, klinv, khinv, khlinv, *key_target, rcon;
	int rounds = 10, n = 1;

	key_target = (uint8x16_t *)ctx->k;
	prefetchw(key_target);
	prefetch(&aes_consts);
	prefetch(&aes_consts.iptlo);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	v_0   = (uint8x16_t){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	rcon  = aes_consts.rcon;
	memcpy(&key, in, sizeof(key)); /* load key */
	/* input transform */
	/* shedule transform */
	khi = vrshrq_n_u8(vbicq_u8(key, aes_consts.s0f), 4);
	klo = vandq_u8(key, aes_consts.s0f);
	klo = pshufb(aes_consts.iptlo, klo);
	khi = pshufb(aes_consts.ipthi, khi);
	key = veorq_u8(khi, klo);
	kt  = key;
	/* output zeroth round key */
	*key_target = key;
	do
	{
		/* extract rcon */
		kt     = veorq_u8(vextq_u8(rcon, v_0, 15), kt);
		rcon   = vextq_u8(rcon, rcon, 15);
		/* rotate */
		key    = (uint8x16_t)vdupq_lane_u32((uint32x2_t)vget_high_u8(key), 1);
		key    = vextq_u8(key, key, 1);
		/* smear kt */
		kt     = veorq_u8(vextq_u8(v_0, kt, 16 - 4), kt);
		kt     = veorq_u8(vextq_u8(v_0, kt, 16 - 8), kt);
		kt     = veorq_u8(kt, aes_consts.s63);
		/* subbytes */
		khi    = vrshrq_n_u8(vbicq_u8(key, aes_consts.s0f), 4);
		klo    = vandq_u8(key, aes_consts.s0f);
		klinv  = pshufb(aes_consts.invhi, klo);
		klo    = veorq_u8(klo, khi);
		khinv  = pshufb(aes_consts.invlo, khi);
		khinv  = veorq_u8(khinv, klinv);
		khlinv = pshufb(aes_consts.invlo, klo);
		khlinv = veorq_u8(khlinv, klinv);
		khinv  = pshufb(aes_consts.invlo, khinv);
		khinv  = veorq_u8(khinv, klo);
		khlinv = pshufb(aes_consts.invlo, khlinv);
		khlinv = veorq_u8(khlinv, khi);
		khinv  = pshufb(aes_consts.sb1lo, khinv);
		khlinv = pshufb(aes_consts.sb1hi, khlinv);
		key    = veorq_u8(khinv, khlinv);
		/* add in smeared stuff */
		key    = veorq_u8(key, kt);
		kt     = key;
		if(--rounds == 0)
			break;
		/* write output */
		klo = key;
		klo = veorq_u8(aes_consts.s63, klo);
		klo = pshufb(aes_consts.mcf[0], klo);
		khi = klo;
		klo = pshufb(aes_consts.mcf[0], klo);
		khi = veorq_u8(khi, klo);
		klo = pshufb(aes_consts.mcf[0], klo);
		khi = veorq_u8(khi, klo);
		khi = pshufb(khi, aes_consts.sr[n--]);
		n  &= 2;
		*++key_target = khi;
	} while(1);
	/* schedule last round key */
	key = pshufb(key, aes_consts.sr[n]);
	key = veorq_u8(aes_consts.s63, key);
	khi = vrshrq_n_u8(vbicq_u8(key, aes_consts.s0f), 4);
	klo = vandq_u8(key, aes_consts.s0f);
	klo = pshufb(aes_consts.optlo, klo);
	khi = pshufb(aes_consts.opthi, khi);
	key = veorq_u8(khi, klo);
	*++key_target = key;
}

void aes_ecb_encrypt(const struct aes_encrypt_ctx *ctx, void *dout, const void *din)
{
	uint8x16_t v_0;
	const uint8x16_t *key_store;
	uint8x16_t in, ilo, ihi, ilhinv, ihlinv, ihllinv;
	int rounds = 10 - 1, n = 1;

	key_store = (const uint8x16_t *)ctx->k;

	prefetch(key_store);
	prefetch(&aes_consts);
	prefetch(&aes_consts.iptlo);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	v_0   = (uint8x16_t){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	memcpy(&in, din, sizeof(in));

	ihi = vrshrq_n_u8(vbicq_u8(in, aes_consts.s0f), 4);
	ilo = vandq_u8(in, aes_consts.s0f);
	ilo = pshufb(aes_consts.iptlo, ilo);
	ihi = pshufb(aes_consts.ipthi, ihi);
	ilo = veorq_u8(*++key_store, ilo);
	in  = veorq_u8(ihi, ilo);
	goto start_round;
	do
	{
		/* middle of middle round */
		ihllinv = pshufb(aes_consts.sb1lo, ilhinv);
		ihllinv = veorq_u8(*++key_store, ihllinv);
		ilo     = pshufb(aes_consts.sb1hi, ihlinv);
		ilo     = veorq_u8(ilo, ihllinv);
		ihllinv = pshufb(aes_consts.sb2lo, ilhinv);
		ilhinv  = pshufb(aes_consts.sb2hi, ihlinv);
		ilhinv  = veorq_u8(ilhinv, ihllinv);
		ihlinv  = pshufb(ilo, aes_consts.mcb[n]);
		ilo     = pshufb(ilo, aes_consts.mcf[n]);
		ilo     = veorq_u8(ilo, ilhinv);
		ihlinv  = veorq_u8(ihlinv, ilo);
		ilo     = pshufb(ilo, aes_consts.mcf[n]);
		in      = veorq_u8(ilo, ihlinv);
		n = (n + 1) % 4;
start_round:
		/* top of round */
		ihi     = vrshrq_n_u8(vbicq_u8(in, aes_consts.s0f), 4);
		ilo     = vandq_u8(in, aes_consts.s0f);
		ilhinv  = pshufb(aes_consts.invhi, ilo);
		ilo     = veorq_u8(ilo, ihi);
		ihlinv  = pshufb(aes_consts.invlo, ihi);
		ihlinv  = veorq_u8(ilhinv, ihlinv);
		ihllinv = pshufb(aes_consts.invlo, ilo);
		ihllinv = veorq_u8(ilhinv, ihllinv);
		ilhinv  = pshufb(aes_consts.invlo, ihlinv);
		ilhinv  = veorq_u8(ilo, ilhinv);
		ihlinv  = pshufb(aes_consts.invlo, ihllinv);
		ihlinv  = veorq_u8(ilo, ihlinv);
	} while(--rounds);
	prefetchw(dout);
	/* middle of last round */
	ihllinv = pshufb(aes_consts.sbolo, ilhinv);
	ihllinv = veorq_u8(*key_store, ihllinv);
	ilo     = pshufb(aes_consts.sbohi, ihlinv);
	ilo     = veorq_u8(ilo, ihllinv);
	in      = pshufb(ilo, aes_consts.sr[n]);
	memcpy(dout, &in, sizeof(in));
}

/*@unused@*/
static char const rcsid_aesa[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/aes.c"
#endif

