/*
 * aes.c
 * AES routines, x86 implementation
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
 * $Id:$
 */

#include "../my_bitops.h"
#include "x86_features.h"

static void aes_encrypt_key128_generic(struct aes_encrypt_ctx *, const void *);
static void aes_ecb_encrypt_generic(const struct aes_encrypt_ctx *, void *, const void *);
static void aes_ecb_encrypt_padlock(const struct aes_encrypt_ctx *, void *, const void *);

static void aes_encrypt_key128_SSEAES(struct aes_encrypt_ctx *ctx, const void *in)
{
	size_t k;

	asm(
			".subsection 2\n\t"
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
			".previous\n\t"
			"movups	(%2), %%xmm0\n\t"	/* user key (first 16 bytes) */
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
		: /* %2 */ "r" (in),
		  /* %4 */ "m" (*(const char *)in)
#ifdef __SSE__
		: "xmm0", "xmm1", "xmm4"
#endif
	);
}

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
		"\txchg	%5, %%ebx\n"
#endif
		: /* %0 */ "=S" (S),
		  /* %1 */ "=D" (D),
		  /* %2 */ "=c" (c),
		  /* %3 */ "=m" (*(char *)out)
		: /* %4 */ "d" (control_word),
#if defined(__i386__) && defined(__PIC__)
		  /* %5 */ "rm" (&ctx->k),
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
		  /* %3 */ "m" (in),
		  /* %4 */ "m" (out),
		  /* %5 */ "m" (*(const char *)in)
	);
#endif
}

static void aes_ecb_encrypt_SSEAES(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	size_t k;

	asm(
			"movups	(%2), %%xmm0\n\t"	/* input */
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
			"movups	%%xmm0, (%3)"	/* output */
		: /* %0 */ "=r" (k),
		  /* %1 */ "=m" (*(char *)out)
		: /* %2 */ "r" (in),
		  /* %3 */ "r" (out),
		  /* %4 */ "0" (&ctx->k),
		  /* %5 */ "m" (*(const char *)in)
#ifdef __SSE__
		: "xmm0", "xmm1"
#endif
	);
}

#define ARCH_NAME_SUFFIX _generic
#include "../generic/aes.c"

static const struct test_cpu_feature key_feat[] =
{
	{.func = (void (*)(void))aes_encrypt_key128_SSEAES, .flags_needed = CFEATURE_AES},
	{.func = (void (*)(void))aes_encrypt_key128_generic, .flags_needed = -1 },
};

static const struct test_cpu_feature enc_feat[] =
{
	{.func = (void (*)(void))aes_ecb_encrypt_padlock, .flags_needed = CFEATURE_PL_ACE_E},
	{.func = (void (*)(void))aes_ecb_encrypt_SSEAES, .flags_needed = CFEATURE_AES},
	{.func = (void (*)(void))aes_ecb_encrypt_generic, .flags_needed = -1 },
};


static void aes_encrypt_key128_runtime_sw(struct aes_encrypt_ctx *ctx, const void *in);
static void aes_ecb_encrypt_runtime_sw(const struct aes_encrypt_ctx *ctx, void *out, const void *in);

/*
 * Func ptr
 */
static void (*aes_encrypt_key128_ptr)(struct aes_encrypt_ctx *ctx, const void *in) = aes_encrypt_key128_runtime_sw;
static void (*aes_ecb_encrypt_ptr)(const struct aes_encrypt_ctx *ctx, void *out, const void *in) = aes_ecb_encrypt_runtime_sw;

/*
 * constructor
 */
static void aes_select(void) GCC_ATTR_CONSTRUCT;
static void aes_select(void)
{
	aes_encrypt_key128_ptr = test_cpu_feature(key_feat, anum(key_feat));
	aes_ecb_encrypt_ptr = test_cpu_feature(enc_feat, anum(enc_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer failes
 */
static void aes_encrypt_key128_runtime_sw(struct aes_encrypt_ctx *ctx, const void *in)
{
	aes_select();
	aes_encrypt_key128(ctx, in);
}

static void aes_ecb_encrypt_runtime_sw(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	aes_select();
	aes_ecb_encrypt(ctx, out, in);
}

/*
 * trampoline
 */
void aes_encrypt_key128(struct aes_encrypt_ctx *ctx, const void *in)
{
	aes_encrypt_key128_ptr(ctx, in);
}

void aes_ecb_encrypt(const struct aes_encrypt_ctx *ctx, void *out, const void *in)
{
	aes_ecb_encrypt_ptr(ctx, out, in);
}

/*@unused@*/
static char const rcsid_aesx[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
