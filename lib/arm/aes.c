/*
 * aes.c
 * AES routines, arm implementation
 *
 * Copyright (c) 2010-2015 Jan Seiffert
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
#if 1
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
	 * 7 to writeback (whatever that means...)), ...
	 */
	asm ("vtbl.8 %f0, %h1, %f2"
		: /* %0 */ "=w" (ret)
		: /* %1 */ "w" (x),
		  /* %2 */ "w" (idx),
		  /* %3 */ "0" (ret)
	);
	return ret;
#else
	/*
	 * ... but otherwise the compiler would totaly f*** it up:
	 * gcc is to stupid to exploit that d & q regs alias, this vget_*
	 * and vcombine_* stuff generates aweful instruction sequences.
	 *
	 * Result is spills and moves all over the place, because he runs
	 * out of regs, because he wants to move stuff around, because he
	 * can't keep an uint8x16 in a proper pair but instead really emits
	 * an instruction on combine or get_low/high instead of simply aliasing.
	 *
	 * If the compiler would just keep it in his pants, code would be
	 * nice, clean, short and mostly spill free, then proper sheduling
	 * could be done.
	 *
	 * Example, code above:
	 *  vrshr.u8        q11, q11, #4
	 *  vand    q12, q12, q14
	 *  subs    ip, ip, #1
	 *  veor    q9, q12, q11
	 *  add     r3, r5, r2, lsl #4
	 *  vtbl.8  d20, {d26-d27}, d24
	 *  vtbl.8  d0, {d16-d17}, d18
	 *  vtbl.8  d21, {d26-d27}, d25
	 *  vtbl.8  d1, {d16-d17}, d19
	 *  vtbl.8  d24, {d16-d17}, d22
	 *  veor    q0, q10, q0
	 *  vtbl.8  d25, {d16-d17}, d23
	 *  add     r4, r2, #1
	 *  veor    q10, q10, q12
	 *  vtbl.8  d22, {d16-d17}, d0
	 *  vtbl.8  d24, {d16-d17}, d20
	 *  vtbl.8  d23, {d16-d17}, d1
	 *  vtbl.8  d25, {d16-d17}, d21
	 *  veor    q11, q9, q11
	 *  mov     r6, r3
	 *  veor    q9, q9, q12
	 *
	 * only loads from the constant pool (pc relative).
	 *
	 * have the compiler have it's way:
	 *  vrshr.u8        q1, q1, #4
	 *  vand    q3, q3, q5
	 *  vorr    d8, d16, d16
	 *  add     r3, sp, #48     ; 0x30
	 *  vorr    d9, d17, d17
	 *  veor    q5, q3, q1
	 *  vtbl.8  d28, {d8-d9}, d3
	 *  vtbl.8  d29, {d16-d17}, d2
	 *  vorr    d0, d18, d18
	 *  vstr    d16, [sp, #48]  ; 0x30
	 *  vorr    d1, d19, d19
	 *  vstr    d17, [sp, #56]  ; 0x38
	 *  vld1.64 {d2-d3}, [r3 :64]
	 *  add     r3, sp, #64     ; 0x40
	 *  vtbl.8  d30, {d0-d1}, d7
	 *  vtbl.8  d27, {d2-d3}, d10
	 *  vstr    d16, [sp, #64]  ; 0x40
	 *  vstr    d17, [sp, #72]  ; 0x48
	 *  vtbl.8  d31, {d18-d19}, d6
	 *  vld1.64 {d2-d3}, [r3 :64]
	 *  vswp    d30, d31
	 *  vtbl.8  d26, {d2-d3}, d11
	 *  add     r3, sp, #16
	 *  vswp    d28, d29
	 *  veor    q14, q15, q14
	 *  vstr    d16, [sp, #16]
	 *  vswp    d26, d27
	 *  vstr    d17, [sp, #24]
	 *  vld1.64 {d2-d3}, [r3 :64]
	 *  add     r3, sp, #32
	 *  veor    q13, q15, q13
	 *  vtbl.8  d7, {d2-d3}, d28
	 *  vstr    d16, [sp, #32]
	 *  vorr    d14, d16, d16
	 *  vorr    d12, d8, d8
	 *  vstr    d17, [sp, #40]  ; 0x28
	 *  vorr    d15, d17, d17
	 *  vld1.64 {d2-d3}, [r3 :64]
	 *  vorr    d13, d17, d17
	 *  vtbl.8  d29, {d2-d3}, d29
	 *  vtbl.8  d28, {d14-d15}, d26
	 *  subs    ip, ip, #1
	 *  vtbl.8  d26, {d12-d13}, d27
	 *  vorr    d30, d7, d7
	 *  vorr    d31, d29, d29
	 *  vorr    d2, d28, d28
	 *  vorr    d3, d26, d26
	 *  veor    q15, q5, q15
	 *  veor    q1, q5, q
	 *
	 * Silly much?
	 * And that is just a short excerpt, it is even worse in the
	 * rest of the code.
	 */
	#define uint8x16_to_8x8x2(v) ((uint8x8x2_t){{vget_low_u8(v), vget_high_u8(v)}})
	return vcombine_u8(vtbl2_u8(uint8x16_to_8x8x2(x), vget_low_u8(idx)),
	                   vtbl2_u8(uint8x16_to_8x8x2(x), vget_high_u8(idx)));
#endif
}

static inline uint8x16_t aes_schedule_round_low(uint8x16_t key, uint8x16_t kt)
{
	uint8x16_t v_0 = (uint8x16_t){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint8x16_t klo, khi, klinv, khinv, khlinv;

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
	return   veorq_u8(key, kt);
}

static inline uint8x16_t aes_schedule_round(uint8x16_t key, uint8x16_t kt, uint8x16_t *rcon)
{
	uint8x16_t v_0 = (uint8x16_t){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	/* extract rcon */
	kt      = veorq_u8(vextq_u8(*rcon, v_0, 15), kt);
	*rcon   = vextq_u8(*rcon, *rcon, 15);
	/* rotate */
	key    = (uint8x16_t)vdupq_lane_u32((uint32x2_t)vget_high_u8(key), 1);
	key    = vextq_u8(key, key, 1);
	return   aes_schedule_round_low(key, kt);
}

static inline uint8x16_t aes_schedule_transform(uint8x16_t key)
{
	uint8x16_t khi, klo;

	/* shedule transform */
	khi = vrshrq_n_u8(vbicq_u8(key, aes_consts.s0f), 4);
	klo = vandq_u8(key, aes_consts.s0f);
	klo = pshufb(aes_consts.iptlo, klo);
	khi = pshufb(aes_consts.ipthi, khi);
	return veorq_u8(khi, klo);
}

static inline uint8x16_t aes_schedule_mangle(uint8x16_t key, unsigned *n)
{
	uint8x16_t klo, khi;

	/* write output */
	klo = key;
	klo = veorq_u8(aes_consts.s63, klo);
	klo = pshufb(aes_consts.mcf[0], klo);
	khi = klo;
	klo = pshufb(aes_consts.mcf[0], klo);
	khi = veorq_u8(khi, klo);
	klo = pshufb(aes_consts.mcf[0], klo);
	khi = veorq_u8(khi, klo);
	khi = pshufb(khi, aes_consts.sr[(*n)--]);
	*n &= 2;
	return khi;
}

static inline uint8x16_t aes_schedule_mangle_last(uint8x16_t key, unsigned n)
{
	/* schedule last round key */
	key = pshufb(key, aes_consts.sr[n]);
	key = veorq_u8(aes_consts.s63, key);

	return aes_schedule_transform(key);
}

void aes_encrypt_key128(struct aes_encrypt_ctx *ctx, const void *in)
{
	uint8x16_t key, kt, *key_target, rcon;
	unsigned rounds = 10, n = 1;

	key_target = (uint8x16_t *)ctx->k;
	prefetchw(key_target);
	prefetch(&aes_consts);
	prefetch(&aes_consts.iptlo);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	rcon  = aes_consts.rcon;
	key = vld1q_u8(in); /* load key */
	/* input transform */
	kt = aes_schedule_transform(key);
	/* output zeroth round key */
	*key_target = key;
	do
	{
		key = aes_schedule_round(key, kt, &rcon);
		if(--rounds == 0)
			break;
		*++key_target = aes_schedule_mangle(key, &n);
	} while(1);
	*++key_target = aes_schedule_mangle_last(key, n);
}

void aes_encrypt_key256(struct aes_encrypt_ctx *ctx, const void *in)
{
	uint8x16_t key_lo, key_hi, *key_target, rcon;
	unsigned rounds = 7, n = 1;

	key_target = (uint8x16_t *)ctx->k;
	prefetchw(key_target);
	prefetch(&aes_consts);
	prefetch(&aes_consts.iptlo);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	rcon  = aes_consts.rcon;
	key_hi = vld1q_u8(in); /* load key */
	key_lo = vld1q_u8(((const unsigned char *)in)+sizeof(key_hi)); /* load key */

	key_hi = aes_schedule_transform(key_hi);
	key_lo = aes_schedule_transform(key_lo);
	do
	{
		*++key_target = aes_schedule_mangle(key_lo, &n);
		key_hi = aes_schedule_round(key_lo, key_hi, &rcon);
		if(--rounds == 0)
			break;
		*++key_target = aes_schedule_mangle(key_hi, &n);
		key_lo = aes_schedule_round_low((uint8x16_t)vdupq_lane_u32((uint32x2_t)vget_high_u8(key_hi), 1), key_lo);
	} while(1);
	*++key_target = aes_schedule_mangle_last(key_hi, n);
}

#if !(defined(__ARM_ACLE) || defined(__ARM_FEATURE_CRYPTO))
static void aes_ecb_encrypt(const struct aes_encrypt_ctx *ctx, void *dout, const void *din, int mrounds)
{
	const uint8x16_t *key_store;
	uint8x16_t in, ilo, ihi, ilhinv, ihlinv, ihllinv;
	int rounds = mrounds, n = 1;

	key_store = (const uint8x16_t *)ctx->k;

	prefetch(key_store);
	prefetch(&aes_consts);
	prefetch(&aes_consts.iptlo);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	in = vld1q_u8(din);

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
	vst1q_u8(dout, in);
}
#else
# include <arm_acle.h>
static void aes_ecb_encrypt(const struct aes_encrypt_ctx *ctx, void *dout, const void *din, int mrounds)
{
	const uint8x16_t *key_store;
	uint8x16_t in;
	int rounds = mrounds-1;

	key_store = (const uint8x16_t *)ctx->k;

	prefetch(key_store);
	in = vld1q_u8(din);

//TODO: number of rounds
	do
	{
		/* single full round */
		in = vaeseq_u8(in, *key_store++);
		in = vaesmcq_u8(in);
	} while(--rounds);
	prefetchw(dout);
	/* last round  */
	in = vaeseq_u8(in, *key_store++);
	in = veorq_u8(in, *key_store);
	vst1q_u8(dout, in);
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
static char const rcsid_aesa[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/aes.c"
#endif

