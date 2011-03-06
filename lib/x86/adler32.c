/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   x86 implementation
 * Copyright (C) 1995-2004 Mark Adler
 * Copyright (C) 2009-2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

/*
 * This is not the original adler32.c from the zlib distribution,
 * but a heavily modified version. If you are looking for the
 * original, please go to zlib.net.
 */

#include "../other.h"
#define HAVE_ADLER32_VEC
#ifndef USE_SIMPLE_DISPATCH
uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len);
#else
static uint32_t adler32_vec(uint32_t adler, const uint8_t *buf, unsigned len);
#endif
#define MIN_WORK 32

#include "../generic/adler32.c"
#include "x86_features.h"

static noinline const uint8_t *adler32_jumped(const uint8_t *buf, uint32_t *s1, uint32_t *s2, unsigned k)
{
	uint32_t t;
	unsigned n = k % 16;
	buf += n;
	k = (k / 16) + 1;

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
		"2:\n\t"
#  ifdef __i386
		".byte 0x3e\n\t"
#  endif
		"add	$0x10, %2\n\t"
		".p2align 1\n"
		"1:\n\t"
	/* 128 */
		"movzbl	-16(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/* 120 */
		"movzbl	-15(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/* 112 */
		"movzbl	-14(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/* 104 */
		"movzbl	-13(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/*  96 */
		"movzbl	-12(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/*  88 */
		"movzbl	-11(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/*  80 */
		"movzbl	-10(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/*  72 */
		"movzbl	 -9(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/*  64 */
		"movzbl	 -8(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/*  56 */
		"movzbl	 -7(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/*  48 */
		"movzbl	 -6(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/*  40 */
		"movzbl	 -5(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/*  32 */
		"movzbl	 -4(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/*  24 */
		"movzbl	 -3(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/*  16 */
		"movzbl	 -2(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/*   8 */
		"movzbl	 -1(%2), %4\n\t"	/* 4 */
		"add	%4, %0\n\t"	/* 2 */
		"add	%0, %1\n\t"	/* 2 */
	/*   0 */
		"dec	%3\n\t"
		"jnz	2b"
	: /* %0 */ "=R" (*s1),
	  /* %1 */ "=R" (*s2),
	  /* %2 */ "=abdSD" (buf),
	  /* %3 */ "=c" (k),
	  /* %4 */ "="CLOB"R" (t)
	: /* %5 */ "r" (16 - n),
	  /*    */ "0" (*s1),
	  /*    */ "1" (*s2),
	  /*    */ "2" (buf),
	  /*    */ "3" (k)
	);

	return buf;
}

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
	static const char vord[] GCC_ATTR_ALIGNED(16) = {16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1};
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	unsigned k;

	k    = ALIGN_DIFF(buf, 16);
	len -= k;
	if(k)
		buf = adler32_jumped(buf, &s1, &s2, k);

	asm(
		"mov	%6, %3\n\t"
		"cmp	%3, %4\n\t"
		"cmovb	%4, %3\n\t"
		"sub	%3, %4\n\t"
		"cmp	$16, %3\n\t"
		"jb	88f\n\t"
		"prefetchnta	16(%0)\n\t"
		"movdqa	%5, %%xmm5\n\t"
		"movd %2, %%xmm2\n\t"
		"movd %1, %%xmm3\n\t"
		"pxor	%%xmm4, %%xmm4\n"
		"3:\n\t"
		"pxor	%%xmm7, %%xmm7\n\t"
		".p2align 3,,3\n\t"
		".p2align 2\n"
		"2:\n\t"
		"mov	$128, %1\n\t"
		"cmp	%1, %3\n\t"
		"cmovb	%3, %1\n\t"
		"and	$-16, %1\n\t"
		"sub	%1, %3\n\t"
		"shr	$4, %1\n\t"
		"pxor	%%xmm6, %%xmm6\n\t"
		".p2align 3,,3\n\t"
		".p2align 2\n"
		"1:\n\t"
		"movdqa	(%0), %%xmm0\n\t"
		"prefetchnta	64(%0)\n\t"
		"paddd	%%xmm3, %%xmm7\n\t"
		"add	$16, %0\n\t"
		"dec	%1\n\t"
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
		"movdqa	%%xmm7, %%xmm0\n\t"
		"call	sse2_reduce\n\t"
		"pslld	$4, %%xmm0\n\t"
		"paddd	%%xmm2, %%xmm0\n\t"
		"call	sse2_reduce\n\t"
		"movdqa	%%xmm0, %%xmm2\n\t"
		"movdqa	%%xmm3, %%xmm0\n\t"
		"call	sse2_reduce\n\t"
		"movdqa	%%xmm0, %%xmm3\n\t"
		"add	%3, %4\n\t"
		"mov	%6, %3\n\t"
		"cmp	%3, %4\n\t"
		"cmovb	%4, %3\n\t"
		"sub	%3, %4\n\t"
		"cmp	$15, %3\n\t"
		"jg	3b\n\t"
		"pshufd	$0xEE, %%xmm3, %%xmm1\n\t"
		"pshufd	$0xEE, %%xmm2, %%xmm0\n\t"
		"paddd	%%xmm3, %%xmm1\n\t"
		"paddd	%%xmm2, %%xmm0\n\t"
		"pshufd	$0xE5, %%xmm0, %%xmm2\n\t"
		"paddd	%%xmm0, %%xmm2\n\t"
		"movd	%%xmm1, %1\n\t"
		"movd	%%xmm2, %2\n\t"
		"88:"
	: /* %0 */ "=r" (buf),
	  /* %1 */ "=r" (s1),
	  /* %2 */ "=r" (s2),
	  /* %3 */ "=r" (k),
	  /* %4 */ "=r" (len)
	: /* %5 */ "m" (vord[0]),
	  /* %6 */ "i" (6*NMAX),
	  /*    */ "0" (buf),
	  /*    */ "1" (s1),
	  /*    */ "2" (s2),
	  /*    */ "4" (len)
#  ifdef __SSE__
	: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#  endif
	);

	if(unlikely(k))
		buf = adler32_jumped(buf, &s1, &s2, k);
	MOD(s1);
	MOD(s2);
	return (s2 << 16) | s1;
}
# endif
#endif

static uint32_t adler32_SSE2(uint32_t adler, const uint8_t *buf, unsigned len)
{
	static const short vord_l[] GCC_ATTR_ALIGNED(16) = {16,15,14,13,12,11,10,9};
	static const short vord_h[] GCC_ATTR_ALIGNED(16) = {8,7,6,5,4,3,2,1};
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	unsigned k;

	k    = ALIGN_DIFF(buf, 16);
	len -= k;
	if(k)
		buf = adler32_jumped(buf, &s1, &s2, k);

	asm(
		"mov	%7, %3\n\t"
		"cmp	%3, %4\n\t"
		"cmovb	%4, %3\n\t"
		"sub	%3, %4\n\t"
		"cmp	$16, %3\n\t"
		"jb	88f\n\t"
		"prefetchnta	16(%0)\n\t"
		"pxor	%%xmm7, %%xmm7\n\t"
		"movdqa	%5, %%xmm6\n\t"
		"movdqa	%6, %%xmm5\n\t"
		"movd	%1, %%xmm4\n\t"
		"movd	%2, %%xmm3\n\t"
#  ifdef __ELF__
		".subsection 2\n\t"
#  else
		"jmp	77f\n\t"
#  endif
		".p2align 2\n"
		"sse2_reduce:\n\t"
		"movdqa	%%xmm0, %%xmm1\n\t"
		"pslld	$16, %%xmm1\n\t"
		"psrld	$16, %%xmm0\n\t"
		"psrld	$16, %%xmm1\n\t"
		"psubd	%%xmm0, %%xmm1\n\t"
		"pslld	$4, %%xmm0\n\t"
		"paddd	%%xmm1, %%xmm0\n\t"
		"ret\n\t"
#  ifdef __ELF__
		".previous\n\t"
#  else
		"77:\n\t"
#  endif
		".p2align 3,,3\n\t"
		".p2align 2\n"
		"1:\n\t"
		"prefetchnta	32(%0)\n\t"
		"movdqa	(%0), %%xmm1\n\t"
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
		"movdqa	%%xmm3, %%xmm0\n\t"
		"call	sse2_reduce\n\t"
		"call	sse2_reduce\n\t"
		"movdqa	%%xmm0, %%xmm3\n\t"
		"movdqa	%%xmm4, %%xmm0\n\t"
		"call	sse2_reduce\n\t"
		"movdqa	%%xmm0, %%xmm4\n\t"
		"add	%3, %4\n\t"
		"mov	%7, %3\n\t"
		"cmp	%3, %4\n\t"
		"cmovb	%4, %3\n\t"
		"sub	%3, %4\n\t"
		"cmp	$15, %3\n\t"
		"jg	1b\n\t"
		"pshufd	$0xEE, %%xmm3, %%xmm0\n\t"
		"pshufd	$0xEE, %%xmm4, %%xmm1\n\t"
		"paddd	%%xmm3, %%xmm0\n\t"
		"pshufd	$0xE5, %%xmm0, %%xmm2\n\t"
		"paddd	%%xmm4, %%xmm1\n\t"
		"movd	%%xmm1, %1\n\t"
		"paddd	%%xmm0, %%xmm2\n\t"
		"movd	%%xmm2, %2\n\t"
		"88:"
	: /* %0 */ "=r" (buf),
	  /* %1 */ "=r" (s1),
	  /* %2 */ "=r" (s2),
	  /* %3 */ "=r" (k),
	  /* %4 */ "=r" (len)
	: /* %5 */ "m" (vord_l[0]),
	  /* %6 */ "m" (vord_h[0]),
	  /* %7 */ "i" (NMAX + NMAX/3),
	  /*    */ "0" (buf),
	  /*    */ "1" (s1),
	  /*    */ "2" (s2),
	  /*    */ "4" (len)
	);

	if(unlikely(k))
		buf = adler32_jumped(buf, &s1, &s2, k);
	MOD(s1);
	MOD(s2);
	return (s2 << 16) | s1;
}

#ifndef __x86_64__
static uint32_t adler32_MMX(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	unsigned k;

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
			buf = adler32_jumped(buf, &s1, &s2, k);
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

static __init_cdata const struct test_cpu_feature tfeat_adler32_vec[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))adler32_SSSE3, .features = {[1] = CFB(CFEATURE_SSSE3), [0] = CFB(CFEATURE_CMOV)}},
# endif
#endif
	{.func = (void (*)(void))adler32_SSE2,  .features = {[0] = CFB(CFEATURE_SSE2)|CFB(CFEATURE_CMOV)}},
#ifndef __x86_64__
	{.func = (void (*)(void))adler32_MMX,   .features = {[0] = CFB(CFEATURE_MMX)}},
#endif
	{.func = (void (*)(void))adler32_x86,   .features = {}, .flags = CFF_DEFAULT},
};

DYN_JMP_DISPATCH_ST(uint32_t, adler32_vec, (uint32_t adler, const uint8_t *buf, unsigned len), (adler, buf, len))

static char const rcsid_a32x86[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
