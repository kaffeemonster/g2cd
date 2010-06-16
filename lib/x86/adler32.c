/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   x86 implementation
 * Copyright (C) 1995-2004 Mark Adler
 * Copyright (C) 2009 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/*
 * This is not the original adler32.c from the zlib distribution,
 * but a heavily modified version. If you are looking for the
 * original, please go to zlib.net.
 */

#include "x86_features.h"

#define BASE 65521UL    /* largest prime smaller than 65536 */
/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */
#define NMAX 5552

/* use NO_DIVIDE if your processor does not do division in hardware */
#ifdef NO_DIVIDE
# define MOD(a) \
	do { \
		if (a >= (BASE << 16)) a -= (BASE << 16); \
		if (a >= (BASE << 15)) a -= (BASE << 15); \
		if (a >= (BASE << 14)) a -= (BASE << 14); \
		if (a >= (BASE << 13)) a -= (BASE << 13); \
		if (a >= (BASE << 12)) a -= (BASE << 12); \
		if (a >= (BASE << 11)) a -= (BASE << 11); \
		if (a >= (BASE << 10)) a -= (BASE << 10); \
		if (a >= (BASE << 9)) a -= (BASE << 9); \
		if (a >= (BASE << 8)) a -= (BASE << 8); \
		if (a >= (BASE << 7)) a -= (BASE << 7); \
		if (a >= (BASE << 6)) a -= (BASE << 6); \
		if (a >= (BASE << 5)) a -= (BASE << 5); \
		if (a >= (BASE << 4)) a -= (BASE << 4); \
		if (a >= (BASE << 3)) a -= (BASE << 3); \
		if (a >= (BASE << 2)) a -= (BASE << 2); \
		if (a >= (BASE << 1)) a -= (BASE << 1); \
		if (a >= BASE) a -= BASE; \
	} while (0)
# define MOD4(a) \
	do { \
		if (a >= (BASE << 4)) a -= (BASE << 4); \
		if (a >= (BASE << 3)) a -= (BASE << 3); \
		if (a >= (BASE << 2)) a -= (BASE << 2); \
		if (a >= (BASE << 1)) a -= (BASE << 1); \
		if (a >= BASE) a -= BASE; \
	} while (0)
#else
# define MOD(a) a %= BASE
# define MOD4(a) a %= BASE
#endif

#if 0
		/*
		 * Will XOP processors have SSSE3/AVX??
		 */
		if(k >= 16)
		{
			static const char vord[] GCC_ATTR_ALIGNED(16) = {16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1};
			asm(
				"prefetchnta	16(%0)\n\t"
				"movdqa	%4, %%xmm4\n\t"
				"movd	%1, %%xmm3\n\t"
				"movd	%2, %%xmm2\n\t"
				".p2align 3,,3\n\t"
				".p2align 2\n"
				"1:\n\t"
				"lddqu	(%0), %%xmm0\n\t"
				"prefetchnta	32(%0)\n\t"
				"sub	$16, %3\n\t"
				"add	$16, %0\n\t"
				"cmp	$15, %3\n\t"
				"vpslld	$4, %%xmm3, %%xmm1\n\t"
				"vphaddubd	%%xmm0, %%xmm5\n\t" /* A */
				"vpmaddubsw %%xmm4, %%xmm0, %%xmm0\n\t"/* AVX! */ /* 1 */
				"vpaddd	%%xmm1, %%xmm2, %%xmm2\n\t" /* vpmacudd w. mul = 16 */
				"vphadduwd	%%xmm0, %%xmm0\n\t" /* 2 */
				"vpaddd	%%xmm5, %%xmm3, %%xmm3\n\t" /* B: A+B = hadd+acc or vpmadcubd w. mul = 1 */
				"vpaddd	%%xmm0, %%xmm2, %%xmm2\n\t" /* 3: 1+2+3 = vpmadcubd w. mul = 16,15,14... */
				"jg	1b\n\t"
				/* do the modulo SIMD, stay longer in loop  */
				/* (u64) t = x * 0x80078071U
				 * t = t >> 32 >> 0xf; // pmulhud + psrld $15
				 * (i32)t *= 0xfff1;
				 * x -= t; // vpmsubudd
				 */
				"vphaddudq	%%xmm2, %%xmm0\n\t"
				"vphaddudq	%%xmm3, %%xmm1\n\t"
				"pshufd	$0xE6, %%xmm0, %%xmm2\n\t"
				"pshufd	$0xE6, %%xmm1, %%xmm3\n\t"
				"paddd	%%xmm0, %%xmm2\n\t"
				"paddd	%%xmm1, %%xmm3\n\t"
				"movd	%%xmm2, %2\n\t"
				"movd	%%xmm3, %1\n\t"
			: /* %0 */ "=r" (buf),
			  /* %1 */ "=r" (s1),
			  /* %2 */ "=r" (s2),
			  /* %3 */ "=r" (k)
			: /* %4 */ "m" (vord[0]),
			  /*    */ "0" (buf),
			  /*    */ "1" (s1),
			  /*    */ "2" (s2),
			  /*    */ "3" (k)
#ifdef __SSE__
			: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
			);
		}
#endif

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
static uint32_t adler32_SSSE3(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	unsigned k;

	if(!buf)
		return 1L;

	while(likely(len))
	{
		k = len < NMAX ? len : NMAX;
		len -= k;

		if(k >= 16)
		{
			static const char vord[] GCC_ATTR_ALIGNED(16) = {16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1};
			unsigned l;
			asm(
				"prefetchnta	16(%0)\n\t"
				"movdqa	%5, %%xmm5\n\t"
				"movd %2, %%xmm2\n\t"
				"movd %1, %%xmm3\n\t"
				"pxor	%%xmm4, %%xmm4\n\t"
				"pxor	%%xmm7, %%xmm7\n\t"
				".p2align 3,,3\n\t"
				".p2align 2\n"
				"2:\n\t"
				"mov	$128, %4\n\t"
				"cmp	%4, %3\n\t"
				"cmovb	%3, %4\n\t"
				"and	$-16, %4\n\t"
				"sub	%4, %3\n\t"
				"shr	$4, %4\n\t"
				"pxor	%%xmm6, %%xmm6\n\t"
				".p2align 3,,3\n\t"
				".p2align 2\n"
				"1:\n\t"
				"lddqu	(%0), %%xmm0\n\t"
				"prefetchnta	64(%0)\n\t"
				"paddd	%%xmm3, %%xmm7\n\t"
				"add	$16, %0\n\t"
				"dec	%4\n\t"
				"movdqa	%%xmm0, %%xmm1\n\t"
				"pmaddubsw %%xmm5, %%xmm0\n\t"
				"psadbw	%%xmm4, %%xmm1\n\t"
				"paddw	%%xmm0, %%xmm6\n\t"
				"paddd	%%xmm1, %%xmm3\n\t"
				"jnz	1b\n\t"
				"movdqa	%%xmm6, %%xmm0\n\t"
				"punpckhwd	%%xmm4, %%xmm0\n\t"
				"punpcklwd	%%xmm4, %%xmm6\n\t"
				"paddd	%%xmm0, %%xmm2\n\t"
				"paddd	%%xmm6, %%xmm2\n\t"
				"cmp	$15, %3\n\t"
				"jg	2b\n\t"
				"pslld	$4, %%xmm7\n\t"
				"paddd	%%xmm7, %%xmm2\n\t"
				"pshufd	$0xEE, %%xmm3, %%xmm1\n\t"
				"pshufd	$0xEE, %%xmm2, %%xmm0\n\t"
				"paddd	%%xmm3, %%xmm1\n\t"
				"paddd	%%xmm2, %%xmm0\n\t"
				"pshufd	$0xE5, %%xmm0, %%xmm2\n\t"
				"paddd	%%xmm0, %%xmm2\n\t"
				"movd	%%xmm1, %1\n\t"
				"movd	%%xmm2, %2\n\t"
			: /* %0 */ "=r" (buf),
			  /* %1 */ "=r" (s1),
			  /* %2 */ "=r" (s2),
			  /* %3 */ "=r" (k),
			  /* %4 */ "=r" (l)
			: /* %5 */ "m" (vord[0]),
			  /*    */ "0" (buf),
			  /*    */ "1" (s1),
			  /*    */ "2" (s2),
			  /*    */ "3" (k)
#  ifdef __SSE__
			: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#  endif
			);
		}

		if(unlikely(k))
		{
			uint32_t t;
			unsigned n = k % 16;
			buf -= 16 - n;
			k /= 16;

			asm(
#  ifdef __i386__
#   ifndef __PIC__
#    define CLOB
				"lea	1f(,%5,8), %4\n\t"
#   else
#    define CLOB "&"
				"call	i686_get_pc\n\t"
				"lea	1f-.(%4,%5,8), %4\n\t"
				".subsection 2\n"
				"i686_get_pc:\n\t"
				"movl	(%%esp), %4\n\t"
				"ret\n\t"
				".previous\n\t"
#   endif
				"jmp	*%4\n\t"
#  else
#   define CLOB "&"
				"lea	1f(%%rip), %q4\n\t"
				"lea	(%q4,%q5,8), %q4\n\t"
				"jmp	*%q4\n\t"
#  endif
				".p2align 1\n"
				"1:\n\t"
			/* 128 */
				"movzbl	(%2), %4\n\t"	/* 3 */
				"dec	%3\n\t"	/* 1 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/* 120 */
				"movzbl	0x1(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/* 112 */
				"movzbl	0x2(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/* 104 */
				"movzbl	0x3(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  96 */
				"movzbl	0x4(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  88 */
				"movzbl	0x5(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  80 */
				"movzbl	0x6(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  72 */
				"movzbl	0x7(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  64 */
				"movzbl	0x8(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  56 */
				"movzbl	0x9(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  48 */
				"movzbl	0xa(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  40 */
				"movzbl	0xb(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  32 */
				"movzbl	0xc(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  24 */
				"movzbl	0xd(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  16 */
				"movzbl	0xe(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*   8 */
				"movzbl	0xf(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*   0 */
				"add	$0x10, %2\n\t"
				"test	%3, %3\n\t"
				"jne	1b"
			: /* %0 */ "=R" (s1),
			  /* %1 */ "=R" (s2),
			  /* %2 */ "=abdSD" (buf),
			  /* %3 */ "=c" (k),
			  /* %4 */ "="CLOB"R" (t)
			: /* %5 */ "r" (16 - n),
			  /*    */ "0" (s1),
			  /*    */ "1" (s2),
			  /*    */ "2" (buf),
			  /*    */ "3" (k)
			);
		}
		MOD(s1);
		MOD(s2);
	}
	return (s2 << 16) | s1;
}
# endif
#endif

static uint32_t adler32_SSE2(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	unsigned k;

	/* initial Adler-32 value */
	if(buf == NULL)
		return 1L;

	while(likely(len))
	{
		k = len < NMAX ? len : NMAX;
		len -= k;

		if(k >= 16)
		{
			static const short vord_l[] GCC_ATTR_ALIGNED(16) = {16,15,14,13,12,11,10,9};
			static const short vord_h[] GCC_ATTR_ALIGNED(16) = {8,7,6,5,4,3,2,1};

			asm(
				"prefetchnta	16(%0)\n\t"
				"movdqa	%4, %%xmm6\n\t"
				"movdqa	%5, %%xmm5\n\t"
				"movd	%1, %%xmm4\n\t"
				"movd	%2, %%xmm3\n\t"
				"pxor	%%xmm7, %%xmm7\n\t"
				".p2align 3,,3\n\t"
				".p2align 2\n"
				"1:\n\t"
				"prefetchnta	32(%0)\n\t"
				"movdqu	(%0), %%xmm1\n\t"
				"sub	$16, %3\n\t"
				"movdqa	%%xmm4, %%xmm2\n\t"
				"add	$16, %0\n\t"
				"movdqa	%%xmm1, %%xmm0\n\t"
				"cmp	$15, %3\n\t"
				"pslld	$4, %%xmm2\n\t"
				"paddd	%%xmm3, %%xmm2\n\t"
				"psadbw	%%xmm7, %%xmm0\n\t"
				"paddd	%%xmm0, %%xmm4\n\t"
				"movdqa	%%xmm1, %%xmm0\n\t"
				"punpckhbw	%%xmm7, %%xmm1\n\t"
				"punpcklbw	%%xmm7, %%xmm0\n\t"
				"movdqa	%%xmm1, %%xmm3\n\t"
				"pmaddwd	%%xmm6, %%xmm0\n\t"
				"paddd	%%xmm2, %%xmm0\n\t"
				"pmaddwd	%%xmm5, %%xmm3\n\t"
				"paddd	%%xmm0, %%xmm3\n\t"
				"jg	1b\n\t"
				"pshufd	$0xEE, %%xmm3, %%xmm0\n\t"
				"pshufd	$0xEE, %%xmm4, %%xmm1\n\t"
				"paddd	%%xmm3, %%xmm0\n\t"
				"pshufd	$0xE5, %%xmm0, %%xmm2\n\t"
				"paddd	%%xmm4, %%xmm1\n\t"
				"movd	%%xmm1, %1\n\t"
				"paddd	%%xmm0, %%xmm2\n\t"
				"movd	%%xmm2, %2\n\t"
			: /* %0 */ "=r" (buf),
			  /* %1 */ "=r" (s1),
			  /* %2 */ "=r" (s2),
			  /* %3 */ "=r" (k)
			: /* %4 */ "m" (vord_l[0]),
			  /* %5 */ "m" (vord_h[0]),
			  /*    */ "0" (buf),
			  /*    */ "1" (s1),
			  /*    */ "2" (s2),
			  /*    */ "3" (k)
#ifdef __SSE__
			: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
			);
		}

		if(unlikely(k))
		{
			uint32_t t;
			unsigned n = k % 16;
			buf -= 16 - n;
			k /= 16;

			asm(
#ifdef __i386__
# ifndef __PIC__
# define CLOB
				"lea	1f(,%5,8), %4\n\t"
# else
# define CLOB "&"
				"call	i686_get_pc\n\t"
				"lea	1f-.(%4,%5,8), %4\n\t"
				".subsection 2\n"
				"i686_get_pc:\n\t"
				"movl	(%%esp), %4\n\t"
				"ret\n\t"
				".previous\n\t"
# endif
				"jmp	*%4\n\t"
#else
# define CLOB "&"
				"lea	1f(%%rip), %q4\n\t"
				"lea	(%q4,%q5,8), %q4\n\t"
				"jmp	*%q4\n\t"
#endif
				".p2align 1\n"
				"1:\n\t"
			/* 128 */
				"movzbl	(%2), %4\n\t"	/* 3 */
				"dec	%3\n\t"	/* 1 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/* 120 */
				"movzbl	0x1(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/* 112 */
				"movzbl	0x2(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/* 104 */
				"movzbl	0x3(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  96 */
				"movzbl	0x4(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  88 */
				"movzbl	0x5(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  80 */
				"movzbl	0x6(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  72 */
				"movzbl	0x7(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  64 */
				"movzbl	0x8(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  56 */
				"movzbl	0x9(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  48 */
				"movzbl	0xa(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  40 */
				"movzbl	0xb(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  32 */
				"movzbl	0xc(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  24 */
				"movzbl	0xd(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  16 */
				"movzbl	0xe(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*   8 */
				"movzbl	0xf(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*   0 */
				"add	$0x10, %2\n\t"
				"test	%3, %3\n\t"
				"jne	1b"
			: /* %0 */ "=R" (s1),
			  /* %1 */ "=R" (s2),
			  /* %2 */ "=abdDS" (buf),
			  /* %3 */ "=c" (k),
			  /* %4 */ "="CLOB"R" (t)
			: /* %5 */ "r" (16 - n),
			  /*    */ "0" (s1),
			  /*    */ "1" (s2),
			  /*    */ "2" (buf),
			  /*    */ "3" (k)
			);
		}
		MOD(s1);
		MOD(s2);
	}
	return (s2 << 16) | s1;
}

#ifndef __x86_64__
static uint32_t adler32_MMX(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	unsigned k;

	/* initial Adler-32 value */
	if(buf == NULL)
		return 1L;

	while(likely(len))
	{
		k = len < NMAX ? len : NMAX;
		len -= k;

		if(k >= 8)
		{
			static const short v1_4[] GCC_ATTR_ALIGNED(8)   = {1,1,1,1};
			static const short vord_l[] GCC_ATTR_ALIGNED(8) = {8,7,6,5};
			static const short vord_h[] GCC_ATTR_ALIGNED(8) = {4,3,2,1};
			uint32_t t;

			asm(
				"movd	%1, %%mm4\n\t"
				"movd	%2, %%mm1\n\t"
				"movq	%5, %%mm5\n\t"
				"movq	%6, %%mm7\n\t"
				"movq	%7, %%mm6\n\t"
				".p2align 3,,3\n\t"
				".p2align 2\n"
				"1:\n\t"
				"pxor	%%mm2, %%mm2\n\t"
				"movq	%%mm4, %%mm3\n\t"
				"subl	$8, %3\n\t"
				"pslld	$3, %%mm3\n\t"
				"paddd	%%mm1, %%mm3\n\t"
				"movq	(%0), %%mm1\n\t"
				"addl	$8, %0\n\t"
				"movq	%%mm1, %%mm0\n\t"
				"cmpl	$7, %3\n\t"
				"punpckhbw	%%mm2, %%mm1\n\t"
				"punpcklbw	%%mm2, %%mm0\n\t"
				"movq	%%mm0, %%mm2\n\t"
				"pmaddwd	%%mm7, %%mm0\n\t"
				"pmaddwd	%%mm5, %%mm2\n\t"
				"paddd	%%mm3, %%mm0\n\t"
				"paddd	%%mm4, %%mm2\n\t"
				"movq	%%mm1, %%mm4\n\t"
				"pmaddwd	%%mm6, %%mm1\n\t"
				"pmaddwd	%%mm5, %%mm4\n\t"
				"paddd	%%mm0, %%mm1\n\t"
				"paddd	%%mm2, %%mm4\n\t"
				"jg	1b\n\t"
				"movd	%%mm4, %1\n\t"
				"psrlq	$32, %%mm4\n\t"
				"movd	%%mm1, %2\n\t"
				"psrlq	$32, %%mm1\n\t"
				"movd	%%mm4, %4\n\t"
				"add	%4, %1\n\t"
				"movd	%%mm1, %4\n\t"
				"add	%4, %2\n\t"
			: /* %0 */ "=r" (buf),
			  /* %1 */ "=r" (s1),
			  /* %2 */ "=r" (s2),
			  /* %3 */ "=r" (k),
			  /* %4 */ "=r" (t)
			: /* %5 */ "m" (v1_4[0]),
			  /* %6 */ "m" (vord_l[0]),
			  /* %7 */ "m" (vord_h[0]),
			  /*    */ "0" (buf),
			  /*    */ "1" (s1),
			  /*    */ "2" (s2),
			  /*    */ "3" (k)
#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
#endif
			);
		}

		if(unlikely(k))
		{
			uint32_t t;
			unsigned n = k % 16;
			buf -= 16 - n;
			k /= 16;

			asm(
#ifdef __i386__
# ifndef __PIC__
# define CLOB
				"lea	1f(,%5,8), %4\n\t"
# else
# define CLOB "&"
				"call	i686_get_pc\n\t"
				"lea	1f-.(%4,%5,8), %4\n\t"
				".subsection 2\n"
				"i686_get_pc:\n\t"
				"movl	(%%esp), %4\n\t"
				"ret\n\t"
				".previous\n\t"
# endif
				"jmp	*%4\n\t"
#else
# define CLOB "&"
				"lea	1f(%%rip), %q4\n\t"
				"lea	(%q4,%q5,8), %q4\n\t"
				"jmp	*%q4\n\t"
#endif
				".p2align 1\n"
				"1:\n\t"
			/* 128 */
				"movzbl	(%2), %4\n\t"	/* 3 */
				"dec	%3\n\t"	/* 1 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/* 120 */
				"movzbl	0x1(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/* 112 */
				"movzbl	0x2(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/* 104 */
				"movzbl	0x3(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  96 */
				"movzbl	0x4(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  88 */
				"movzbl	0x5(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  80 */
				"movzbl	0x6(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  72 */
				"movzbl	0x7(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  64 */
				"movzbl	0x8(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  56 */
				"movzbl	0x9(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  48 */
				"movzbl	0xa(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  40 */
				"movzbl	0xb(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  32 */
				"movzbl	0xc(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  24 */
				"movzbl	0xd(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*  16 */
				"movzbl	0xe(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*   8 */
				"movzbl	0xf(%2), %4\n\t"	/* 4 */
				"add	%4, %0\n\t"	/* 2 */
				"add	%0, %1\n\t"	/* 2 */
			/*   0 */
				"add	$0x10, %2\n\t"
				"test	%3, %3\n\t"
				"jne	1b"
			: /* %0 */ "=R" (s1),
			  /* %1 */ "=R" (s2),
			  /* %2 */ "=abdSD" (buf),
			  /* %3 */ "=c" (k),
			  /* %4 */ "="CLOB"R" (t)
			: /* %5 */ "r" (16 - n),
			  /*    */ "0" (s1),
			  /*    */ "1" (s2),
			  /*    */ "2" (buf),
			  /*    */ "3" (k)
			);
		}
		MOD(s1);
		MOD(s2);
	}
	return (s2 << 16) | s1;
}
#endif

static uint32_t adler32_x86(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	unsigned n;

	/* initial Adler-32 value */
	if(buf == NULL)
		return 1L;

	while(likely(len))
	{
#ifndef __x86_64__
# define LOOP_COUNT 4
#else
# define LOOP_COUNT 8
#endif
		unsigned k;
		n = len < NMAX ? len : NMAX;
		len -= n;
		k = n / LOOP_COUNT;
		n %= LOOP_COUNT;

		if(likely(k)) do
		{
			/*
			 * Modern compiler can do "wonders".
			 * It only sucks if they "trick them self".
			 * This was unrolled 16 times not because someone
			 * anticipated autovectorizing compiler, but the
			 * classical "avoid loop overhead".
			 *
			 * But things get tricky if the compiler starts to see:
			 * "hey lets disambiguate one sum step from the other",
			 * the classical prevent-pipeline-stalls-thing.
			 *
			 * Suddenly we have 16 temporary sums, which unfortunatly
			 * blows x86 limited register set...
			 *
			 * Loopunrolling is also a little bad for the I-cache.
			 *
			 * So turn this a little bit down for x86.
			 * Instead we try to keep it in the register set. 4 sums fits
			 * into i386 registerset with no framepointer.
			 * x86_64 is a little more splendit, but still we can not
			 * take 16, so take 8 sums.
			 */
			s1 += buf[0]; s2 += s1;
			s1 += buf[1]; s2 += s1;
			s1 += buf[2]; s2 += s1;
			s1 += buf[3]; s2 += s1;
#ifdef __x86_64__
			s1 += buf[4]; s2 += s1;
			s1 += buf[5]; s2 += s1;
			s1 += buf[6]; s2 += s1;
			s1 += buf[7]; s2 += s1;
#endif
			buf  += LOOP_COUNT;
		} while(likely(--k));
		if(n) do {
			s1 += *buf++;
			s2 += s1;
		} while(--n);
		MOD(s1);
		MOD(s2);
	}
	return (s2 << 16) | s1;
}

static const struct test_cpu_feature t_feat[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))adler32_SSSE3, .flags_needed = CFEATURE_SSSE3, .callback = NULL},
# endif
#endif
	{.func = (void (*)(void))adler32_SSE2, .flags_needed = CFEATURE_SSE2, .callback = NULL},
#ifndef __x86_64__
	{.func = (void (*)(void))adler32_MMX, .flags_needed = CFEATURE_MMX, .callback = NULL},
#endif
	{.func = (void (*)(void))adler32_x86, .flags_needed = -1, .callback = NULL},
};

static uint32_t adler32_runtime_sw(uint32_t adler, const uint8_t *buf, unsigned len);
/*
 * Func ptr
 */
static uint32_t (*adler32_ptr)(uint32_t adler, const uint8_t *buf, unsigned len) = adler32_runtime_sw;

/*
 * constructor
 */
static GCC_ATTR_CONSTRUCT void adler32_select(void)
{
	adler32_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructor fails
 */
static uint32_t adler32_runtime_sw(uint32_t adler, const uint8_t *buf, unsigned len)
{
	adler32_select();
	return adler32_ptr(adler, buf, len);
}

/* ========================================================================= */
static noinline uint32_t adler32_common(uint32_t adler, const uint8_t *buf, unsigned len)
{
	/* split Adler-32 into component sums */
	uint32_t sum2 = (adler >> 16) & 0xffff;
	adler &= 0xffff;

	/* in case user likes doing a byte at a time, keep it fast */
	if(len == 1)
	{
		adler += buf[0];
		if(adler >= BASE)
			adler -= BASE;
		sum2 += adler;
		if(sum2 >= BASE)
			sum2 -= BASE;
		return adler | (sum2 << 16);
	}

	/* initial Adler-32 value (deferred check for len == 1 speed) */
	if(buf == NULL)
		return 1L;

	/* in case short lengths are provided, keep it somewhat fast */
	while(len--) {
		adler += *buf++;
		sum2 += adler;
	}
	if(adler >= BASE)
		adler -= BASE;
	MOD4(sum2);	/* only added so many BASE's */
	return adler | (sum2 << 16);
}

uint32_t adler32(uint32_t adler, const uint8_t *buf, unsigned len)
{
	if(len < 16)
		return adler32_common(adler, buf, len);
	return adler32_ptr(adler, buf, len);
}

static char const rcsid_a32g[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
