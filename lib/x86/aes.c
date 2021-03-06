/*
 * aes.c
 * AES routines, x86 implementation
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
 * $Id:$
 */

#include "../my_bitops.h"
#include "x86.h"
#include "x86_features.h"

static void aes_encrypt_key128_generic(struct aes_encrypt_ctx *, const void *);
static void aes_encrypt_key256_generic(struct aes_encrypt_ctx *, const void *);
static void aes_ecb_encrypt128_generic(const struct aes_encrypt_ctx *, void *, const void *);
static void aes_ecb_encrypt256_generic(const struct aes_encrypt_ctx *, void *, const void *);
static void aes_ecb_encrypt128_padlock(const struct aes_encrypt_ctx *, void *, const void *);
static void aes_ecb_encrypt256_padlock(const struct aes_encrypt_ctx *, void *, const void *);

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 219
/*
 * This code does not use any AVX feature, it only uses the new
 * v* opcodes, so the upper half of the register gets 0-ed,
 * and the CPU is not caught with lower/upper half merges
 */
static void aes_encrypt_key128_AVXAES(struct aes_encrypt_ctx *ctx, const void *in)
{
	size_t k;

	asm(
#ifdef HAVE_SUBSECTION
			".subsection 2\n\t"
#else
			"jmp	1f\n\t"
#endif
			".align 16\n"
			"_key_expansion_128_avx:\n\t"
			"vpshufd	$0xff, %%xmm1, %%xmm1\n\t"
			"vshufps	$0x10, %%xmm0, %%xmm4, %%xmm4\n\t"
			"vpxor	%%xmm4, %%xmm0, %%xmm0\n\t"
			"vshufps	$0x8c, %%xmm0, %%xmm4, %%xmm4\n\t"
			"vpxor	%%xmm4, %%xmm0, %%xmm0\n\t"
			"vpxor	%%xmm1, %%xmm0, %%xmm0\n\t"
			"vmovdqa	%%xmm0, (%"PTRP"0)\n\t"
			"add	$0x10, %0\n\t"
			"ret\n\t"
#ifdef HAVE_SUBSECTION
			".previous\n\t"
#else
			"1:\n\t"
#endif
			"vlddqu	%2, %%xmm0\n\t"	/* user key (first 16 bytes) */
			"vmovdqa	%%xmm0, (%"PTRP"0)\n\t"
			"add	$0x10, %0\n\t"	/* key addr */
			"vpxor	%%xmm4, %%xmm4, %%xmm4\n\t" /* init xmm4 */
			/* aeskeygenassist $0x1, %xmm0, %xmm1 */
			"vaeskeygenassist $0x1, %%xmm0, %%xmm1\n\t" /* round 1 */
			"call	_key_expansion_128_avx\n\t"
			"vaeskeygenassist $0x2, %%xmm0, %%xmm1\n\t" /* round 2 */
			"call	_key_expansion_128_avx\n\t"
			"vaeskeygenassist $0x4, %%xmm0, %%xmm1\n\t" /* round 3 */
			"call	_key_expansion_128_avx\n\t"
			"vaeskeygenassist $0x8, %%xmm0, %%xmm1\n\t" /* round 4 */
			"call	_key_expansion_128_avx\n\t"
			"vaeskeygenassist $0x10, %%xmm0, %%xmm1\n\t" /* round 5 */
			"call	_key_expansion_128_avx\n\t"
			"vaeskeygenassist $0x20, %%xmm0, %%xmm1\n\t" /* round 6 */
			"call	_key_expansion_128_avx\n\t"
			"vaeskeygenassist $0x40, %%xmm0, %%xmm1\n\t" /* round 7 */
			"call	_key_expansion_128_avx\n\t"
			"vaeskeygenassist $0x80, %%xmm0, %%xmm1\n\t" /* round 8 */
			"call	_key_expansion_128_avx\n\t"
			"vaeskeygenassist $0x1b, %%xmm0, %%xmm1\n\t" /* round 9 */
			"call	_key_expansion_128_avx\n\t"
			"vaeskeygenassist $0x36, %%xmm0, %%xmm1\n\t" /* round 10 */
			"call	_key_expansion_128_avx"
		: /* %0 */ "=r" (k),
		  /* %1 */ "=m" (*ctx->k)
		: /* %2 */ "m" (*(const char *)in),
		  /* %3 */ "0" (&ctx->k)
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm4"
#  endif
	);
}

static void aes_encrypt_key256_AVXAES(struct aes_encrypt_ctx *ctx, const void *in)
{
	size_t k;

	asm(
#ifdef HAVE_SUBSECTION
			".subsection 2\n\t"
#else
			"jmp	1f\n\t"
#endif
			".align 16\n"
			"_key_expansion_256a_avx:\n\t"
			"vpshufd	$0xff, %%xmm1, %%xmm1\n\t"
			"vshufps	$0x10, %%xmm0, %%xmm4, %%xmm4\n\t"
			"vpxor	%%xmm4, %%xmm0, %%xmm0\n\t"
			"vshufps	$0x8c, %%xmm0, %%xmm4, %%xmm4\n\t"
			"vpxor	%%xmm4, %%xmm0, %%xmm0\n\t"
			"vpxor	%%xmm1, %%xmm0, %%xmm0\n\t"
			"vmovdqa	%%xmm0, (%"PTRP"0)\n\t"
			"add	$0x10, %0\n\t"
			"ret\n\t"
			".align 4\n"
			"_key_expansion_256b_avx:\n\t"
			"vpshufd $0xAA, %%xmm1, %%xmm1\n\t"
			"vshufps $0x10, %%xmm2, %%xmm4, %%xmm4\n\t"
			"vpxor %%xmm4, %%xmm2, %%xmm2\n\t"
			"vshufps $0x8c, %%xmm2, %%xmm4, %%xmm4\n\t"
			"vpxor %%xmm4, %%xmm2, %%xmm2\n\t"
			"vpxor %%xmm1, %%xmm2, %%xmm2\n\t"
			"vmovdqa %%xmm2, (%"PTRP"0)\n\t"
			"add $0x10, %0\n\t"
			"ret\n\t"
#ifdef HAVE_SUBSECTION
			".previous\n\t"
#else
			"1:\n\t"
#endif
			"vlddqu	%2, %%xmm0\n\t"	/* user key (first 16 bytes) */
			"vlddqu	0x10 %2, %%xmm2\n\t"	/* user key (second 16 bytes) */
			"vmovdqa	%%xmm0, (%"PTRP"0)\n\t"
			"vmovdqa	%%xmm2, 0x10(%"PTRP"0)\n\t"
			"add	$0x20, %0\n\t"	/* key addr */
			"vpxor	%%xmm4, %%xmm4, %%xmm4\n\t" /* init xmm4 */
			"vaeskeygenassist $0x1, %%xmm2, %%xmm1\n\t" /* round 1 */
			"call	_key_expansion_256a_avx\n\t"
			"vaeskeygenassist $0x1, %%xmm0, %%xmm1\n\t"
			"call	_key_expansion_256b_avx\n\t"
			"vaeskeygenassist $0x2, %%xmm2, %%xmm1\n\t" /* round 2 */
			"call	_key_expansion_256a_avx\n\t"
			"vaeskeygenassist $0x2, %%xmm0, %%xmm1\n\t"
			"call	_key_expansion_256b_avx\n\t"
			"vaeskeygenassist $0x4, %%xmm2, %%xmm1\n\t" /* round 3 */
			"call	_key_expansion_256a_avx\n\t"
			"vaeskeygenassist $0x4, %%xmm0, %%xmm1\n\t"
			"call	_key_expansion_256b_avx\n\t"
			"vaeskeygenassist $0x8, %%xmm2, %%xmm1\n\t" /* round 4 */
			"call	_key_expansion_256a_avx\n\t"
			"vaeskeygenassist $0x8, %%xmm0, %%xmm1\n\t"
			"call	_key_expansion_256b_avx\n\t"
			"vaeskeygenassist $0x10, %%xmm2, %%xmm1\n\t" /* round 5 */
			"call	_key_expansion_256a_avx\n\t"
			"vaeskeygenassist $0x10, %%xmm0, %%xmm1\n\t"
			"call	_key_expansion_256b_avx\n\t"
			"vaeskeygenassist $0x20, %%xmm2, %%xmm1\n\t" /* round 6 */
			"call	_key_expansion_256a_avx\n\t"
			"vaeskeygenassist $0x20, %%xmm0, %%xmm1\n\t"
			"call	_key_expansion_256b_avx\n\t"
			"vaeskeygenassist $0x40, %%xmm2, %%xmm1\n\t" /* round 7 */
			"call	_key_expansion_256a_avx\n\t"
		: /* %0 */ "=r" (k),
		  /* %1 */ "=m" (*ctx->k)
		: /* %2 */ "m" (*(const char *)in),
		  /* %3 */ "0" (&ctx->k)
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm4"
#  endif
	);
}
# endif

# if HAVE_BINUTILS >= 217
static void aes_encrypt_key128_SSEAES(struct aes_encrypt_ctx *ctx, const void *in)
{
	size_t k;

	asm(
#ifdef HAVE_SUBSECTION
			".subsection 2\n\t"
#else
			"jmp	1f\n\t"
#endif
			".align 4\n"
			"_key_expansion_128:\n\t"
			"pshufd	$0xff, %%xmm1, %%xmm1\n\t"
			"shufps	$0x10, %%xmm0, %%xmm4\n\t"
			"pxor	%%xmm4, %%xmm0\n\t"
			"shufps	$0x8c, %%xmm0, %%xmm4\n\t"
			"pxor	%%xmm4, %%xmm0\n\t"
			"pxor	%%xmm1, %%xmm0\n\t"
			"movaps	%%xmm0, (%"PTRP"0)\n\t"
			"add	$0x10, %0\n\t"
			"ret\n\t"
#ifdef HAVE_SUBSECTION
			".previous\n\t"
#else
			"1:\n\t"
#endif
			"movups	%2, %%xmm0\n\t"	/* user key (first 16 bytes) */
			"movaps	%%xmm0, (%"PTRP"0)\n\t"
			"add	$0x10, %"PTRP"0\n\t" /* key addr */
			"pxor	%%xmm4, %%xmm4\n\t" /* init xmm4 */
			/* aeskeygenassist $0x1, %xmm0, %xmm1 */
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x01\n\t" /* round 1 */
			"call	_key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x02\n\t" /* round 2 */
			"call	_key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x04\n\t" /* round 3 */
			"call	_key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x08\n\t" /* round 4 */
			"call	_key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x10\n\t" /* round 5 */
			"call	_key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x20\n\t" /* round 6 */
			"call	_key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x40\n\t" /* round 7 */
			"call	_key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x80\n\t" /* round 8 */
			"call	_key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x1b\n\t" /* round 9 */
			"call	_key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x36\n\t" /* round 10 */
			"call	_key_expansion_128"
		: /* %0 */ "=r" (k),
		  /* %1 */ "=m" (*ctx->k)
		: /* %2 */ "m" (*(const char *)in),
		  /* %3 */ "0" (ctx->k)
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm4"
#  endif
	);
}

static void aes_encrypt_key256_SSEAES(struct aes_encrypt_ctx *ctx, const void *in)
{
	size_t k;

	asm(
#ifdef HAVE_SUBSECTION
			".subsection 2\n\t"
#else
			"jmp	1f\n\t"
#endif
			".align 4\n"
			"_key_expansion_256a:\n\t"
			"pshufd	$0xff, %%xmm1, %%xmm1\n\t"
			"shufps	$0x10, %%xmm0, %%xmm4\n\t"
			"pxor	%%xmm4, %%xmm0\n\t"
			"shufps	$0x8c, %%xmm0, %%xmm4\n\t"
			"pxor	%%xmm4, %%xmm0\n\t"
			"pxor	%%xmm1, %%xmm0\n\t"
			"movaps	%%xmm0, (%"PTRP"0)\n\t"
			"add	$0x10, %0\n\t"
			"ret\n\t"
			".align 4\n"
			"_key_expansion_256b:\n\t"
			"pshufd $0xAA, %%xmm1, %%xmm1\n\t"
			"shufps $0x10, %%xmm2, %%xmm4\n\t"
			"pxor %%xmm4, %%xmm2\n\t"
			"shufps $0x8c, %%xmm2, %%xmm4\n\t"
			"pxor %%xmm4, %%xmm2\n\t"
			"pxor %%xmm1, %%xmm2\n\t"
			"movaps %%xmm2, (%"PTRP"0)\n\t"
			"add $0x10, %0\n\t"
			"ret\n\t"
#ifdef HAVE_SUBSECTION
			".previous\n\t"
#else
			"1:\n\t"
#endif
			"movups	%2, %%xmm0\n\t"	/* user key (first 16 bytes) */
			"movups	0x10 %2, %%xmm2\n\t"	/* user key (second 16 bytes) */
			"movaps	%%xmm0, (%"PTRP"0)\n\t"
			"movaps	%%xmm2, 0x10(%"PTRP"0)\n\t"
			"add	$0x20, %"PTRP"0\n\t" /* key addr */
			"pxor	%%xmm4, %%xmm4\n\t" /* init xmm4 */
			/* aeskeygenassist $0x1, %xmm2, %xmm1 */
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xca, 0x01\n\t" /* round 1 */
			"call	_key_expansion_256a\n\t"
			/* aeskeygenassist $0x1, %xmm0, %xmm1 */
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x01\n\t"
			"call	_key_expansion_256b\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xca, 0x02\n\t" /* round 2 */
			"call	_key_expansion_256a\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x02\n\t"
			"call	_key_expansion_256b\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xca, 0x04\n\t" /* round 3 */
			"call	_key_expansion_256a\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x04\n\t"
			"call	_key_expansion_256b\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xca, 0x08\n\t" /* round 4 */
			"call	_key_expansion_256a\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x08\n\t"
			"call	_key_expansion_256b\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xca, 0x10\n\t" /* round 5 */
			"call	_key_expansion_256a\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x10\n\t"
			"call	_key_expansion_256b\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xca, 0x20\n\t" /* round 6 */
			"call	_key_expansion_256a\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x20\n\t"
			"call	_key_expansion_256b\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xca, 0x40\n\t" /* round 7 */
			"call	_key_expansion_256a\n\t"
		: /* %0 */ "=r" (k),
		  /* %1 */ "=m" (*ctx->k)
		: /* %2 */ "m" (*(const char *)in),
		  /* %3 */ "0" (ctx->k)
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm4"
#  endif
	);
}

/*
 * original SSSE3 code by:
 * By Mike Hamburg (Stanford University), 2009
 * Public domain
 *
 * adapted by me
 */
#  define S0F	-128
#  define S63	-112
#  define SBOLO	 -96
#  define SBOHI	 -80
#  define INVLO	 -64
#  define INVHI	 -48
#  define SB1LO	 -32
#  define SB1HI	 -16
#  define SB2LO	   0
#  define AES_CONSTS_0MEMBER aes_consts.sb2lo
#  define SB2HI	  16
#  define MCF	  32
#  define MCB	  96
#  define SR	160
#  define IPTLO	224
#  define IPTHI	240
#  define RCON	256
#  define OPTLO	272
#  define OPTHI	288


static const struct
{
	uint64_t s0f[2];
	uint64_t s63[2];
	uint64_t iptlo[2];
	uint64_t ipthi[2];
	uint64_t sbolo[2];
	uint64_t sbohi[2];
	uint64_t invlo[2];
	uint64_t invhi[2];
	uint64_t sb1lo[2];
	uint64_t sb1hi[2];
	uint64_t sb2lo[2];
	uint64_t sb2hi[2];
	uint64_t mcf[8];
	uint64_t mcb[8];
	uint64_t sr[8];
	uint64_t rcon[2];
	uint64_t optlo[2];
	uint64_t opthi[2];
} aes_consts GCC_ATTR_ALIGNED(16) =
{
	.s0f   = { 0x0F0F0F0F0F0F0F0FULL, 0x0F0F0F0F0F0F0F0FULL },
	.s63   = { 0x5B5B5B5B5B5B5B5BULL, 0x5B5B5B5B5B5B5B5BULL },
	.sbolo = { 0xD0D26D176FBDC700ULL, 0x15AABF7AC502A878ULL },
	.sbohi = { 0xCFE474A55FBB6A00ULL, 0x8E1E90D1412B35FAULL },
	.invlo = { 0x0E05060F0D080180ULL, 0x040703090A0B0C02ULL },
	.invhi = { 0x01040A060F0B0780ULL, 0x030D0E0C02050809ULL },
	.sb1lo = { 0xB19BE18FCB503E00ULL, 0xA5DF7A6E142AF544ULL },
	.sb1hi = { 0x3618D415FAE22300ULL, 0x3BF7CCC10D2ED9EFULL },
	.sb2lo = { 0xE27A93C60B712400ULL, 0x5EB7E955BC982FCDULL },
	.sb2hi = { 0x69EB88400AE12900ULL, 0xC2A163C8AB82234AULL },
	.mcf   = { 0x0407060500030201ULL, 0x0C0F0E0D080B0A09ULL,
	           0x080B0A0904070605ULL, 0x000302010C0F0E0DULL,
	           0x0C0F0E0D080B0A09ULL, 0x0407060500030201ULL,
	           0x000302010C0F0E0DULL, 0x080B0A0904070605ULL },
	.mcb   = { 0x0605040702010003ULL, 0x0E0D0C0F0A09080BULL,
	           0x020100030E0D0C0FULL, 0x0A09080B06050407ULL,
	           0x0E0D0C0F0A09080BULL, 0x0605040702010003ULL,
	           0x0A09080B06050407ULL, 0x020100030E0D0C0FULL },
	.sr    = { 0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL,
	           0x030E09040F0A0500ULL, 0x0B06010C07020D08ULL,
	           0x0F060D040B020900ULL, 0x070E050C030A0108ULL,
	           0x0B0E0104070A0D00ULL, 0x0306090C0F020508ULL },
	.iptlo = { 0xC2B2E8985A2A7000ULL, 0xCABAE09052227808ULL },
	.ipthi = { 0x4C01307D317C4D00ULL, 0xCD80B1FCB0FDCC81ULL },
	.rcon  = { 0x1F8391B9AF9DEEB6ULL, 0x702A98084D7C7D81ULL },
	.optlo = { 0xFF9F4929D6B66000ULL, 0xF7974121DEBE6808ULL },
	.opthi = { 0x01EDBD5150BCEC00ULL, 0xE10D5DB1B05C0CE0ULL },
};

#  if HAVE_BINUTILS >= 219
/*
 * This code does not use any AVX feature, it only uses the new
 * v* opcodes, so the upper half of the register gets 0-ed,
 * and the CPU is not caught with lower/upper half merges
 */
#   ifndef __i386__
#    define AVX_M2OP(op, src1, src2, dest) op "	" src1 ", " src2 ", " dest "\n\t"
#    define AVX_S0F "%%xmm9"
#    define AVX_INVHI "%%xmm11"
#    define AVX_S63 "%%xmm12"
#    define AVX_SB1LO "%%xmm13"
#    define AVX_SB1HI "%%xmm14"
#    define AVX_MCF "%%xmm15"
#   else
#    define AVX_M2OP(op, src1, src2, dest) "vmovdqa " src2 ", " dest "\n\t" op " " src1 ", " dest ", " dest "\n\t"
#    define AVX_INVHI str_it(INVHI)"(%"PTRP"4)"
#    define AVX_S0F str_it(S0F)"(%"PTRP"4)"
#    define AVX_S63 str_it(S63)"(%"PTRP"4)"
#    define AVX_SB1LO str_it(SB1LO)"(%"PTRP"4)"
#    define AVX_SB1HI str_it(SB1HI)"(%"PTRP"4)"
#    define AVX_MCF "%%xmm1"
#   endif
static void aes_encrypt_key128_AVX(struct aes_encrypt_ctx *ctx, const void *in)
{
	size_t rounds, n;
	void *k;

	prefetch(in);
	prefetchw(ctx->k);
	prefetch(&aes_consts);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	asm volatile (
			"vlddqu	    %5, %%xmm0\n\t"	/* load key */
			"vmovdqa	"str_it(INVLO)"(%"PTRP"4), %%xmm5\n\t"	/* inv */
#   ifndef __i386__
			"vmovdqa	"str_it(S0F)"(%"PTRP"4), "AVX_S0F"\n\t"	/* 0F */
			"vmovdqa	"str_it(INVHI)"(%"PTRP"4), "AVX_INVHI"\n\t"	/* inva */
			"vmovdqa	"str_it(S63)"(%"PTRP"4), "AVX_S63"\n\t"	/* S63 */
			"vmovdqa	"str_it(SB1LO)"(%"PTRP"4), "AVX_SB1LO"\n\t"	/* inva */
			"vmovdqa	"str_it(SB1HI)"(%"PTRP"4), "AVX_SB1HI"\n\t"	/* inva */
			"vmovdqa	"str_it(MCF)"(%"PTRP"4), "AVX_MCF"\n\t"	/* inva */
#   endif
			"vmovdqa	"str_it(RCON)"(%"PTRP"4), %%xmm6\n\t"
			/* input transform */
			"vmovdqa	%%xmm0, %%xmm3\n\t"
			/* shedule transform */
			AVX_M2OP("vpandn", "%%xmm0", AVX_S0F, "%%xmm1")
			"vpsrld	    $4, %%xmm1, %%xmm1\n\t"
			"vpand	"AVX_S0F", %%xmm0, %%xmm0\n\t"
			"vmovdqa	"str_it(IPTLO)"(%"PTRP"4), %%xmm2\n\t"	/* iptlo */
			"vpshufb	%%xmm0, %%xmm2, %%xmm2\n\t"
			"vmovdqa	"str_it(IPTHI)"(%"PTRP"4), %%xmm0\n\t"	/* ipthi */
			"vpshufb	%%xmm1, %%xmm0, %%xmm0\n\t"
			"vpxor	%%xmm2, %%xmm0, %%xmm0\n\t"
			"vmovdqa	%%xmm0, %%xmm7\n\t"
			/* output zeroth round key */
			"vmovdqa	%%xmm0, (%"PTRP"2)\n\t"
			"jmp	1f\n\t"
			".p2align 3\n"
			"1:\n\t"
			/* extract rcon from xmm8 */
			"vpxor	%%xmm1, %%xmm1, %%xmm1\n\t"
			"vpalignr	$15, %%xmm6, %%xmm1, %%xmm1\n\t"
			"vpalignr	$15, %%xmm6, %%xmm6, %%xmm6\n\t"
			"vpxor	%%xmm1, %%xmm7, %%xmm1\n\t"
			/* rotate */
			"vpshufd	$0xFF, %%xmm0, %%xmm0\n\t"
			"vpalignr	$1, %%xmm0, %%xmm0, %%xmm0\n\t"
			/* smear xmm7 */
			"vpslldq	     $4, %%xmm1, %%xmm7\n\t"
			"vpxor	 %%xmm1, %%xmm7, %%xmm7\n\t"
			"vpslldq	     $8, %%xmm7, %%xmm1\n\t"
			"vpxor	 %%xmm1, %%xmm7, %%xmm7\n\t"
			"vpxor	"AVX_S63", %%xmm7, %%xmm7\n\t"
			/* subbytes */
			AVX_M2OP("vpandn", "%%xmm0", AVX_S0F, "%%xmm1")
			"vpsrld	     $4, %%xmm1, %%xmm1\n\t"	/* 1 = i */
			"vpand	"AVX_S0F", %%xmm0, %%xmm0\n\t"	/* 0 = k */
			AVX_M2OP("vpshufb", "%%xmm0", AVX_INVHI, "%%xmm2")
			"vpxor	 %%xmm1, %%xmm0, %%xmm0\n\t"	/* 0 = j */
			"vpshufb	 %%xmm1, %%xmm5, %%xmm3\n\t"	/* 3 = 1/i */
			"vpxor	 %%xmm2, %%xmm3, %%xmm3\n\t"	/* 3 = iak = 1/i + a/k */
			"vpshufb	 %%xmm0, %%xmm5, %%xmm4\n\t"	/* 4 = 1/j */
			"vpxor	 %%xmm2, %%xmm4, %%xmm4\n\t"	/* 4 = jak = 1/j + a/k */
			"vpshufb	 %%xmm3, %%xmm5, %%xmm2\n\t"	/* 2 = 1/iak */
			"vpxor	 %%xmm0, %%xmm2, %%xmm2\n\t"	/* 2 = io */
			"vpshufb	 %%xmm4, %%xmm5, %%xmm3\n\t"	/* 3 = 1/jak */
			"vpxor	 %%xmm1, %%xmm3, %%xmm3\n\t"	/* 3 = jo */
			AVX_M2OP("vpshufb", "%%xmm2", AVX_SB1LO, "%%xmm4")		/* 4 = sbou */
			AVX_M2OP("vpshufb", "%%xmm3", AVX_SB1HI, "%%xmm0")		/* 0 = sb1t */
			"vpxor	%%xmm4, %%xmm0, %%xmm0\n\t"	/* 0 = sbox output */
			/* add in smeared stuff */
			"vpxor	%%xmm7, %%xmm0, %%xmm0\n\t"
			"dec	%0\n\t"
			"jz 	2f\n\t"
			"vmovdqa	%%xmm0, %%xmm7\n\t"
			/* write output */
#   ifdef __i386__
			"vmovdqa	"str_it(MCF)"(%"PTRP"4), "AVX_MCF"\n\t"
#   endif
			"add	$16, %2\n\t"
			"vpxor	"AVX_S63", %%xmm0, %%xmm2\n\t"	/* save xmm0 for later */
			"vpshufb	"AVX_MCF", %%xmm2, %%xmm3\n\t"
			"vpshufb	"AVX_MCF", %%xmm3, %%xmm2\n\t"
			"vpxor	%%xmm2, %%xmm3, %%xmm3\n\t"
			"vpshufb	"AVX_MCF", %%xmm2, %%xmm2\n\t"
			"vpxor	%%xmm2, %%xmm3, %%xmm3\n\t"
			"vpshufb	"str_it(SR)"(%"PTRP"1,%"PTRP"4), %%xmm3, %%xmm3\n\t"
			"add	  $-16, %1\n\t"
			"and	   $48, %1\n\t"
			"vmovdqa	%%xmm3, (%"PTRP"2)\n\t"
			"jmp 	1b\n"
			"2:\n\t"
			/* schedule last round key from xmm0 */
			"vpshufb	"str_it(SR)"(%"PTRP"1,%"PTRP"4), %%xmm0, %%xmm0\n\t"	/* output permute */
			"add	$16, %2\n\t"
			"vpxor	"AVX_S63", %%xmm0, %%xmm0\n\t"
			AVX_M2OP("vpandn", "%%xmm0", AVX_S0F, "%%xmm1")
			"vpsrld	    $4, %%xmm1, %%xmm1\n\t"
			"vpand	"AVX_S0F", %%xmm0, %%xmm0\n\t"	/* 0 = k */
			"vmovdqa	"str_it(OPTLO)"(%"PTRP"4), %%xmm2\n\t"	/* optlo */
			"vpshufb	%%xmm0, %%xmm2, %%xmm2\n\t"
			"vmovdqa	"str_it(OPTHI)"(%"PTRP"4), %%xmm0\n\t"	/* opthi */
			"vpshufb	%%xmm1, %%xmm0, %%xmm0\n\t"
			"vpxor	%%xmm0, %%xmm2, %%xmm0\n\t"
			"vmovdqa	%%xmm0, (%"PTRP"2)\n\t"	/* save last key */
		: /* %0 */ "=&r" (rounds),
		  /* %1 */ "=&r" (n),
		  /* %2 */ "=&r" (k),
		  /* %3 */ "=m" (ctx->k[0])
		: /* %4 */ "r" (AES_CONSTS_0MEMBER),
		  /* %5 */ "m" (*(const int *)in),
		  /* %6 */ "0" (10),
		  /* %7 */ "1" (16),
		  /* %8 */ "2" (&ctx->k[0])
#   ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#    ifndef __i386__
		  , "xmm9", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
#    endif
#   endif
	);
}

#   ifndef __i386__
#    define AVX_INVLO "%%xmm8"
#   else
#    define AVX_INVLO str_it(INVLO)"(%"PTRP"4)"
#   endif

static void aes_encrypt_key256_AVX(struct aes_encrypt_ctx *ctx, const void *in)
{
	size_t rounds, n;
	void *k;

	prefetch(in);
	prefetchw(ctx->k);
	prefetch(&aes_consts);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	asm volatile (
			"vlddqu	    %5, %%xmm0\n\t"	/* load key */
#   ifndef __i386__
			"vmovdqa	"str_it(INVLO)"(%"PTRP"4), "AVX_INVLO"\n\t"	/* inv */
			"vmovdqa	"str_it(S0F)"(%"PTRP"4), "AVX_S0F"\n\t"	/* 0F */
			"vmovdqa	"str_it(INVHI)"(%"PTRP"4), "AVX_INVHI"\n\t"	/* inva */
			"vmovdqa	"str_it(S63)"(%"PTRP"4), "AVX_S63"\n\t"	/* S63 */
			"vmovdqa	"str_it(SB1LO)"(%"PTRP"4), "AVX_SB1LO"\n\t"	/* inva */
			"vmovdqa	"str_it(SB1HI)"(%"PTRP"4), "AVX_SB1HI"\n\t"	/* inva */
			"vmovdqa	"str_it(MCF)"(%"PTRP"4), "AVX_MCF"\n\t"	/* inva */
#   endif
			"vmovdqa	"str_it(RCON)"(%"PTRP"4), %%xmm6\n\t"
			/* input transform */
			"vmovdqa	%%xmm0, %%xmm3\n\t"
			/* shedule transform */
			AVX_M2OP("vpandn", "%%xmm0", AVX_S0F, "%%xmm1")
			"vpsrld	    $4, %%xmm1, %%xmm1\n\t"
			"vpand	"AVX_S0F", %%xmm0, %%xmm0\n\t"
			"vmovdqa	"str_it(IPTLO)"(%"PTRP"4), %%xmm2\n\t"	/* iptlo */
			"vpshufb	%%xmm0, %%xmm2, %%xmm2\n\t"
			"vmovdqa	"str_it(IPTHI)"(%"PTRP"4), %%xmm0\n\t"	/* ipthi */
			"vpshufb	%%xmm1, %%xmm0, %%xmm0\n\t"
			"vpxor	%%xmm2, %%xmm0, %%xmm0\n\t"
			"vmovdqa	%%xmm0, %%xmm7\n\t"
			/* output zeroth round key */
			"vmovdqa	%%xmm0, (%"PTRP"2)\n\t"
			"jmp	1f\n\t"
			".p2align 3\n"
			"1:\n\t"
			/* extract rcon from xmm8 */
			"vpxor	%%xmm1, %%xmm1, %%xmm1\n\t"
			"vpalignr	$15, %%xmm6, %%xmm1, %%xmm1\n\t"
			"vpalignr	$15, %%xmm6, %%xmm6, %%xmm6\n\t"
			"vpxor	%%xmm1, %%xmm7, %%xmm1\n\t"
			/* rotate */
			"vpshufd	$0xFF, %%xmm0, %%xmm0\n\t"
			"vpalignr	$1, %%xmm0, %%xmm0, %%xmm0\n\t"
			/* smear xmm7 */
			"vpslldq	     $4, %%xmm1, %%xmm7\n\t"
			"vpxor	 %%xmm1, %%xmm7, %%xmm7\n\t"
			"vpslldq	     $8, %%xmm7, %%xmm1\n\t"
			"vpxor	 %%xmm1, %%xmm7, %%xmm7\n\t"
			"vpxor	"AVX_S63", %%xmm7, %%xmm7\n\t"
			/* subbytes */
			AVX_M2OP("vpandn", "%%xmm0", AVX_S0F, "%%xmm1")
			"vpsrld	     $4, %%xmm1, %%xmm1\n\t"	/* 1 = i */
			"vpand	"AVX_S0F", %%xmm0, %%xmm0\n\t"	/* 0 = k */
			AVX_M2OP("vpshufb", "%%xmm0", AVX_INVHI, "%%xmm2")
			"vpxor	 %%xmm1, %%xmm0, %%xmm0\n\t"	/* 0 = j */
			AVX_M2OP("vpshufb", "%%xmm1", AVX_INVLO, "%%xmm3")	/* 3 = 1/i */
			"vpxor	 %%xmm2, %%xmm3, %%xmm3\n\t"	/* 3 = iak = 1/i + a/k */
			AVX_M2OP("vpshufb", "%%xmm0", AVX_INVLO, "%%xmm4")	/* 4 = 1/j */
			"vpxor	 %%xmm2, %%xmm4, %%xmm4\n\t"	/* 4 = jak = 1/j + a/k */
			AVX_M2OP("vpshufb", "%%xmm3", AVX_INVLO, "%%xmm2")	/* 2 = 1/iak */
			"vpxor	 %%xmm0, %%xmm2, %%xmm2\n\t"	/* 2 = io */
			AVX_M2OP("vpshufb", "%%xmm4", AVX_INVLO, "%%xmm3")	/* 3 = 1/jak */
			"vpxor	 %%xmm1, %%xmm3, %%xmm3\n\t"	/* 3 = jo */
			AVX_M2OP("vpshufb", "%%xmm2", AVX_SB1LO, "%%xmm4")		/* 4 = sbou */
			AVX_M2OP("vpshufb", "%%xmm3", AVX_SB1HI, "%%xmm0")		/* 0 = sb1t */
			"vpxor	%%xmm4, %%xmm0, %%xmm0\n\t"	/* 0 = sbox output */
			/* add in smeared stuff */
			"vpxor	%%xmm7, %%xmm0, %%xmm0\n\t"
			"dec	%0\n\t"
			"jz 	2f\n\t"
			"vmovdqa	%%xmm0, %%xmm7\n\t"
			/* write output */
#   ifdef __i386__
			"vmovdqa	"str_it(MCF)"(%"PTRP"4), "AVX_MCF"\n\t"
#   endif
			"add	$16, %2\n\t"
			"vpxor	"AVX_S63", %%xmm0, %%xmm2\n\t"	/* save xmm0 for later */
			"vpshufb	"AVX_MCF", %%xmm2, %%xmm3\n\t"
			"vpshufb	"AVX_MCF", %%xmm3, %%xmm2\n\t"
			"vpxor	%%xmm2, %%xmm3, %%xmm3\n\t"
			"vpshufb	"AVX_MCF", %%xmm2, %%xmm2\n\t"
			"vpxor	%%xmm2, %%xmm3, %%xmm3\n\t"
			"vpshufb	"str_it(SR)"(%"PTRP"1,%"PTRP"4), %%xmm3, %%xmm3\n\t"
			"add	  $-16, %1\n\t"
			"and	   $48, %1\n\t"
			"vmovdqa	%%xmm3, (%"PTRP"2)\n\t"
			"jmp 	1b\n"
			"2:\n\t"
			/* schedule last round key from xmm0 */
			"vpshufb	"str_it(SR)"(%"PTRP"1,%"PTRP"4), %%xmm0, %%xmm0\n\t"	/* output permute */
			"add	$16, %2\n\t"
			"vpxor	"AVX_S63", %%xmm0, %%xmm0\n\t"
			AVX_M2OP("vpandn", "%%xmm0", AVX_S0F, "%%xmm1")
			"vpsrld	    $4, %%xmm1, %%xmm1\n\t"
			"vpand	"AVX_S0F", %%xmm0, %%xmm0\n\t"	/* 0 = k */
			"vmovdqa	"str_it(OPTLO)"(%"PTRP"4), %%xmm2\n\t"	/* optlo */
			"vpshufb	%%xmm0, %%xmm2, %%xmm2\n\t"
			"vmovdqa	"str_it(OPTHI)"(%"PTRP"4), %%xmm0\n\t"	/* opthi */
			"vpshufb	%%xmm1, %%xmm0, %%xmm0\n\t"
			"vpxor	%%xmm0, %%xmm2, %%xmm0\n\t"
			"vmovdqa	%%xmm0, (%"PTRP"2)\n\t"	/* save last key */
		: /* %0 */ "=&r" (rounds),
		  /* %1 */ "=&r" (n),
		  /* %2 */ "=&r" (k),
		  /* %3 */ "=m" (ctx->k[0])
		: /* %4 */ "r" (AES_CONSTS_0MEMBER),
		  /* %5 */ "m" (*(const int *)in),
		  /* %6 */ "0" (10),
		  /* %7 */ "1" (16),
		  /* %8 */ "2" (&ctx->k[0])
#   ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#    ifndef __i386__
		  , "xmm9", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
#    endif
#   endif
	);
}
#   undef AVX_S0F
#   undef AVX_INVHI
#   undef AVX_S63
#   undef AVX_SB1LO
#   undef AVX_SB1HI
#   undef AVX_MCF
#  endif

#  ifndef __i386__
#   define SSSE3_S0F "%%xmm9"
#   define SSSE3_INVHI "%%xmm11"
#   define SSSE3_S63 "%%xmm12"
#   define SSSE3_SB1LO "%%xmm13"
#   define SSSE3_SB1HI "%%xmm14"
#   define SSSE3_MCF "%%xmm15"
#  else
#   define SSSE3_S0F str_it(S0F)"(%"PTRP"4)"
#   define SSSE3_INVHI str_it(INVHI)"(%"PTRP"4)"
#   define SSSE3_S63 str_it(S63)"(%"PTRP"4)"
#   define SSSE3_SB1LO str_it(SB1LO)"(%"PTRP"4)"
#   define SSSE3_SB1HI str_it(SB1HI)"(%"PTRP"4)"
#   define SSSE3_MCF "%%xmm1"
#  endif
static void aes_encrypt_key128_SSSE3(struct aes_encrypt_ctx *ctx, const void *in)
{
	size_t rounds, n;
	void *k;

	prefetch(in);
	prefetchw(ctx->k);
	prefetch(&aes_consts);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	asm volatile (
			"lddqu	    %5, %%xmm0\n\t"	/* load key */
			"movdqa	"str_it(INVLO)"(%"PTRP"4), %%xmm5\n\t"	/* inv */
#  ifndef __i386__
			"movdqa	"str_it(S0F)"(%"PTRP"4), "SSSE3_S0F"\n\t"	/* 0F */
			"movdqa	"str_it(INVHI)"(%"PTRP"4), "SSSE3_INVHI"\n\t"	/* inva */
			"movdqa	"str_it(S63)"(%"PTRP"4), "SSSE3_S63"\n\t"	/* S63 */
			"movdqa	"str_it(SB1LO)"(%"PTRP"4), "SSSE3_SB1LO"\n\t"	/* SB1LO */
			"movdqa	"str_it(SB1HI)"(%"PTRP"4), "SSSE3_SB1HI"\n\t"	/* SB1HI */
			"movdqa	"str_it(MCF)"(%"PTRP"4), "SSSE3_MCF"\n\t"	/* MCF */
#  endif
			"movdqa	"str_it(RCON)"(%"PTRP"4), %%xmm6\n\t"
			/* input transform */
			"movdqa	%%xmm0, %%xmm3\n\t"
			/* shedule transform */
			"movdqa	"SSSE3_S0F", %%xmm1\n\t"
			"pandn	%%xmm0, %%xmm1\n\t"
			"psrld	    $4, %%xmm1\n\t"
			"pand	"SSSE3_S0F", %%xmm0\n\t"
			"movdqa	"str_it(IPTLO)"(%"PTRP"4), %%xmm2\n\t"	/* iptlo */
			"pshufb	%%xmm0, %%xmm2\n\t"
			"movdqa	"str_it(IPTHI)"(%"PTRP"4), %%xmm0\n\t"	/* ipthi */
			"pshufb	%%xmm1, %%xmm0\n\t"
			"pxor	%%xmm2, %%xmm0\n\t"
			"movdqa	%%xmm0, %%xmm7\n\t"
			/* output zeroth round key */
			"movdqa	%%xmm0, (%"PTRP"2)\n\t"
			"jmp	1f\n\t"
			".p2align 3\n"
			"1:\n\t"
			/* extract rcon from xmm8 */
			"pxor	%%xmm1, %%xmm1\n\t"
			"palignr	$15, %%xmm6, %%xmm1\n\t"
			"palignr	$15, %%xmm6, %%xmm6\n\t"
			"pxor	%%xmm1, %%xmm7\n\t"
			/* rotate */
			"pshufd	$0xFF, %%xmm0, %%xmm0\n\t"
			"palignr	$1, %%xmm0, %%xmm0\n\t"
			/* smear xmm7 */
			"movdqa	 %%xmm7, %%xmm1\n\t"
			"pslldq	     $4, %%xmm7\n\t"
			"pxor	 %%xmm1, %%xmm7\n\t"
			"movdqa	 %%xmm7, %%xmm1\n\t"
			"pslldq	     $8, %%xmm7\n\t"
			"pxor	 %%xmm1, %%xmm7\n\t"
			"pxor	"SSSE3_S63", %%xmm7\n\t"
			/* subbytes */
			"movdqa	"SSSE3_S0F", %%xmm1\n\t"
			"pandn	 %%xmm0, %%xmm1\n\t"
			"psrld	     $4, %%xmm1\n\t"	/* 1 = i */
			"pand	"SSSE3_S0F", %%xmm0\n\t"	/* 0 = k */
			"movdqa	"SSSE3_INVHI", %%xmm2\n\t"	/* 2 : a/k */
			"pshufb	 %%xmm0, %%xmm2\n\t"	/* 2 = a/k */
			"pxor	 %%xmm1, %%xmm0\n\t"	/* 0 = j */
			"movdqa	 %%xmm5, %%xmm3\n\t"	/* 3 : 1/i */
			"pshufb	 %%xmm1, %%xmm3\n\t"	/* 3 = 1/i */
			"pxor	 %%xmm2, %%xmm3\n\t"	/* 3 = iak = 1/i + a/k */
			"movdqa	 %%xmm5, %%xmm4\n\t"	/* 4 : 1/j */
			"pshufb	 %%xmm0, %%xmm4\n\t"	/* 4 = 1/j */
			"pxor	 %%xmm2, %%xmm4\n\t"	/* 4 = jak = 1/j + a/k */
			"movdqa	 %%xmm5, %%xmm2\n\t"	/* 2 : 1/iak */
			"pshufb	 %%xmm3, %%xmm2\n\t"	/* 2 = 1/iak */
			"pxor	 %%xmm0, %%xmm2\n\t"	/* 2 = io */
			"movdqa	 %%xmm5, %%xmm3\n\t"	/* 3 : 1/jak */
			"pshufb	 %%xmm4, %%xmm3\n\t"	/* 3 = 1/jak */
			"pxor	 %%xmm1, %%xmm3\n\t"	/* 3 = jo */
			"movdqa	"SSSE3_SB1LO", %%xmm4\n\t"	/* 4 : sbou */
			"pshufb  %%xmm2,  %%xmm4\n\t"		/* 4 = sbou */
			"movdqa	"SSSE3_SB1LO", %%xmm0\n\t"	/* 0 : sbot */
			"pshufb  %%xmm3, %%xmm0\n\t"	/* 0 = sb1t */
			"pxor	%%xmm4, %%xmm0\n\t"	/* 0 = sbox output */
			/* add in smeared stuff */
			"pxor	%%xmm7, %%xmm0\n\t"
			"dec	%0\n\t"
			"jz 	2f\n\t"
			"movdqa	%%xmm0, %%xmm7\n\t"
			/* write output */
			"movdqa	%%xmm0, %%xmm2\n\t"	/* save xmm0 for later */
#  ifdef __i386__
			"movdqa	"str_it(MCF)"(%"PTRP"4), "SSSE3_MCF"\n\t"
#  endif
			"add	$16, %2\n\t"
			"pxor	"SSSE3_S63", %%xmm2\n\t"
			"pshufb	"SSSE3_MCF", %%xmm2\n\t"
			"movdqa	%%xmm2, %%xmm3\n\t"
			"pshufb	"SSSE3_MCF", %%xmm2\n\t"
			"pxor	%%xmm2, %%xmm3\n\t"
			"pshufb	"SSSE3_MCF", %%xmm2\n\t"
			"pxor	%%xmm2, %%xmm3\n\t"
			"pshufb	"str_it(SR)"(%"PTRP"1,%"PTRP"4),%%xmm3\n\t"
			"add	  $-16, %1\n\t"
			"and	   $48, %1\n\t"
			"movdqa	%%xmm3, (%"PTRP"2)\n\t"
			"jmp 	1b\n"
			"2:\n\t"
			/* schedule last round key from xmm0 */
			"pshufb	"str_it(SR)"(%"PTRP"1,%"PTRP"4), %%xmm0\n\t"	/* output permute */
			"add	$16, %2\n\t"
			"pxor	"SSSE3_S63", %%xmm0\n\t"
			"movdqa	"SSSE3_S0F", %%xmm1\n\t"
			"pandn	%%xmm0, %%xmm1\n\t"
			"psrld	    $4, %%xmm1\n\t"
			"pand	"SSSE3_S0F", %%xmm0\n\t"	/* 0 = k */
			"movdqa	"str_it(OPTLO)"(%"PTRP"4), %%xmm2\n\t"	/* optlo */
			"pshufb	%%xmm0, %%xmm2\n\t"
			"movdqa	"str_it(OPTHI)"(%"PTRP"4), %%xmm0\n\t"	/* opthi */
			"pshufb	%%xmm1, %%xmm0\n\t"
			"pxor	%%xmm2, %%xmm0\n\t"
			"movdqa	%%xmm0, (%"PTRP"2)\n\t"	/* save last key */
		: /* %0 */ "=&r" (rounds),
		  /* %1 */ "=&r" (n),
		  /* %2 */ "=&r" (k),
		  /* %3 */ "=m" (ctx->k[0])
		: /* %4 */ "r" (AES_CONSTS_0MEMBER),
		  /* %5 */ "m" (*(const int *)in),
		  /* %6 */ "0" (10),
		  /* %7 */ "1" (16),
		  /* %8 */ "2" (&ctx->k[0])
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#   ifndef __i386__
		  , "xmm9", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
#   endif
#  endif
	);
}

#  ifndef __i386__
#   define SSSE3_INVLO "%%xmm8"
#  else
#   define SSSE3_INVLO str_it(INVLO)"(%"PTRP"4)"
#  endif
static void aes_encrypt_key256_SSSE3(struct aes_encrypt_ctx *ctx, const void *in)
{
	size_t rounds, n;
	void *k;

	prefetch(in);
	prefetchw(ctx->k);
	prefetch(&aes_consts);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	asm volatile (
			"lddqu	     %5, %%xmm0\n\t"	/* load key */
#  ifndef __i386__
			"movdqa	"str_it(INVLO)"(%"PTRP"4), "SSSE3_INVLO"\n\t"	/* inv */
			"movdqa	"str_it(S0F)"(%"PTRP"4), "SSSE3_S0F"\n\t"	/* 0F */
			"movdqa	"str_it(INVHI)"(%"PTRP"4), "SSSE3_INVHI"\n\t"	/* inva */
			"movdqa	"str_it(S63)"(%"PTRP"4), "SSSE3_S63"\n\t"	/* S63 */
			"movdqa	"str_it(SB1LO)"(%"PTRP"4), "SSSE3_SB1LO"\n\t"	/* SB1LO */
			"movdqa	"str_it(SB1HI)"(%"PTRP"4), "SSSE3_SB1HI"\n\t"	/* SB1HI */
			"movdqa	"str_it(MCF)"(%"PTRP"4), "SSSE3_MCF"\n\t"	/* MCF */
#  endif
			"movdqa	"str_it(RCON)"(%"PTRP"4), %%xmm6\n\t"
			/* input transform */
			"movdqa	%%xmm0, %%xmm3\n\t"
			/* shedule transform */
			"movdqa	"SSSE3_S0F", %%xmm1\n\t"
			"pandn	%%xmm0, %%xmm1\n\t"
			"psrld	    $4, %%xmm1\n\t"
			"pand	"SSSE3_S0F", %%xmm0\n\t"
			"movdqa	"str_it(IPTLO)"(%"PTRP"4), %%xmm2\n\t"	/* iptlo */
			"pshufb	%%xmm0, %%xmm2\n\t"
			"movdqa	"str_it(IPTHI)"(%"PTRP"4), %%xmm0\n\t"	/* ipthi */
			"pshufb	%%xmm1, %%xmm0\n\t"
			"pxor	%%xmm2, %%xmm0\n\t"
			"movdqa	%%xmm0, %%xmm7\n\t"
			/* output zeroth round key */
			"movdqa	%%xmm0, (%"PTRP"2)\n\t"
			/* load key second part */
			"lddqu	0x10 %5, %%xmm0\n\t"	/* key second part */
			/* shedule transform */
			"movdqa	"SSSE3_S0F", %%xmm1\n\t"
			"pandn	%%xmm0, %%xmm1\n\t"
			"psrld	    $4, %%xmm1\n\t"
			"pand	"SSSE3_S0F", %%xmm0\n\t"
			"movdqa	"str_it(IPTLO)"(%"PTRP"4), %%xmm2\n\t"	/* iptlo */
			"pshufb	%%xmm0, %%xmm2\n\t"
			"movdqa	"str_it(IPTHI)"(%"PTRP"4), %%xmm0\n\t"	/* ipthi */
			"pshufb	%%xmm1, %%xmm0\n\t"
			"pxor	%%xmm2, %%xmm0\n\t"
			"jmp	1f\n\t"
			".p2align 3\n"
			"1:\n\t"

			/* write output */
			"movdqa	%%xmm0, %%xmm2\n\t"	/* save xmm0 for later */
#  ifdef __i386__
			"movdqa	"str_it(MCF)"(%"PTRP"4), "SSSE3_MCF"\n\t"
#  endif
			"add	$16, %2\n\t"
			"pxor	"SSSE3_S63", %%xmm2\n\t"
			"pshufb	"SSSE3_MCF", %%xmm2\n\t"
			"movdqa	%%xmm2, %%xmm3\n\t"
			"pshufb	"SSSE3_MCF", %%xmm2\n\t"
			"pxor	%%xmm2, %%xmm3\n\t"
			"pshufb	"SSSE3_MCF", %%xmm2\n\t"
			"pxor	%%xmm2, %%xmm3\n\t"
			"pshufb	"str_it(SR)"(%"PTRP"1,%"PTRP"4),%%xmm3\n\t"
			"add	  $-16, %1\n\t"
			"and	   $48, %1\n\t"
			"movdqa	%%xmm3, (%"PTRP"2)\n\t"
			/* save cur_lo for later */
			"movdqa	%%xmm0, %%xmm5\n\t"

			/* high round */
			/* extract rcon from xmm8 */
			"pxor	%%xmm1, %%xmm1\n\t"
			"palignr	$15, %%xmm6, %%xmm1\n\t"
			"palignr	$15, %%xmm6, %%xmm6\n\t"
			"pxor	%%xmm1, %%xmm7\n\t"
			/* rotate */
			"pshufd	$0xFF, %%xmm0, %%xmm0\n\t"
			"palignr	$1, %%xmm0, %%xmm0\n\t"
			/* smear xmm7 */
			"movdqa	 %%xmm7, %%xmm1\n\t"
			"pslldq	     $4, %%xmm7\n\t"
			"pxor	 %%xmm1, %%xmm7\n\t"
			"movdqa	 %%xmm7, %%xmm1\n\t"
			"pslldq	     $8, %%xmm7\n\t"
			"pxor	 %%xmm1, %%xmm7\n\t"
			"pxor	"SSSE3_S63", %%xmm7\n\t"
			/* subbytes */
			"movdqa	"SSSE3_S0F", %%xmm1\n\t"
			"pandn	 %%xmm0, %%xmm1\n\t"
			"psrld	     $4, %%xmm1\n\t"	/* 1 = i */
			"pand	"SSSE3_S0F", %%xmm0\n\t"	/* 0 = k */
			"movdqa	"SSSE3_INVHI", %%xmm2\n\t"	/* 2 : a/k */
			"pshufb	 %%xmm0, %%xmm2\n\t"	/* 2 = a/k */
			"pxor	 %%xmm1, %%xmm0\n\t"	/* 0 = j */
			"movdqa	 "SSSE3_INVLO", %%xmm3\n\t"	/* 3 : 1/i */
			"pshufb	 %%xmm1, %%xmm3\n\t"	/* 3 = 1/i */
			"pxor	 %%xmm2, %%xmm3\n\t"	/* 3 = iak = 1/i + a/k */
			"movdqa	 "SSSE3_INVLO", %%xmm4\n\t"	/* 4 : 1/j */
			"pshufb	 %%xmm0, %%xmm4\n\t"	/* 4 = 1/j */
			"pxor	 %%xmm2, %%xmm4\n\t"	/* 4 = jak = 1/j + a/k */
			"movdqa	 "SSSE3_INVLO", %%xmm2\n\t"	/* 2 : 1/iak */
			"pshufb	 %%xmm3, %%xmm2\n\t"	/* 2 = 1/iak */
			"pxor	 %%xmm0, %%xmm2\n\t"	/* 2 = io */
			"movdqa	 "SSSE3_INVLO", %%xmm3\n\t"	/* 3 : 1/jak */
			"pshufb	 %%xmm4, %%xmm3\n\t"	/* 3 = 1/jak */
			"pxor	 %%xmm1, %%xmm3\n\t"	/* 3 = jo */
			"movdqa	"SSSE3_SB1LO", %%xmm4\n\t"	/* 4 : sbou */
			"pshufb  %%xmm2,  %%xmm4\n\t"		/* 4 = sbou */
			"movdqa	"SSSE3_SB1LO", %%xmm0\n\t"	/* 0 : sbot */
			"pshufb  %%xmm3, %%xmm0\n\t"	/* 0 = sb1t */
			"pxor	%%xmm4, %%xmm0\n\t"	/* 0 = sbox output */
			/* add in smeared stuff */
			"pxor	%%xmm7, %%xmm0\n\t"
			"dec	%0\n\t"
			"jz 	2f\n\t"
			"movdqa	%%xmm0, %%xmm7\n\t"
			/* write output */
			"movdqa	%%xmm0, %%xmm2\n\t"	/* save xmm0 for later */
#  ifdef __i386__
			"movdqa	"str_it(MCF)"(%"PTRP"4), "SSSE3_MCF"\n\t"
#  endif
			"add	$16, %2\n\t"
			"pxor	"SSSE3_S63", %%xmm2\n\t"
			"pshufb	"SSSE3_MCF", %%xmm2\n\t"
			"movdqa	%%xmm2, %%xmm3\n\t"
			"pshufb	"SSSE3_MCF", %%xmm2\n\t"
			"pxor	%%xmm2, %%xmm3\n\t"
			"pshufb	"SSSE3_MCF", %%xmm2\n\t"
			"pxor	%%xmm2, %%xmm3\n\t"
			"pshufb	"str_it(SR)"(%"PTRP"1,%"PTRP"4),%%xmm3\n\t"
			"add	  $-16, %1\n\t"
			"and	   $48, %1\n\t"
			"movdqa	%%xmm3, (%"PTRP"2)\n\t"

			/* low round */
			"pshufd	$0xff, %%xmm0, %%xmm0\n\t"
			/* swap low and high */
			"movdqa	%%xmm7, %%xmm1\n\t"
			"movdqa	%%xmm5, %%xmm7\n\t"
			"movdqa	%%xmm1, %%xmm5\n\t"

			/* smear xmm7 */
			"movdqa	 %%xmm7, %%xmm1\n\t"
			"pslldq	     $4, %%xmm7\n\t"
			"pxor	 %%xmm1, %%xmm7\n\t"
			"movdqa	 %%xmm7, %%xmm1\n\t"
			"pslldq	     $8, %%xmm7\n\t"
			"pxor	 %%xmm1, %%xmm7\n\t"
			"pxor	"SSSE3_S63", %%xmm7\n\t"
			/* subbytes */
			"movdqa	"SSSE3_S0F", %%xmm1\n\t"
			"pandn	 %%xmm0, %%xmm1\n\t"
			"psrld	     $4, %%xmm1\n\t"	/* 1 = i */
			"pand	"SSSE3_S0F", %%xmm0\n\t"	/* 0 = k */
			"movdqa	"SSSE3_INVHI", %%xmm2\n\t"	/* 2 : a/k */
			"pshufb	 %%xmm0, %%xmm2\n\t"	/* 2 = a/k */
			"pxor	 %%xmm1, %%xmm0\n\t"	/* 0 = j */
			"movdqa	 "SSSE3_INVLO", %%xmm3\n\t"	/* 3 : 1/i */
			"pshufb	 %%xmm1, %%xmm3\n\t"	/* 3 = 1/i */
			"pxor	 %%xmm2, %%xmm3\n\t"	/* 3 = iak = 1/i + a/k */
			"movdqa	 "SSSE3_INVLO", %%xmm4\n\t"	/* 4 : 1/j */
			"pshufb	 %%xmm0, %%xmm4\n\t"	/* 4 = 1/j */
			"pxor	 %%xmm2, %%xmm4\n\t"	/* 4 = jak = 1/j + a/k */
			"movdqa	 "SSSE3_INVLO", %%xmm2\n\t"	/* 2 : 1/iak */
			"pshufb	 %%xmm3, %%xmm2\n\t"	/* 2 = 1/iak */
			"pxor	 %%xmm0, %%xmm2\n\t"	/* 2 = io */
			"movdqa	 "SSSE3_INVLO", %%xmm3\n\t"	/* 3 : 1/jak */
			"pshufb	 %%xmm4, %%xmm3\n\t"	/* 3 = 1/jak */
			"pxor	 %%xmm1, %%xmm3\n\t"	/* 3 = jo */
			"movdqa	"SSSE3_SB1LO", %%xmm4\n\t"	/* 4 : sbou */
			"pshufb  %%xmm2,  %%xmm4\n\t"		/* 4 = sbou */
			"movdqa	"SSSE3_SB1LO", %%xmm0\n\t"	/* 0 : sbot */
			"pshufb  %%xmm3, %%xmm0\n\t"	/* 0 = sb1t */
			"pxor	%%xmm4, %%xmm0\n\t"	/* 0 = sbox output */
			/* add in smeared stuff */
			"pxor	%%xmm7, %%xmm0\n\t"
			"movdqa	%%xmm5, %%xmm7\n\t"
			"jmp 	1b\n"
			"2:\n\t"
			/* schedule last round key from xmm0 */
			"pshufb	"str_it(SR)"(%"PTRP"1,%"PTRP"4), %%xmm0\n\t"	/* output permute */
			"add	$16, %2\n\t"
			"pxor	"SSSE3_S63", %%xmm0\n\t"
			"movdqa	"SSSE3_S0F", %%xmm1\n\t"
			"pandn	%%xmm0, %%xmm1\n\t"
			"psrld	    $4, %%xmm1\n\t"
			"pand	"SSSE3_S0F", %%xmm0\n\t"	/* 0 = k */
			"movdqa	"str_it(OPTLO)"(%"PTRP"4), %%xmm2\n\t"	/* optlo */
			"pshufb	%%xmm0, %%xmm2\n\t"
			"movdqa	"str_it(OPTHI)"(%"PTRP"4), %%xmm0\n\t"	/* opthi */
			"pshufb	%%xmm1, %%xmm0\n\t"
			"pxor	%%xmm2, %%xmm0\n\t"
			"movdqa	%%xmm0, (%"PTRP"2)\n\t"	/* save last key */
		: /* %0 */ "=&r" (rounds),
		  /* %1 */ "=&r" (n),
		  /* %2 */ "=&r" (k),
		  /* %3 */ "=m" (ctx->k[0])
		: /* %4 */ "r" (AES_CONSTS_0MEMBER),
		  /* %5 */ "m" (*(const int *)in),
		  /* %6 */ "0" (7),
		  /* %7 */ "1" (16),
		  /* %8 */ "2" (&ctx->k[0])
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#   ifndef __i386__
		  , "xmm9", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
#   endif
#  endif
	);
}
#  undef SSSE3_INVLO
#  undef SSSE3_S0F
#  undef SSSE3_INVHI
#  undef SSSE3_S63
#  undef SSSE3_SB1LO
#  undef SSSE3_SB1HI
#  undef SSSE3_MCF
# endif
#endif

#define PL_R128 10
#define PL_R192 12
#define PL_R256 14
#define PL_DGEST (1 << 4)
#define PL_ALIGN (1 << 5)
#define PL_CIPHR (1 << 6)
#define PL_KEYGN (1 << 7)
#define PL_INTER (1 << 8)
#define PL_CRYPT (1 << 9)
#define PL_K128 (0 << 10)
#define PL_K192 (1 << 10)
#define PL_K256 (2 << 10)
/*
 * gcc has a hard time to digest this inline asm.
 * If no optimization is in effect (like for debuging),
 * it is not capable of assigning registers.
 * The compiler has eax ebp esp, but still:
 * "Wuah! No more reg in class GENERAL REG"
 *
 * -O1 is enough to fix this, but unfortunatly maybe
 * makes debugging already impossible because to much
 * is removed (internal symbols & small funcs vanish,
 * which would be OK, but you can not see the args
 * anymore).
 *
 * And as always, you can not fold this back to a single/
 * set of gcc option which fixes this (-foo-bla-blub)
 * because -O1 enables more than what is selectable with
 * command line switches (basic code analysis steps).
 *
 * GCC 4.4 and up has a attribute to override optimization
 * levels on a function by function basis, but its fresh,
 * at least for some time...
 */
static GCC_ATTR_OPTIMIZE(3) void aes_ecb_encrypt128_padlock(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	static const struct {
		uint32_t x[4];
	} control_word GCC_ATTR_ALIGNED(16) = {
		{PL_R128 | PL_KEYGN | PL_K128, 0, 0, 0}
	};
#if _GNUC_PREREQ (4,4) || defined(__OPTIMIZE__)
	size_t c, S, D;

	asm(
#if defined(__i386__) && defined(__PIC__)
			"xchg	%5, %%ebx\n\t"
#endif
			"rep xcryptecb\n\t"
#if defined(__i386__) && defined(__PIC__)
			"xchg	%5, %%ebx\n"
#endif
		: /* %0 */ "=S" (S),
		  /* %1 */ "=D" (D),
		  /* %2 */ "=c" (c),
		  /* %3 */ "=m" (*(char *)out)
		: /* %4 */ "d" (&control_word),
#if defined(__i386__) && defined(__PIC__)
		  /* %5 */ "a" (&ctx->k),
#else
		  /* %5 */ "b" (&ctx->k),
#endif
		  /* %6 */ "2" (1),
		  /* %7 */ "0" (in),
		  /* %8 */ "1" (out),
		  /* %9 */ "m" (*(const char *)in)
	);
#else
# ifdef __i386__
#  define REG_b "%%ebx"
#  define REG_c "%%ecx"
#  define REG_d "%%edx"
#  define REG_S "%%esi"
#  define REG_D "%%edi"
# else
#  define REG_b "%%rbx"
#  define REG_c "%%rcx"
#  define REG_d "%%rdx"
#  define REG_S "%%rsi"
#  define REG_D "%%rdi"
# endif
	const void *ptr = ctx->k;
	asm(
			"push	"REG_S"\n\t"
			"push	"REG_D"\n\t"
			"push	"REG_c"\n\t"
			"push	"REG_d"\n\t"
			"push	"REG_b"\n\t"
			"mov	%3, "REG_S"\n\t"
			"mov	%4, "REG_D"\n\t"
			"mov	%1, "REG_d"\n\t"
			"mov	%2, "REG_b"\n\t"
			"xor	"REG_c", "REG_c"\n\t"
			"add	$1, "REG_c"\n\t"
			"rep xcryptecb\n\t"
			"pop	"REG_b"\n\t"
			"pop	"REG_d"\n\t"
			"pop	"REG_c"\n\t"
			"pop	"REG_D"\n\t"
			"pop	"REG_S"\n\t"
		: /* %0 */ "=m" (*(char *)out)
		: /* %1 */ "m" (&control_word),
		  /* %2 */ "m" (ptr),
		  /* %3 */ "m" (in),
		  /* %4 */ "m" (out),
		  /* %5 */ "m" (*(const char *)in)
	);
#endif
}

static GCC_ATTR_OPTIMIZE(3) void aes_ecb_encrypt256_padlock(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	static const struct {
		uint32_t x[4];
	} control_word GCC_ATTR_ALIGNED(16) = {
		{PL_R256 | PL_K256, 0, 0, 0}
	};
#if _GNUC_PREREQ (4,4) || defined(__OPTIMIZE__)
	size_t c, S, D;

	asm(
#if defined(__i386__) && defined(__PIC__)
			"xchg	%5, %%ebx\n\t"
#endif
			"rep xcryptecb\n\t"
#if defined(__i386__) && defined(__PIC__)
			"xchg	%5, %%ebx\n"
#endif
		: /* %0 */ "=S" (S),
		  /* %1 */ "=D" (D),
		  /* %2 */ "=c" (c),
		  /* %3 */ "=m" (*(char *)out)
		: /* %4 */ "d" (&control_word),
#if defined(__i386__) && defined(__PIC__)
		  /* %5 */ "a" (&ctx->k),
#else
		  /* %5 */ "b" (&ctx->k),
#endif
		  /* %6 */ "2" (1),
		  /* %7 */ "0" (in),
		  /* %8 */ "1" (out),
		  /* %9 */ "m" (*(const char *)in)
	);
#else
# ifdef __i386__
#  define REG_b "%%ebx"
#  define REG_c "%%ecx"
#  define REG_d "%%edx"
#  define REG_S "%%esi"
#  define REG_D "%%edi"
# else
#  define REG_b "%%rbx"
#  define REG_c "%%rcx"
#  define REG_d "%%rdx"
#  define REG_S "%%rsi"
#  define REG_D "%%rdi"
# endif
	const void *ptr = ctx->k;
	asm(
			"push	"REG_S"\n\t"
			"push	"REG_D"\n\t"
			"push	"REG_c"\n\t"
			"push	"REG_d"\n\t"
			"push	"REG_b"\n\t"
			"mov	%3, "REG_S"\n\t"
			"mov	%4, "REG_D"\n\t"
			"mov	%1, "REG_d"\n\t"
			"mov	%2, "REG_b"\n\t"
			"xor	"REG_c", "REG_c"\n\t"
			"add	$1, "REG_c"\n\t"
			"rep xcryptecb\n\t"
			"pop	"REG_b"\n\t"
			"pop	"REG_d"\n\t"
			"pop	"REG_c"\n\t"
			"pop	"REG_D"\n\t"
			"pop	"REG_S"\n\t"
		: /* %0 */ "=m" (*(char *)out)
		: /* %1 */ "m" (&control_word),
		  /* %2 */ "m" (ptr),
		  /* %3 */ "m" (in),
		  /* %4 */ "m" (out),
		  /* %5 */ "m" (*(const char *)in)
	);
#endif
}

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 219
/*
 * This code does not use any AVX feature, it only uses the new
 * v* opcodes, so the upper half of the register gets 0-ed,
 * and the CPU is not caught with lower/upper half merges
 */
static void aes_ecb_encrypt128_AVXAES(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	size_t k;

	asm(
			"prefetch	(%"PTRP"0)\n\t"
			"add	$0x30, %0\n\t"	/* prime key address */
			"vlddqu	%2, %%xmm0\n\t"	/* input */
			"vpxor	-0x30(%"PTRP"0), %%xmm0, %%xmm0\n\t"	/* key round 0 */
			"vaesenc	-0x20(%"PTRP"0), %%xmm0, %%xmm0\n\t"	/* key key */
			"vaesenc	-0x10(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	     (%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	 0x10(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	 0x20(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	 0x30(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	 0x40(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	 0x50(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	 0x60(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenclast	0x70(%"PTRP"0), %%xmm0, %%xmm0\n\t"	/* last round */
			"vmovdqu	%%xmm0, %1"	/* output */
		: /* %0 */ "=r" (k),
		  /* %1 */ "=m" (*(char *)out)
		: /* %2 */ "m" (*(const char *)in),
		  /* %3 */ "0" (&ctx->k)
#  ifdef __SSE__
		: "xmm0"
#  endif
	);
}

static void aes_ecb_encrypt256_AVXAES(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	size_t k;

	asm(
			"prefetch	(%"PTRP"0)\n\t"
			"add	$0x70, %0\n\t"	/* prime key address */
			"vlddqu	%2, %%xmm0\n\t"	/* input */
			"vpxor	-0x70(%"PTRP"0), %%xmm0, %%xmm0\n\t"	/* key round 0 */
			"vaesenc	-0x60(%"PTRP"0), %%xmm0, %%xmm0\n\t"	/* key key */
			"vaesenc	-0x40(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	-0x40(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	-0x30(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	-0x20(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	-0x10(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	     (%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	 0x10(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	 0x20(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	 0x30(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	 0x40(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	 0x50(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenc	 0x60(%"PTRP"0), %%xmm0, %%xmm0\n\t"
			"vaesenclast	0x70(%"PTRP"0), %%xmm0, %%xmm0\n\t"	/* last round */
			"vmovdqu	%%xmm0, %1"	/* output */
		: /* %0 */ "=r" (k),
		  /* %1 */ "=m" (*(char *)out)
		: /* %2 */ "m" (*(const char *)in),
		  /* %3 */ "0" (&ctx->k)
#  ifdef __SSE__
		: "xmm0"
#  endif
	);
}
# endif

# if HAVE_BINUTILS >= 217
static void aes_ecb_encrypt128_SSEAES(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	size_t k;

	asm(
			"movups	%2, %%xmm0\n\t"	/* input */
			"movaps	(%"PTRP"0), %%xmm1\n\t"	/* key */
			"pxor	%%xmm1, %%xmm0\n\t"	/* round 0 */
			"add	$0x30, %0\n\t"	/* prime key address */
			"movaps	-0x20(%"PTRP"0), %%xmm1\n\t"	/* key key */
			/* aesenc %xmm1, %xmm0 */	/* round */
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	-0x10(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x10(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x20(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x30(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x40(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x50(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x60(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x70(%"PTRP"0), %%xmm1\n\t"
			/* aesenclast %xmm1, %xmm0 */	/* last round */
			".byte	0x66, 0x0f, 0x38, 0xdd, 0xc1\n\t"
			"movups	%%xmm0, %1"	/* output */
		: /* %0 */ "=r" (k),
		  /* %1 */ "=m" (*(char *)out)
		: /* %2 */ "m" (*(const char *)in),
		  /* %3 */ "0" (&ctx->k)
#  ifdef __SSE__
		: "xmm0", "xmm1"
#  endif
	);
}

static void aes_ecb_encrypt256_SSEAES(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	size_t k;

	asm(
			"movups	%2, %%xmm0\n\t"	/* input */
			"movaps	(%"PTRP"0), %%xmm1\n\t"	/* key */
			"pxor	%%xmm1, %%xmm0\n\t"	/* round 0 */
			"add	$0x70, %0\n\t"	/* prime key address */
			"movaps	-0x60(%"PTRP"0), %%xmm1\n\t"	/* key key */
			/* aesenc %xmm1, %xmm0 */	/* round */
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	-0x50(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	-0x40(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	-0x30(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	-0x20(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	-0x10(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x10(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x20(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x30(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x40(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x50(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x60(%"PTRP"0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x70(%"PTRP"0), %%xmm1\n\t"
			/* aesenclast %xmm1, %xmm0 */	/* last round */
			".byte	0x66, 0x0f, 0x38, 0xdd, 0xc1\n\t"
			"movups	%%xmm0, %1"	/* output */
		: /* %0 */ "=r" (k),
		  /* %1 */ "=m" (*(char *)out)
		: /* %2 */ "m" (*(const char *)in),
		  /* %3 */ "0" (&ctx->k)
#  ifdef __SSE__
		: "xmm0", "xmm1"
#  endif
	);
}

/*
 * original SSSE3 code by:
 * By Mike Hamburg (Stanford University), 2009
 * Public domain
 *
 * adapted by me
 */
#  if HAVE_BINUTILS >= 219
/*
 * This code does not use any AVX feature, it only uses the new
 * v* opcodes, so the upper half of the register gets 0-ed,
 * and the CPU is not caught with lower/upper half merges
 */
#   ifndef __i386__
#    define AVX_SB1LO "%%xmm13"
#    define AVX_SB1HI "%%xmm12"
#    define AVX_SB2LO "%%xmm15"
#    define AVX_SB2HI "%%xmm14"
#   else
#    define AVX_SB1LO str_it(SB1LO)"(%"PTRP"4)"
#    define AVX_SB1HI str_it(SB1HI)"(%"PTRP"4)"
#    define AVX_SB2LO str_it(SB2LO)"(%"PTRP"4)"
#    define AVX_SB2HI str_it(SB2HI)"(%"PTRP"4)"
#   endif
static void aes_ecb_encrypt_AVX(const struct aes_encrypt_ctx *ctx, void *out, const void *in, size_t mrounds)
{
	size_t rounds, n;
	void *k;

	prefetch(in);
	prefetchw(out);
	prefetch(ctx->k);
	prefetch(&aes_consts);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	asm volatile (
			"vlddqu	%5, %%xmm0\n\t"
			"vmovdqa	"str_it(S0F)"(%"PTRP"4), %%xmm6\n\t"	/* 0F */
			"vmovdqa	"str_it(INVLO)"(%"PTRP"4), %%xmm5\n\t"	/* inv */
			"vmovdqa	"str_it(INVHI)"(%"PTRP"4), %%xmm7\n\t"	/* inva */
#   ifndef __i386__
			"vmovdqa	"str_it(SB1LO)"(%"PTRP"4), "AVX_SB1LO"\n\t"	/* sb1u */
			"vmovdqa	"str_it(SB1HI)"(%"PTRP"4), "AVX_SB1HI"\n\t"	/* sb1t */
			"vmovdqa	"str_it(SB2LO)"(%"PTRP"4), "AVX_SB2LO"\n\t"	/* sb2u */
			"vmovdqa	"str_it(SB2HI)"(%"PTRP"4), "AVX_SB2HI"\n\t"	/* sb2t */
#   endif
			"vmovdqa	"str_it(IPTLO)"(%"PTRP"4), %%xmm2\n\t"	/* iptlo */
			"vpandn	 %%xmm0, %%xmm6, %%xmm1\n\t"
			"vpsrld	     $4, %%xmm1, %%xmm1\n\t"
			"vpand	 %%xmm6, %%xmm0, %%xmm0\n\t"
			"vpshufb	 %%xmm0, %%xmm2, %%xmm2\n\t"
			"vmovdqa	"str_it(IPTHI)"(%"PTRP"4), %%xmm0\n\t"	/* ipthi */
			"vpshufb	 %%xmm1, %%xmm0, %%xmm0\n\t"
			"vpxor	 16(%"PTRP"2), %%xmm2, %%xmm2\n\t"
			"vpxor	 %%xmm2, %%xmm0, %%xmm0\n\t"
			"add	    $32, %2\n\t"
			"jmp	     2f\n\t"
			".p2align 4\n"
			"1:\n\t"
		/* middle of middle round */
			"vmovdqa	"str_it(MCF)"(%"PTRP"1,%"PTRP"4), %%xmm1\n\t"
			AVX_M2OP("vpshufb", "%%xmm2", AVX_SB1LO, "%%xmm4")	/* 4 = sb1u */
			"vpxor	   (%"PTRP"2), %%xmm4, %%xmm4\n\t"	/* 4 = sb1u + k */
			AVX_M2OP("vpshufb", "%%xmm3", AVX_SB1HI, "%%xmm0")	/* 0 = sb1t */
			"vpxor	 %%xmm4, %%xmm0, %%xmm0\n\t"	/* 0 = A */
			AVX_M2OP("vpshufb", "%%xmm2", AVX_SB2LO, "%%xmm4")	/* 4 = sb2u */
			AVX_M2OP("vpshufb", "%%xmm3", AVX_SB2HI, "%%xmm2")	/* 2 = sb2t */
			"vpxor	 %%xmm4, %%xmm2, %%xmm2\n\t"	/* 2 = 2A */
			"vpshufb	"str_it(MCB)"(%"PTRP"1,%"PTRP"4), %%xmm0, %%xmm3\n\t"	/* 3 = D */
			"vpshufb	 %%xmm1, %%xmm0, %%xmm0\n\t"	/* 0 = B */
			"vpxor	 %%xmm2, %%xmm0, %%xmm0\n\t"	/* 0 = 2A+B */
			"vpxor	 %%xmm0, %%xmm3, %%xmm3\n\t"	/* 3 = 2A+B+D */
			"add	    $16, %2\n\t"	/* next key */
			"vpshufb	 %%xmm1, %%xmm0, %%xmm0\n\t"	/* 0 = 2B+C */
			"vpxor	 %%xmm3, %%xmm0, %%xmm0\n\t"	/* 0 = 2A+3B+C+D */
			"add	    $16, %1\n\t"	/* next mc */
			"and	    $48, %1\n\t"	/* ... mod 4 */
			"dec	     %0\n"	/* rounds-- */
			"2:\n\t"
			/* top of round */
			"vpandn	 %%xmm0, %%xmm6, %%xmm1\n\t"	/* 1 = i<<4 */
			"vpsrld	     $4, %%xmm1, %%xmm1\n\t"	/* 1 = i */
			"vpand	 %%xmm6, %%xmm0, %%xmm0\n\t"	/* 0 = k */
			"vpshufb	 %%xmm0, %%xmm7, %%xmm2\n\t"	/* 2 = a/k */
			"vpxor	 %%xmm1, %%xmm0, %%xmm0\n\t"	/* 0 = j */
			"vpshufb	 %%xmm1, %%xmm5, %%xmm3\n\t"	/* 3 = 1/i */
			"vpxor	 %%xmm2, %%xmm3, %%xmm3\n\t"	/* 3 = iak = 1/i + a/k */
			"vpshufb	 %%xmm0, %%xmm5, %%xmm4\n\t"	/* 4 = 1/j */
			"vpxor	 %%xmm2, %%xmm4, %%xmm4\n\t"	/* 4 = jak = 1/j + a/k */
			"vpshufb	 %%xmm3, %%xmm5, %%xmm2\n\t"	/* 2 = 1/iak */
			"vpxor	 %%xmm0, %%xmm2, %%xmm2\n\t"	/* 2 = io */
			"vpshufb	 %%xmm4, %%xmm5, %%xmm3\n\t"	/* 3 = 1/jak */
			"vpxor	 %%xmm1, %%xmm3, %%xmm3\n\t"	/* # 3 = jo */
			"jnz	1b\n\t"
			/* middle of last round */
			"vmovdqa	"str_it(SBOLO)"(%"PTRP"4), %%xmm4\n\t"	/* 3 : sbou */
			"vmovdqa	"str_it(SBOHI)"(%"PTRP"4), %%xmm0\n\t"	/* 0 : sbot */
			"vpshufb	 %%xmm2, %%xmm4, %%xmm4\n\t"	/* 4 = sbou */
			"vpshufb	 %%xmm3, %%xmm0, %%xmm0\n\t"	/* 0 = sb1t */
			"vpxor	   (%"PTRP"2), %%xmm4, %%xmm4\n\t"	/* 4 = sb1u + k */
			"vpxor	%%xmm0, %%xmm4, %%xmm0\n\t"	/* 0 = A */
			"vpshufb	"str_it(SR)"(%"PTRP"1,%"PTRP"4), %%xmm0, %%xmm0\n\t"
			"vmovdqu	%%xmm0, %3\n\t"
		: /* %0 */ "=&r" (rounds),
		  /* %1 */ "=&R" (n),
		  /* %2 */ "=&r" (k),
		  /* %3 */ "=m" (*(int *)out)
		: /* %4 */ "R" (AES_CONSTS_0MEMBER),
		  /* %5 */ "m" (*(const int *)in),
		  /* %6 */ "0" (mrounds),
		  /* %7 */ "1" (16),
		  /* %8 */ "2" (&ctx->k[0])
#   ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#    ifndef __i386__
		  , "xmm12", "xmm13", "xmm14", "xmm15"
#    endif
#   endif
	);
}
#   undef AVX_SB1LO
#   undef AVX_SB1HI
#   undef AVX_SB2LO
#   undef AVX_SB2HI

static void aes_ecb_encrypt128_AVX(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	aes_ecb_encrypt_AVX(ctx, out, in, 10 - 1);
}

static void aes_ecb_encrypt256_AVX(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	aes_ecb_encrypt_AVX(ctx, out, in, 14 - 1);
}

#  endif

#  ifndef __i386__
#   define SSSE3_SB1LO "%%xmm13"
#   define SSSE3_SB1HI "%%xmm12"
#   define SSSE3_SB2LO "%%xmm15"
#   define SSSE3_SB2HI "%%xmm14"
#  else
#   define SSSE3_SB1LO str_it(SB1LO)"(%"PTRP"4)"
#   define SSSE3_SB1HI str_it(SB1HI)"(%"PTRP"4)"
#   define SSSE3_SB2LO str_it(SB2LO)"(%"PTRP"4)"
#   define SSSE3_SB2HI str_it(SB2HI)"(%"PTRP"4)"
#  endif
static void aes_ecb_encrypt_SSSE3(const struct aes_encrypt_ctx *ctx, void *out, const void *in, size_t mrounds)
{
	size_t rounds, n;
	void *k;

	prefetch(in);
	prefetchw(out);
	prefetch(ctx->k);
	prefetch(&aes_consts);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);

	asm volatile (
			"lddqu	%5, %%xmm0\n\t"
			"movdqa	"str_it(S0F)"(%"PTRP"4), %%xmm6\n\t"	/* 0F */
			"movdqa	"str_it(INVLO)"(%"PTRP"4), %%xmm5\n\t"	/* inv */
			"movdqa	"str_it(INVHI)"(%"PTRP"4), %%xmm7\n\t"	/* inva */
#  ifndef __i386__
			"movdqa	"str_it(SB1LO)"(%"PTRP"4), "SSSE3_SB1LO"\n\t"	/* sb1u */
			"movdqa	"str_it(SB1HI)"(%"PTRP"4), "SSSE3_SB1HI"\n\t"	/* sb1t */
			"movdqa	"str_it(SB2LO)"(%"PTRP"4), "SSSE3_SB2LO"\n\t"	/* sb2u */
			"movdqa	"str_it(SB2HI)"(%"PTRP"4), "SSSE3_SB2HI"\n\t"	/* sb2t */
#  endif
			"movdqa	"str_it(IPTLO)"(%"PTRP"4), %%xmm2\n\t"	/* iptlo */
			"movdqa	 %%xmm6, %%xmm1\n\t"
			"pandn	 %%xmm0, %%xmm1\n\t"
			"psrld	     $4, %%xmm1\n\t"
			"pand	 %%xmm6, %%xmm0\n\t"
			"pshufb	 %%xmm0, %%xmm2\n\t"
			"movdqa	"str_it(IPTHI)"(%"PTRP"4), %%xmm0\n\t"	/* ipthi */
			"pshufb	 %%xmm1, %%xmm0\n\t"
			"pxor	 16(%"PTRP"2), %%xmm2\n\t"
			"pxor	 %%xmm2, %%xmm0\n\t"
			"add	    $32, %2\n\t"
			"jmp	     2f\n\t"
			".p2align 4\n"
			"1:\n\t"
		/* middle of middle round */
			"movdqa	"str_it(MCF)"(%"PTRP"1,%"PTRP"4), %%xmm1\n\t"
			"movdqa	"SSSE3_SB1LO", %%xmm4\n\t"	/* 4 : sb1u */
			"pshufb	 %%xmm2, %%xmm4\n\t"	/* 4 = sb1u */
			"pxor	   (%"PTRP"2), %%xmm4\n\t"	/* 4 = sb1u + k */
			"movdqa	"SSSE3_SB1HI", %%xmm0\n\t"	/* 0 : sb1t */
			"pshufb	 %%xmm3, %%xmm0\n\t"	/* 0 = sb1t */
			"pxor	 %%xmm4, %%xmm0\n\t"	/* 0 = A */
			"movdqa	"SSSE3_SB2LO", %%xmm4\n\t"	/* 4 : sb2u */
			"pshufb	 %%xmm2, %%xmm4\n\t"	/* 4 = sb2u */
			"movdqa	"SSSE3_SB2HI", %%xmm2\n\t"	/* 2 : sb2t */
			"pshufb	 %%xmm3, %%xmm2\n\t"	/* 2 = sb2t */
			"pxor	 %%xmm4, %%xmm2\n\t"	/* 2 = 2A */
			"movdqa	 %%xmm0, %%xmm3\n\t"	/* 3 = A */
			"pshufb	 %%xmm1, %%xmm0\n\t"	/* 0 = B */
			"pxor	 %%xmm2, %%xmm0\n\t"	/* 0 = 2A+B */
			"pshufb	"str_it(MCB)"(%"PTRP"1,%"PTRP"4), %%xmm3\n\t"	/* 3 = D */
			"pxor	 %%xmm0, %%xmm3\n\t"	/* 3 = 2A+B+D */
			"add	    $16, %2\n\t"	/* next key */
			"pshufb	 %%xmm1, %%xmm0\n\t"	/* 0 = 2B+C */
			"pxor	 %%xmm3, %%xmm0\n\t"	/* 0 = 2A+3B+C+D */
			"add	    $16, %1\n\t"	/* next mc */
			"and	    $48, %1\n\t"	/* ... mod 4 */
			"dec	     %0\n"	/* rounds-- */
			"2:\n\t"
			/* top of round */
			"movdqa	 %%xmm6, %%xmm1\n\t"	/* 1 : i */
			"pandn	 %%xmm0, %%xmm1\n\t"	/* 1 = i<<4 */
			"psrld	     $4, %%xmm1\n\t"	/* 1 = i */
			"pand	 %%xmm6, %%xmm0\n\t"	/* 0 = k */
			"movdqa	 %%xmm7, %%xmm2\n\t"	/* 2 : a/k */
			"pshufb	 %%xmm0, %%xmm2\n\t"	/* 2 = a/k */
			"pxor	 %%xmm1, %%xmm0\n\t"	/* 0 = j */
			"movdqa	 %%xmm5, %%xmm3\n\t"	/* 3 : 1/i */
			"pshufb	 %%xmm1, %%xmm3\n\t"	/* 3 = 1/i */
			"pxor	 %%xmm2, %%xmm3\n\t"	/* 3 = iak = 1/i + a/k */
			"movdqa	 %%xmm5, %%xmm4\n\t"	/* 4 : 1/j */
			"pshufb	 %%xmm0, %%xmm4\n\t"	/* 4 = 1/j */
			"pxor	 %%xmm2, %%xmm4\n\t"	/* 4 = jak = 1/j + a/k */
			"movdqa	 %%xmm5, %%xmm2\n\t"	/* 2 : 1/iak */
			"pshufb	 %%xmm3, %%xmm2\n\t"	/* 2 = 1/iak */
			"pxor	 %%xmm0, %%xmm2\n\t"	/* 2 = io */
			"movdqa	 %%xmm5, %%xmm3\n\t"	/* 3 : 1/jak */
			"pshufb	 %%xmm4, %%xmm3\n\t"	/* 3 = 1/jak */
			"pxor	 %%xmm1, %%xmm3\n\t"	/* # 3 = jo */
			"jnz	1b\n\t"
			/* middle of last round */
			"movdqa	"str_it(SBOLO)"(%"PTRP"4), %%xmm4\n\t"	/* 3 : sbou */
			"movdqa	"str_it(SBOHI)"(%"PTRP"4), %%xmm0\n\t"	/* 0 : sbot */
			"pshufb	 %%xmm2, %%xmm4\n\t"	/* 4 = sbou */
			"pshufb	 %%xmm3, %%xmm0\n\t"	/* 0 = sb1t */
			"pxor	   (%"PTRP"2), %%xmm4\n\t"	/* 4 = sb1u + k */
			"pxor	 %%xmm4, %%xmm0\n\t"	/* 0 = A */
			"pshufb	"str_it(SR)"(%"PTRP"1,%"PTRP"4), %%xmm0\n\t"
			"movdqu	%%xmm0, %3\n\t"
		: /* %0 */ "=&r" (rounds),
		  /* %1 */ "=&R" (n),
		  /* %2 */ "=&r" (k),
		  /* %3 */ "=m" (*(int *)out)
		: /* %4 */ "R" (AES_CONSTS_0MEMBER),
		  /* %5 */ "m" (*(const int *)in),
		  /* %6 */ "0" (mrounds),
		  /* %7 */ "1" (16),
		  /* %8 */ "2" (&ctx->k[0])
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#   ifndef __i386__
		  , "xmm12", "xmm13", "xmm14", "xmm15"
#   endif
#  endif
	);
}
#   undef SSSE3_SB1LO
#   undef SSSE3_SB1HI
#   undef SSSE3_SB2LO
#   undef SSSE3_SB2HI

static void aes_ecb_encrypt128_SSSE3(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	aes_ecb_encrypt_SSSE3(ctx, out, in, 10 - 1);
}

static void aes_ecb_encrypt256_SSSE3(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	aes_ecb_encrypt_SSSE3(ctx, out, in, 14 - 1);
}
# endif
#endif

#define ARCH_NAME_SUFFIX _generic
#include "../generic/aes.c"

static __init_cdata const struct test_cpu_feature tfeat_aes_encrypt_key128[] =
{
	{.func = (void (*)(void))aes_encrypt_key128_generic, .features = {[7] = CFB(CFEATURE_PL_ACE_E)}},
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 219
	{.func = (void (*)(void))aes_encrypt_key128_AVXAES,  .features = {[1] = CFB(CFEATURE_AES)|CFB(CFEATURE_AVX)}, .flags = CFF_AVX_TST},
	{.func = (void (*)(void))aes_encrypt_key128_AVX,     .features = {[1] = CFB(CFEATURE_AVX)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))aes_encrypt_key128_SSEAES,  .features = {[1] = CFB(CFEATURE_AES)}},
	{.func = (void (*)(void))aes_encrypt_key128_SSSE3,   .features = {[1] = CFB(CFEATURE_SSSE3)}},
# endif
#endif
	{.func = (void (*)(void))aes_encrypt_key128_generic, .features = {}, .flags = CFF_DEFAULT},
};

static __init_cdata const struct test_cpu_feature tfeat_aes_ecb_encrypt128[] =
{
	{.func = (void (*)(void))aes_ecb_encrypt128_padlock,  .features = {[7] = CFB(CFEATURE_PL_ACE_E)}},
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 219
	{.func = (void (*)(void))aes_ecb_encrypt128_AVXAES,   .features = {[1] = CFB(CFEATURE_AES)|CFB(CFEATURE_AVX)}, .flags = CFF_AVX_TST},
	{.func = (void (*)(void))aes_ecb_encrypt128_AVX,      .features = {[1] = CFB(CFEATURE_AVX)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))aes_ecb_encrypt128_SSEAES,   .features = {[1] = CFB(CFEATURE_AES)}},
	{.func = (void (*)(void))aes_ecb_encrypt128_SSSE3,    .features = {[1] = CFB(CFEATURE_SSSE3)}},
# endif
#endif
	{.func = (void (*)(void))aes_ecb_encrypt128_generic,  .features = {}, .flags = CFF_DEFAULT},
};

DYN_JMP_DISPATCH_NR(aes_encrypt_key128, (struct aes_encrypt_ctx *ctx, const void *in), (ctx, in))
DYN_JMP_DISPATCH_NR(aes_ecb_encrypt128, (const struct aes_encrypt_ctx *ctx, void *out, const void *in), (ctx, out, in))

static __init_cdata const struct test_cpu_feature tfeat_aes_encrypt_key256[] =
{
	{.func = (void (*)(void))aes_encrypt_key256_generic, .features = {[7] = CFB(CFEATURE_PL_ACE_E)}},
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 219
	{.func = (void (*)(void))aes_encrypt_key256_AVXAES,  .features = {[1] = CFB(CFEATURE_AES)|CFB(CFEATURE_AVX)}, .flags = CFF_AVX_TST},
	{.func = (void (*)(void))aes_encrypt_key256_AVX,     .features = {[1] = CFB(CFEATURE_AVX)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))aes_encrypt_key256_SSEAES,  .features = {[1] = CFB(CFEATURE_AES)}},
	{.func = (void (*)(void))aes_encrypt_key256_SSSE3,   .features = {[1] = CFB(CFEATURE_SSSE3)}},
# endif
#endif
	{.func = (void (*)(void))aes_encrypt_key256_generic, .features = {}, .flags = CFF_DEFAULT},
};

static __init_cdata const struct test_cpu_feature tfeat_aes_ecb_encrypt256[] =
{
	{.func = (void (*)(void))aes_ecb_encrypt256_padlock,  .features = {[7] = CFB(CFEATURE_PL_ACE_E)}},
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 219
	{.func = (void (*)(void))aes_ecb_encrypt256_AVXAES,   .features = {[1] = CFB(CFEATURE_AES)|CFB(CFEATURE_AVX)}, .flags = CFF_AVX_TST},
	{.func = (void (*)(void))aes_ecb_encrypt256_AVX,      .features = {[1] = CFB(CFEATURE_AVX)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))aes_ecb_encrypt256_SSEAES,   .features = {[1] = CFB(CFEATURE_AES)}},
	{.func = (void (*)(void))aes_ecb_encrypt256_SSSE3,    .features = {[1] = CFB(CFEATURE_SSSE3)}},
# endif
#endif
	{.func = (void (*)(void))aes_ecb_encrypt256_generic,  .features = {}, .flags = CFF_DEFAULT},
};

DYN_JMP_DISPATCH_NR(aes_encrypt_key256, (struct aes_encrypt_ctx *ctx, const void *in), (ctx, in))
DYN_JMP_DISPATCH_NR(aes_ecb_encrypt256, (const struct aes_encrypt_ctx *ctx, void *out, const void *in), (ctx, out, in))

/*@unused@*/
static char const rcsid_aesx[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
