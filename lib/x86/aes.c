/*
 * aes.c
 * AES routines, x86 implementation
 *
 * Copyright (c) 2009-2011 Jan Seiffert
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
#include "x86_features.h"

static void aes_encrypt_key128_generic(struct aes_encrypt_ctx *, const void *);
static void aes_ecb_encrypt_generic(const struct aes_encrypt_ctx *, void *, const void *);
static void aes_ecb_encrypt_padlock(const struct aes_encrypt_ctx *, void *, const void *);

#ifdef HAVE_BINUTILS
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
			".align 16\n"
			"key_expansion_128:\n\t"
			"movaps	%%xmm1, %%xmm4\n\t"
			"psrldq	$12, %%xmm1\n\t"
			"pxor	%%xmm0, %%xmm1\n\t"
			"palignr	$12, %%xmm4, %%xmm1\n\t"
			"pxor	%%xmm0, %%xmm1\n\t"
			"palignr	$12, %%xmm4, %%xmm1\n\t"
			"pxor	%%xmm0, %%xmm1\n\t"
			"palignr	$12, %%xmm4, %%xmm1\n\t"
			"pxor	%%xmm1, %%xmm0\n\t"
			"movaps	%%xmm0, (%0)\n\t"
			"add	$0x10, %0\n\t"
			"ret\n\t"
#ifdef HAVE_SUBSECTION
			".previous\n\t"
#else
			"1:\n\t"
#endif
			"movups	%2, %%xmm0\n\t"	/* user key (first 16 bytes) */
			"movaps	%%xmm0, (%0)\n\t"
			"add	0x10, %0\n\t"	/* key addr */
			/* aeskeygenassist $0x1, %xmm0, %xmm1 */
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x01\n\t" /* round 1 */
			"call	key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x02\n\t" /* round 2 */
			"call	key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x04\n\t" /* round 3 */
			"call	key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x08\n\t" /* round 4 */
			"call	key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x10\n\t" /* round 5 */
			"call	key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x20\n\t" /* round 6 */
			"call	key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x40\n\t" /* round 7 */
			"call	key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x80\n\t" /* round 8 */
			"call	key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x1b\n\t" /* round 9 */
			"call	key_expansion_128\n\t"
			".byte	0x66, 0x0f, 0x3a, 0xdf, 0xc8, 0x36\n\t" /* round 10 */
			"call	key_expansion_128"
		: /* %0 */ "=r" (k),
		  /* %1 */ "=m" (*ctx->k)
		: /* %2 */ "m" (*(const char *)in),
		  /* %3 */ "0" (&ctx->k)
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm4"
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
#  define S0F	  0
#  define S63	 16
#  define IPTLO	 32
#  define IPTHI	 48
#  define SBOLO	 64
#  define SBOHI	 80
#  define INVLO	 96
#  define INVHI	112
#  define SB1LO	128
#  define SB1HI	144
#  define SB2LO	160
#  define SB2HI	176
#  define MCF	192
#  define MCB	256
#  define SR	320
#  define RCON	384
#  define OPTLO	400
#  define OPTHI	416

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
	.iptlo = { 0xC2B2E8985A2A7000ULL, 0xCABAE09052227808ULL },
	.ipthi = { 0x4C01307D317C4D00ULL, 0xCD80B1FCB0FDCC81ULL },
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
	.rcon  = { 0x1F8391B9AF9DEEB6ULL, 0x702A98084D7C7D81ULL },
	.optlo = { 0xFF9F4929D6B66000ULL, 0xF7974121DEBE6808ULL },
	.opthi = { 0x01EDBD5150BCEC00ULL, 0xE10D5DB1B05C0CE0ULL },
};

static void aes_encrypt_key128_SSSE3(struct aes_encrypt_ctx *ctx, const void *in)
{
	size_t rounds, n;
	void *k;

	prefetchw(ctx->k);
	prefetch(&aes_consts);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);
	asm(
			"lddqu	    %5, %%xmm0\n\t"	/* load key */
			"movdqa	"str_it(INVLO)"(%4), %%xmm5\n\t"	/* inv */
#  ifndef __i386__
			"movdqa	"str_it(S0F)"(%4), %%xmm9\n\t"	/* 0F */
			"movdqa	"str_it(INVHI)"(%4), %%xmm11\n\t"	/* inva */
#  endif
			"movdqa	"str_it(RCON)"(%4), %%xmm6\n\t"
			/* input transform */
			"movdqa	%%xmm0, %%xmm3\n\t"
			/* shedule transform */
#  ifndef __i386__
			"movdqa	%%xmm9, %%xmm1\n\t"
#  else
			"movdqa	"str_it(S0F)"(%4), %%xmm1\n\t"
#  endif
			"pandn	%%xmm0, %%xmm1\n\t"
			"psrld	    $4, %%xmm1\n\t"
#  ifndef __i386__
			"pand	%%xmm9, %%xmm0\n\t"
#  else
			"pand	"str_it(S0F)"(%4), %%xmm0\n\t"
#  endif
			"movdqa	"str_it(IPTLO)"(%4), %%xmm2\n\t"	/* iptlo */
			"pshufb	%%xmm0, %%xmm2\n\t"
			"movdqa	"str_it(IPTHI)"(%4), %%xmm0\n\t"	/* ipthi */
			"pshufb	%%xmm1, %%xmm0\n\t"
			"pxor	%%xmm2, %%xmm0\n\t"
			"movdqa	%%xmm0, %%xmm7\n\t"
			/* output zeroth round key */
			"movdqa	%%xmm0, (%2)\n\t"
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
			"pxor	"str_it(S63)"(%4), %%xmm7\n\t"
			/* subbytes */
#  ifndef __i386__
			"movdqa	%%xmm9, %%xmm1\n\t"
#  else
			"movdqa	"str_it(S0F)"(%4), %%xmm1\n\t"
#  endif
			"pandn	 %%xmm0, %%xmm1\n\t"
			"psrld	     $4, %%xmm1\n\t"	/* 1 = i */
#  ifndef __i386__
			"pand	%%xmm9, %%xmm0\n\t"	/* 0 = k */
#  else
			"pand	"str_it(S0F)"(%4), %%xmm0\n\t"	/* 0 = k */
#  endif
#  ifndef __i386__
			"movdqa	%%xmm11, %%xmm2\n\t"	/* 2 : a/k */
#  else
			"movdqa	"str_it(INVHI)"(%4), %%xmm2\n\t"	/* 2 : a/k */
#  endif
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
			"movdqa	"str_it(SB1LO)"(%4), %%xmm4\n\t"	/* 4 : sbou */
			"pshufb  %%xmm2,  %%xmm4\n\t"		/* 4 = sbou */
			"movdqa	"str_it(SB1HI)"(%4), %%xmm0\n\t"	/* 0 : sbot */
			"pshufb  %%xmm3, %%xmm0\n\t"	/* 0 = sb1t */
			"pxor	%%xmm4, %%xmm0\n\t"	/* 0 = sbox output */
			/* add in smeared stuff */
			"pxor	%%xmm7, %%xmm0\n\t"
			"movdqa	%%xmm0, %%xmm7\n\t"
			"dec	%0\n\t"
			"jz 	2f\n\t"
			/* write output */
			"movdqa	%%xmm0, %%xmm2\n\t"	/* save xmm0 for later */
			"movdqa	"str_it(MCF)"(%4),%%xmm1\n\t"
			"add	$16, %2\n\t"
			"pxor	"str_it(S63)"(%4), %%xmm2\n\t"
			"pshufb	%%xmm1, %%xmm2\n\t"
			"movdqa	%%xmm2, %%xmm3\n\t"
			"pshufb	%%xmm1, %%xmm2\n\t"
			"pxor	%%xmm2, %%xmm3\n\t"
			"pshufb	%%xmm1, %%xmm2\n\t"
			"pxor	%%xmm2, %%xmm3\n\t"
			"pshufb	"str_it(SR)"(%1,%4),%%xmm3\n\t"
			"add	  $-16, %1\n\t"
			"and	   $48, %1\n\t"
			"movdqa	%%xmm3, (%2)\n\t"
			"jmp 	1b\n"
			"2:\n\t"
			/* schedule last round key from xmm0 */
			"pshufb	"str_it(SR)"(%1,%4), %%xmm0\n\t"	/* output permute */
			"add	$16, %2\n\t"
			"pxor	"str_it(S63)"(%4), %%xmm0\n\t"
#  ifndef __i386__
			"movdqa	%%xmm9, %%xmm1\n\t"
#  else
			"movdqa	"str_it(S0F)"(%4), %%xmm1\n\t"
#  endif
			"pandn	%%xmm0, %%xmm1\n\t"
			"psrld	    $4, %%xmm1\n\t"
#  ifndef __i386__
			"pand	%%xmm9, %%xmm0\n\t"	/* 0 = k */
#  else
			"pand	"str_it(S0F)"(%4), %%xmm0\n\t"	/* 0 = k */
#  endif
			"movdqa	"str_it(OPTLO)"(%4), %%xmm2\n\t"	/* optlo */
			"pshufb	%%xmm0, %%xmm2\n\t"
			"movdqa	"str_it(OPTHI)"(%4), %%xmm0\n\t"	/* opthi */
			"pshufb	%%xmm1, %%xmm0\n\t"
			"pxor	%%xmm2, %%xmm0\n\t"
			"movdqa	%%xmm0, (%2)\n\t"	/* save last key */
		: /* %0 */ "=&r" (rounds),
		  /* %1 */ "=&r" (n),
		  /* %2 */ "=&r" (k),
		  /* %3 */ "=m" (ctx->k[0])
		: /* %4 */ "r" (&aes_consts),
		  /* %5 */ "m" (*(const int *)in),
		  /* %6 */ "0" (10),
		  /* %7 */ "1" (16),
		  /* %8 */ "2" (&ctx->k[0])
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#   ifndef __i386__
		  , "xmm9", "xmm11"
#   endif
#  endif
	);
}
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
static GCC_ATTR_OPTIMIZE(3) void aes_ecb_encrypt_padlock(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	static const uint32_t control_word[4] GCC_ATTR_ALIGNED(16) =
		{PL_R128 | PL_KEYGN | PL_K128, 0, 0, 0};
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
		  /* %4 */ "=m" (*(char *)out)
		: /* %4 */ "d" (control_word),
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
			"mov	%4, "REG_S"\n\t"
			"mov	%4, "REG_D"\n\t"
			"mov	%1, "REG_d"\n\t"
			"mov	%2, "REG_b"\n\t"
			"mov	$1, "REG_c"\n\t"
			"rep xcryptecb\n\t"
			"pop	"REG_b"\n\t"
			"pop	"REG_d"\n\t"
			"pop	"REG_c"\n\t"
			"pop	"REG_D"\n\t"
			"pop	"REG_S"\n\t"
		: /* %0 */ "=m" (*(char *)out)
		: /* %1 */ "m" (*control_word),
		  /* %2 */ "m" (ptr),
		  /* %4 */ "m" (in),
		  /* %4 */ "m" (out),
		  /* %5 */ "m" (*(const char *)in)
	);
#endif
}

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
static void aes_ecb_encrypt_SSEAES(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	size_t k;

	asm(
			"movups	%2, %%xmm0\n\t"	/* input */
			"movaps	(%0), %%xmm1\n\t"	/* key */
			"pxor	%%xmm1, %%xmm0\n\t"	/* round 0 */
			"lea	0x30(%0), %0\n\t"	/* prime key address */
			"movaps	-0x20(%0), %%xmm1\n\t"	/* key key */
			/* aesenc %xmm1, %xmm0 */	/* round */
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	-0x10(%0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	(%0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x10(%0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x20(%0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x30(%0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x40(%0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x50(%0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x60(%0), %%xmm1\n\t"
			".byte	0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
			"movaps	0x70(%0), %%xmm1\n\t"
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
static void aes_ecb_encrypt_SSSE3(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	size_t rounds, n;
	void *k;

	prefetchw(out);
	prefetch(ctx->k);
	prefetch(&aes_consts);
	prefetch(aes_consts.mcf);
	prefetch(aes_consts.sr);
	asm(
			"lddqu	%5, %%xmm0\n\t"
			"movdqa	"str_it(S0F)"(%4), %%xmm6\n\t"	/* 0F */
			"movdqa	"str_it(INVLO)"(%4), %%xmm5\n\t"	/* inv */
			"movdqa	"str_it(INVHI)"(%4), %%xmm7\n\t"	/* inva */
#  ifndef __i386__
			"movdqa	"str_it(SB1LO)"(%4), %%xmm13\n\t"	/* sb1u */
			"movdqa	"str_it(SB1HI)"(%4), %%xmm12\n\t"	/* sb1t */
			"movdqa	"str_it(SB2LO)"(%4), %%xmm15\n\t"	/* sb2u */
			"movdqa	"str_it(SB2HI)"(%4), %%xmm14\n\t"	/* sb2t */
#  endif
			"movdqa	"str_it(IPTLO)"(%4), %%xmm2\n\t"	/* iptlo */
			"movdqa	 %%xmm6, %%xmm1\n\t"
			"pandn	 %%xmm0, %%xmm1\n\t"
			"psrld	     $4, %%xmm1\n\t"
			"pand	 %%xmm6, %%xmm0\n\t"
			"pshufb	 %%xmm0, %%xmm2\n\t"
			"movdqa	"str_it(IPTHI)"(%4), %%xmm0\n\t"	/* ipthi */
			"pshufb	 %%xmm1, %%xmm0\n\t"
			"pxor	 16(%2), %%xmm2\n\t"
			"pxor	 %%xmm2, %%xmm0\n\t"
			"add	    $32, %2\n\t"
			"jmp	     2f\n\t"
			".p2align 4\n"
			"1:\n\t"
		/* middle of middle round */
#  ifndef __i386__
			"movdqa	%%xmm13, %%xmm4\n\t"	/* 4 : sb1u */
#  else
			"movdqa	"str_it(SB1LO)"(%4), %%xmm4\n\t"	/* 4 : sb1u */
#  endif
			"pshufb	 %%xmm2, %%xmm4\n\t"	/* 4 = sb1u */
			"pxor	   (%2), %%xmm4\n\t"	/* 4 = sb1u + k */
#  ifndef __i386__
			"movdqa	%%xmm12, %%xmm0\n\t"	/* 0 : sb1t */
#  else
			"movdqa	"str_it(SB1HI)"(%4), %%xmm0\n\t"	/* 0: sb1t */
#  endif
			"pshufb	 %%xmm3, %%xmm0\n\t"	/* 0 = sb1t */
			"pxor	 %%xmm4, %%xmm0\n\t"	/* 0 = A */
#  ifndef __i386__
			"movdqa	%%xmm15, %%xmm4\n\t"	/* 4 : sb2u */
#  else
			"movdqa	"str_it(SB2LO)"(%4), %%xmm4\n\t"	/* 4 : sb2u */
#  endif
			"pshufb	 %%xmm2, %%xmm4\n\t"	/* 4 = sb2u */
			"movdqa	"str_it(MCF)"(%1,%4), %%xmm1\n\t"
#  ifndef __i386__
			"movdqa	%%xmm14, %%xmm2\n\t"	/* 2 : sb2t */
#  else
			"movdqa	"str_it(SB2HI)"(%4), %%xmm2\n\t"	/* 2 : sb2t */
#  endif
			"pshufb	 %%xmm3, %%xmm2\n\t"	/* 2 = sb2t */
			"pxor	 %%xmm4, %%xmm2\n\t"	/* 2 = 2A */
			"movdqa	 %%xmm0, %%xmm3\n\t"	/* 3 = A */
			"pshufb	 %%xmm1, %%xmm0\n\t"	/* 0 = B */
			"pxor	 %%xmm2, %%xmm0\n\t"	/* 0 = 2A+B */
			"pshufb	"str_it(MCB)"(%1,%4), %%xmm3\n\t"	/* 3 = D */
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
			"movdqa	"str_it(SBOLO)"(%4), %%xmm4\n\t"	/* 3 : sbou */
			"pshufb	 %%xmm2, %%xmm4\n\t"	/* 4 = sbou */
			"pxor	   (%2), %%xmm4\n\t"	/* 4 = sb1u + k */
			"movdqa	"str_it(SBOHI)"(%4), %%xmm0\n\t"	/* 0 : sbot */
			"pshufb	 %%xmm3, %%xmm0\n\t"	/* 0 = sb1t */
			"pxor	 %%xmm4, %%xmm0\n\t"	/* 0 = A */
			"pshufb	"str_it(SR)"(%1,%4), %%xmm0\n\t"
			"movdqu	%%xmm0, %3\n\t"
		: /* %0 */ "=&r" (rounds),
		  /* %1 */ "=&r" (n),
		  /* %2 */ "=&r" (k),
		  /* %3 */ "=m" (*(int *)out)
		: /* %4 */ "r" (&aes_consts),
		  /* %5 */ "m" (*(const int *)in),
		  /* %6 */ "0" (10 - 1),
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
# endif
#endif

#define ARCH_NAME_SUFFIX _generic
#include "../generic/aes.c"

static __init_cdata const struct test_cpu_feature tfeat_aes_encrypt_key128[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))aes_encrypt_key128_SSEAES,  .features = {[1] = CFB(CFEATURE_AES)}},
	{.func = (void (*)(void))aes_encrypt_key128_SSSE3,   .features = {[1] = CFB(CFEATURE_SSSE3)}},
# endif
#endif
	{.func = (void (*)(void))aes_encrypt_key128_generic, .features = {}, .flags = CFF_DEFAULT},
};

static __init_cdata const struct test_cpu_feature tfeat_aes_ecb_encrypt[] =
{
	{.func = (void (*)(void))aes_ecb_encrypt_padlock,  .features = {[5] = CFB(CFEATURE_PL_ACE_E)}},
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))aes_ecb_encrypt_SSEAES,   .features = {[1] = CFB(CFEATURE_AES)}},
	{.func = (void (*)(void))aes_ecb_encrypt_SSSE3,    .features = {[1] = CFB(CFEATURE_SSSE3)}},
# endif
#endif
	{.func = (void (*)(void))aes_ecb_encrypt_generic,  .features = {}, .flags = CFF_DEFAULT},
};

DYN_JMP_DISPATCH_NR(aes_encrypt_key128, (struct aes_encrypt_ctx *ctx, const void *in), (ctx, in))
DYN_JMP_DISPATCH_NR(aes_ecb_encrypt, (const struct aes_encrypt_ctx *ctx, void *out, const void *in), (ctx, out, in))

/*@unused@*/
static char const rcsid_aesx[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
