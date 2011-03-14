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
#define MIN_WORK 56

#include "../generic/adler32.c"
#include "x86_features.h"

static const struct { short d[24]; } vord GCC_ATTR_ALIGNED(16) = {
	{1,1,1,1,1,1,1,1,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1}
};
static const struct { char d[16]; } vord_b GCC_ATTR_ALIGNED(16) = {
	{16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1}
};

static noinline const uint8_t *adler32_jumped(const uint8_t *buf, uint32_t *s1, uint32_t *s2, unsigned k)
{
	uint32_t t;
	unsigned n = k % 16;
	buf += n;
	k = (k / 16) + 1;

	asm volatile (
#  ifdef __i386__
#   ifndef __PIC__
#    define CLOB
		"lea	1f(,%5,8), %4\n\t"
#   else
#    define CLOB "&"
		"call	9f\n\t"
		"lea	1f-.(%4,%5,8), %4\n\t"
#   endif
		"jmp	*%4\n\t"
#   ifdef __PIC__
		".p2align 1\n"
		"9:\n\t"
		"movl	(%%esp), %4\n\t"
		"ret\n\t"
#  endif
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
	: "cc", "memory"
	);

	return buf;
}

#if 0
		/*
		 * Will XOP processors have SSSE3/AVX??
		 * And what is the unaligned load performance?
		 */
		"prefetchnta	0x70(%0)\n\t"
		"lddqu	(%0), %%xmm0\n\t"
		"vpaddd	%%xmm3, %%xmm5, %%xmm5\n\t"
		"sub	$16, %3\n\t"
		"add	$16, %0\n\t"
		"cmp	$15, %3\n\t"
		"vphaddubd	%%xmm0, %%xmm1\n\t" /* A */
		"vpmaddubsw %%xmm4, %%xmm0, %%xmm0\n\t"/* AVX! */ /* 1 */
		"vphadduwd	%%xmm0, %%xmm0\n\t" /* 2 */
		"vpaddd	%%xmm1, %%xmm3, %%xmm3\n\t" /* B: A+B => hadd+acc or vpmadcubd w. mul = 1 */
		"vpaddd	%%xmm0, %%xmm2, %%xmm2\n\t" /* 3: 1+2+3 => vpmadcubd w. mul = 16,15,14... */
		"jg	1b\n\t"
		xop_reduce
		xop_reduce
		xop_reduce
		setup
		"jg	1b\n\t"
		"vphaddudq	%%xmm2, %%xmm0\n\t"
		"vphaddudq	%%xmm3, %%xmm1\n\t"
		"pshufd	$0xE6, %%xmm0, %%xmm2\n\t"
		"pshufd	$0xE6, %%xmm1, %%xmm3\n\t"
		"paddd	%%xmm0, %%xmm2\n\t"
		"paddd	%%xmm1, %%xmm3\n\t"
		"movd	%%xmm2, %2\n\t"
		"movd	%%xmm3, %1\n\t"
#endif

static uint32_t adler32_SSSE3(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	unsigned k;

	k    = ALIGN_DIFF(buf, 16);
	len -= k;
	if(k)
		buf = adler32_jumped(buf, &s1, &s2, k);

	asm volatile (
		"mov	%6, %3\n\t"		/* get max. byte count VNMAX till v1_round_sum overflows */
		"cmp	%3, %4\n\t"
		"cmovb	%4, %3\n\t"		/* k = len >= VNMAX ? k : len */
		"sub	%3, %4\n\t"		/* len -= k */
		"cmp	$16, %3\n\t"
		"jb	88f\n\t"		/* if(k < 16) goto OUT */
#ifdef __ELF__
		".subsection 2\n\t"
#else
		"jmp	77f\n\t"
#endif
		".p2align 2\n"
		/*
		 * reduction function to bring a vector sum within the range of BASE
		 * This does no full reduction! When the sum is large, a number > BASE
		 * is the result. To do a full reduction call multiple times.
		 */
		"sse2_reduce:\n\t"
		"movdqa	%%xmm0, %%xmm1\n\t"	/* y = x */
		"pslld	$16, %%xmm1\n\t"	/* y <<= 16 */
		"psrld	$16, %%xmm0\n\t"	/* x >>= 16 */
		"psrld	$16, %%xmm1\n\t"	/* y >>= 16 */
		"psubd	%%xmm0, %%xmm1\n\t"	/* y -= x */
		"pslld	$4, %%xmm0\n\t"		/* x <<= 4 */
		"paddd	%%xmm1, %%xmm0\n\t"	/* x += y */
		"ret\n\t"
#ifdef __ELF__
		".previous\n\t"
#else
		"77:\n\t"
#endif
		"movdqa	%5, %%xmm5\n\t"		/* get vord_b */
		"prefetchnta	0x70(%0)\n\t"
		"movd %2, %%xmm2\n\t"		/* init vector sum vs2 with s2 */
		"movd %1, %%xmm3\n\t"		/* init vector sum vs1 with s1 */
		"pxor	%%xmm4, %%xmm4\n"	/* zero */
		"3:\n\t"
		"pxor	%%xmm7, %%xmm7\n\t"	/* zero vs1_round_sum */
		".p2align 3,,3\n\t"
		".p2align 2\n"
		"2:\n\t"
		"mov	$128, %1\n\t"		/* inner_k = 128 bytes till vs2_i overflows */
		"cmp	%1, %3\n\t"
		"cmovb	%3, %1\n\t"		/* inner_k = k >= inner_k ? inner_k : k */
		"and	$-16, %1\n\t"		/* inner_k = ROUND_TO(inner_k, 16) */
		"sub	%1, %3\n\t"		/* k -= inner_k */
		"shr	$4, %1\n\t"		/* inner_k /= 16 */
		"pxor	%%xmm6, %%xmm6\n\t"	/* zero vs2_i */
		".p2align 4,,7\n"
		".p2align 3\n"
		"1:\n\t"
		"movdqa	(%0), %%xmm0\n\t"	/* fetch input data */
		"prefetchnta	0x70(%0)\n\t"
		"paddd	%%xmm3, %%xmm7\n\t"	/* vs1_round_sum += vs1 */
		"add	$16, %0\n\t"		/* advance input data pointer */
		"dec	%1\n\t"			/* decrement inner_k */
		"movdqa	%%xmm0, %%xmm1\n\t"	/* make a copy of the input data */
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 217
		"pmaddubsw %%xmm5, %%xmm0\n\t"	/* multiply all input bytes by vord_b bytes, add adjecent results to words */
# else
		".byte 0x66, 0x0f, 0x38, 0x04, 0xc5\n\t" /* pmaddubsw %%xmm5, %%xmm0 */
# endif
#else
		".byte 0x66, 0x0f, 0x38, 0x04, 0xc5\n\t" /* pmaddubsw %%xmm5, %%xmm0 */
#endif
		"psadbw	%%xmm4, %%xmm1\n\t"	/* subtract zero from every byte, add 8 bytes to a sum */
		"paddw	%%xmm0, %%xmm6\n\t"	/* vs2_i += in * vorder_b */
		"paddd	%%xmm1, %%xmm3\n\t"	/* vs1 += psadbw */
		"jnz	1b\n\t"			/* repeat if inner_k != 0 */
		"movdqa	%%xmm6, %%xmm0\n\t"	/* copy vs2_i */
		"punpckhwd	%%xmm4, %%xmm0\n\t"	/* zero extent vs2_i upper words to dwords */
		"punpcklwd	%%xmm4, %%xmm6\n\t"	/* zero extent vs2_i lower words to dwords */
		"paddd	%%xmm0, %%xmm2\n\t"	/* vs2 += vs2_i.upper */
		"paddd	%%xmm6, %%xmm2\n\t"	/* vs2 += vs2_i.lower */
		"cmp	$15, %3\n\t"
		"jg	2b\n\t"			/* if(k > 15) repeat */
		"movdqa	%%xmm7, %%xmm0\n\t"	/* move vs1_round_sum */
		"call	sse2_reduce\n\t"	/* reduce vs1_round_sum */
		"pslld	$4, %%xmm0\n\t"		/* vs1_round_sum *= 16 */
		"paddd	%%xmm2, %%xmm0\n\t"	/* vs2 += vs1_round_sum */
		"call	sse2_reduce\n\t"	/* reduce again */
		"movdqa	%%xmm0, %%xmm2\n\t"	/* move vs2 back in place */
		"movdqa	%%xmm3, %%xmm0\n\t"	/* move vs1 */
		"call	sse2_reduce\n\t"	/* reduce */
		"movdqa	%%xmm0, %%xmm3\n\t"	/* move vs1 back in place */
		"add	%3, %4\n\t"		/* len += k */
		"mov	%6, %3\n\t"		/* get max. byte count VNMAX till v1_round_sum overflows */
		"cmp	%3, %4\n\t"
		"cmovb	%4, %3\n\t"		/* k = len >= VNMAX ? k : len */
		"sub	%3, %4\n\t"		/* len -= k */
		"cmp	$15, %3\n\t"
		"jg	3b\n\t"			/* if(k > 15) repeat */
		"pshufd	$0xEE, %%xmm3, %%xmm1\n\t"	/* collect vs1 & vs2 in lowest vector member */
		"pshufd	$0xEE, %%xmm2, %%xmm0\n\t"
		"paddd	%%xmm3, %%xmm1\n\t"
		"paddd	%%xmm2, %%xmm0\n\t"
		"pshufd	$0xE5, %%xmm0, %%xmm2\n\t"
		"paddd	%%xmm0, %%xmm2\n\t"
		"movd	%%xmm1, %1\n\t"		/* mov vs1 to s1 */
		"movd	%%xmm2, %2\n"		/* mov vs2 to s2 */
		"88:"
	: /* %0 */ "=r" (buf),
	  /* %1 */ "=r" (s1),
	  /* %2 */ "=r" (s2),
	  /* %3 */ "=r" (k),
	  /* %4 */ "=r" (len)
	: /* %5 */ "m" (vord_b),
	 /*
	  * somewhere between 5 & 6, psadbw 64 bit sums ruin the party
	  * spreading the sums with palignr only brings it to 7 (?),
	  * while introducing an op into the main loop (2800 ms -> 3200 ms)
	  */
	  /* %6 */ "i" (5*NMAX),
	  /*    */ "0" (buf),
	  /*    */ "1" (s1),
	  /*    */ "2" (s2),
	  /*    */ "4" (len)
	: "cc", "memory"
#ifdef __SSE__
	, "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
	);

	if(unlikely(k))
		buf = adler32_jumped(buf, &s1, &s2, k);
	reduce(s1);
	reduce(s2);
	return (s2 << 16) | s1;
}

static uint32_t adler32_SSE2(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	unsigned k;

	k    = ALIGN_DIFF(buf, 16);
	len -= k;
	if(k)
		buf = adler32_jumped(buf, &s1, &s2, k);

	asm volatile (
		"mov	%6, %3\n\t"
		"cmp	%3, %4\n\t"
		"cmovb	%4, %3\n\t"
		"sub	%3, %4\n\t"
		"cmp	$16, %3\n\t"
		"jb	88f\n\t"
		"prefetchnta	0x70(%0)\n\t"
		"movd	%1, %%xmm4\n\t"
		"movd	%2, %%xmm3\n\t"
		"pxor	%%xmm2, %%xmm2\n\t"
		"pxor	%%xmm5, %%xmm5\n\t"
		".p2align 2\n"
		"3:\n\t"
		"pxor	%%xmm6, %%xmm6\n\t"
		"pxor	%%xmm7, %%xmm7\n\t"
		"mov	$2048, %1\n\t"		/* get byte count till vs2_{l|h}_word overflows */
		"cmp	%1, %3\n\t"
		"cmovb	%3, %1\n"
		"and	$-16, %1\n\t"
		"sub	%1, %3\n\t"
		"shr	$4, %1\n\t"
		".p2align 4,,7\n"
		".p2align 3\n"
		"1:\n\t"
		"prefetchnta	0x70(%0)\n\t"
		"movdqa	(%0), %%xmm0\n\t"	/* fetch input data */
		"paddd	%%xmm4, %%xmm5\n\t"	/* vs1_round_sum += vs1 */
		"add	$16, %0\n\t"
		"dec	%1\n\t"
		"movdqa	%%xmm0, %%xmm1\n\t"	/* copy input data */
		"psadbw	%%xmm2, %%xmm0\n\t"	/* add all bytes horiz. */
		"paddd	%%xmm0, %%xmm4\n\t"	/* add that to vs1 */
		"movdqa	%%xmm1, %%xmm0\n\t"	/* copy input data */
		"punpckhbw	%%xmm2, %%xmm1\n\t"	/* zero extent input upper bytes to words */
		"punpcklbw	%%xmm2, %%xmm0\n\t"	/* zero extent input lower bytes to words */
		"paddw	%%xmm1, %%xmm7\n\t"	/* vs2_h_words += in_high_words */
		"paddw	%%xmm0, %%xmm6\n\t"	/* vs2_l_words += in_low_words */
		"jnz	1b\n\t"
		"cmp	$15, %3\n\t"
		"pmaddwd	32+%5, %%xmm7\n\t"	/* multiply vs2_h_words with order, add adjecend results */
		"pmaddwd	16+%5, %%xmm6\n\t"	/* multiply vs2_l_words with order, add adjecend results */
		"paddd	%%xmm7, %%xmm3\n\t"	/* add to vs2 */
		"paddd	%%xmm6, %%xmm3\n\t"	/* add to vs2 */
		"jg	3b\n\t"
		"movdqa	%%xmm5, %%xmm0\n\t"
		"pxor	%%xmm5, %%xmm5\n\t"
		"call	sse2_reduce\n\t"
		"pslld	$4, %%xmm0\n\t"
		"paddd	%%xmm3, %%xmm0\n\t"
		"call	sse2_reduce\n\t"
		"movdqa	%%xmm0, %%xmm3\n\t"
		"movdqa	%%xmm4, %%xmm0\n\t"
		"call	sse2_reduce\n\t"
		"movdqa	%%xmm0, %%xmm4\n\t"
		"add	%3, %4\n\t"
		"mov	%6, %3\n\t"
		"cmp	%3, %4\n\t"
		"cmovb	%4, %3\n"
		"sub	%3, %4\n\t"
		"cmp	$15, %3\n\t"
		"jg	3b\n\t"
		"pshufd	$0xEE, %%xmm4, %%xmm1\n\t"
		"pshufd	$0xEE, %%xmm3, %%xmm0\n\t"
		"paddd	%%xmm4, %%xmm1\n\t"
		"paddd	%%xmm3, %%xmm0\n\t"
		"pshufd	$0xE5, %%xmm0, %%xmm3\n\t"
		"paddd	%%xmm0, %%xmm3\n\t"
		"movd	%%xmm1, %1\n\t"
		"movd	%%xmm3, %2\n"
		"88:\n\t"
	: /* %0 */ "=r" (buf),
	  /* %1 */ "=r" (s1),
	  /* %2 */ "=r" (s2),
	  /* %3 */ "=r" (k),
	  /* %4 */ "=r" (len)
	: /* %5 */ "m" (vord),
	  /* %6 */ "i" (5*NMAX),
	  /*    */ "0" (buf),
	  /*    */ "1" (s1),
	  /*    */ "2" (s2),
	  /*    */ "3" (k),
	  /*    */ "4" (len)
	: "cc", "memory"
#ifdef __SSE__
	, "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
	);

	if(unlikely(k))
		buf = adler32_jumped(buf, &s1, &s2, k);
	reduce(s1);
	reduce(s2);
	return (s2 << 16) | s1;
}

#if 0
/*
 * The SSE2 version above is faster on my CPUs (Athlon64, Core2,
 * P4 Xeon, K10 Sempron), but has instruction stalls only a
 * Out-Of-Order-Execution CPU can solve.
 * So this Version _may_ be better for the new old thing, Atom.
 */
static uint32_t adler32_SSE2_no_oooe(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	unsigned k;

	k    = ALIGN_DIFF(buf, 16);
	len -= k;
	if(k)
		buf = adler32_jumped(buf, &s1, &s2, k);

	asm volatile (
		"mov	%6, %3\n\t"
		"cmp	%3, %4\n\t"
		"cmovb	%4, %3\n\t"
		"sub	%3, %4\n\t"
		"cmp	$16, %3\n\t"
		"jb	88f\n\t"
		"movdqa	16+%5, %%xmm6\n\t"
		"movdqa	32+%5, %%xmm5\n\t"
		"prefetchnta	16(%0)\n\t"
		"pxor	%%xmm7, %%xmm7\n\t"
		"movd	%1, %%xmm4\n\t"
		"movd	%2, %%xmm3\n\t"
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
		"mov	%6, %3\n\t"
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
		"movd	%%xmm2, %2\n"
		"88:"
	: /* %0 */ "=r" (buf),
	  /* %1 */ "=r" (s1),
	  /* %2 */ "=r" (s2),
	  /* %3 */ "=r" (k),
	  /* %4 */ "=r" (len)
	: /* %5 */ "m" (vord),
	  /* %6 */ "i" (NMAX + NMAX/3),
	  /*    */ "0" (buf),
	  /*    */ "1" (s1),
	  /*    */ "2" (s2),
	  /*    */ "4" (len)
	: "cc", "memory"
#ifdef __SSE__
	, "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
	);

	if(unlikely(k))
		buf = adler32_jumped(buf, &s1, &s2, k);
	reduce(s1);
	reduce(s2);
	return (s2 << 16) | s1;
}
#endif

#ifndef __x86_64__
/*
 * SSE version to help VIA-C3_2, P2 & P3
 */
static uint32_t adler32_SSE(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	int k;

	k    = ALIGN_DIFF(buf, 8);
	len -= k;
	if(k)
		buf = adler32_jumped(buf, &s1, &s2, k);

	asm volatile (
		"mov	%6, %3\n\t"
		"cmp	%3, %4\n\t"
		"cmovb	%4, %3\n"
		"sub	%3, %4\n\t"
		"cmp	$8, %3\n\t"
		"jb	88f\n\t"
		"movd	%1, %%mm4\n\t"
		"movd	%2, %%mm3\n\t"
		"pxor	%%mm2, %%mm2\n\t"
		"pxor	%%mm5, %%mm5\n\t"
# ifdef __ELF__
		".subsection 2\n\t"
# else
		"jmp	77f\n\t"
# endif
		".p2align 2\n"
		"mmx_reduce:\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"pslld	$16, %%mm1\n\t"
		"psrld	$16, %%mm0\n\t"
		"psrld	$16, %%mm1\n\t"
		"psubd	%%mm0, %%mm1\n\t"
		"pslld	$4, %%mm0\n\t"
		"paddd	%%mm1, %%mm0\n\t"
		"ret\n\t"
# ifdef __ELF__
		".previous\n\t"
# else
		"77:\n\t"
# endif
		".p2align	2\n"
		"3:\n\t"
		"pxor	%%mm6, %%mm6\n\t"
		"pxor	%%mm7, %%mm7\n\t"
		"mov	$1024, %1\n\t"
		"cmp	%1, %3\n\t"
		"cmovb	%3, %1\n"
		"and	$-8, %1\n\t"
		"sub	%1, %3\n\t"
		"shr	$3, %1\n\t"
		".p2align 4,,7\n"
		".p2align 3\n"
		"1:\n\t"
		"movq	(%0), %%mm0\n\t"
		"paddd	%%mm4, %%mm5\n\t"
		"add	$8, %0\n\t"
		"dec	%1\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"psadbw	%%mm2, %%mm0\n\t"
		"paddd	%%mm0, %%mm4\n\t"
		"movq	%%mm1, %%mm0\n\t"
		"punpckhbw	%%mm2, %%mm1\n\t"
		"punpcklbw	%%mm2, %%mm0\n\t"
		"paddw	%%mm1, %%mm7\n\t"
		"paddw	%%mm0, %%mm6\n\t"
		"jnz	1b\n\t"
		"cmp	$7, %3\n\t"
		"pmaddwd	40+%5, %%mm7\n\t"
		"pmaddwd	32+%5, %%mm6\n\t"
		"paddd	%%mm7, %%mm3\n\t"
		"paddd	%%mm6, %%mm3\n\t"
		"jg	3b\n\t"
		"movq	%%mm5, %%mm0\n\t"
		"pxor	%%mm5, %%mm5\n\t"
		"call	mmx_reduce\n\t"
		"pslld	$3, %%mm0\n\t"
		"paddd	%%mm3, %%mm0\n\t"
		"call	mmx_reduce\n\t"
		"movq	%%mm0, %%mm3\n\t"
		"movq	%%mm4, %%mm0\n\t"
		"call	mmx_reduce\n\t"
		"movq	%%mm0, %%mm4\n\t"
		"add	%3, %4\n\t"
		"mov	%6, %3\n\t"
		"cmp	%3, %4\n\t"
		"cmovb	%4, %3\n"
		"sub	%3, %4\n\t"
		"cmp	$7, %3\n\t"
		"jg	3b\n\t"
		"movd	%%mm4, %1\n\t"
		"psrlq	$32, %%mm4\n\t"
		"movd	%%mm3, %2\n\t"
		"psrlq	$32, %%mm3\n\t"
		"movd	%%mm4, %4\n\t"
		"add	%4, %1\n\t"
		"movd	%%mm3, %4\n\t"
		"add	%4, %2\n"
		"88:\n\t"
	: /* %0 */ "=r" (buf),
	  /* %1 */ "=r" (s1),
	  /* %2 */ "=r" (s2),
	  /* %3 */ "=r" (k),
	  /* %4 */ "=r" (len)
	: /* %5 */ "m" (vord),
	  /* %6 */ "i" ((5*NMAX)/2),
	  /*    */ "0" (buf),
	  /*    */ "1" (s1),
	  /*    */ "2" (s2),
	  /*    */ "3" (k),
	  /*    */ "4" (len)
	: "cc", "memory"
#ifdef __MMX__
	, "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
#endif
	);

	if(unlikely(k))
		buf = adler32_jumped(buf, &s1, &s2, k);
	reduce(s1);
	reduce(s2);
	return (s2 << 16) | s1;
}

/*
 * Processors which only have MMX will prop. not like this
 * code, they are so old, they are not Out-Of-Order
 * (maybe except AMD K6, Cyrix, Winchip/VIA).
 * I did my best to get at least 1 instruction between result -> use
 */
static uint32_t adler32_MMX(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	int k;

	k    = ALIGN_DIFF(buf, 8);
	len -= k;
	if(k)
		buf = adler32_jumped(buf, &s1, &s2, k);

	asm volatile (
		"mov	%6, %3\n\t"
		"cmp	%3, %4\n\t"
		"jae	11f\n\t"
		"mov	%4, %3\n"
		"11:\n\t"
		"sub	%3, %4\n\t"
		"cmp	$8, %3\n\t"
		"jb	88f\n\t"
		"sub	$8, %%esp\n\t"
		"movd	%1, %%mm4\n\t"
		"movd	%2, %%mm2\n\t"
		"movq	%5, %%mm3\n"
		"33:\n\t"
		"movq	%%mm2, %%mm0\n\t"
		"pxor	%%mm2, %%mm2\n\t"
		"pxor	%%mm5, %%mm5\n\t"
		".p2align 2\n"
		"3:\n\t"
		"movq	%%mm0, (%%esp)\n\t"
		"pxor	%%mm6, %%mm6\n\t"
		"pxor	%%mm7, %%mm7\n\t"
		"mov	$1024, %1\n\t"
		"cmp	%1, %3\n\t"
		"jae	44f\n\t"
		"mov	%3, %1\n"
		"44:\n\t"
		"and	$-8, %1\n\t"
		"sub	%1, %3\n\t"
		"shr	$3, %1\n\t"
		".p2align 4,,7\n"
		".p2align 3\n"
		"1:\n\t"
		"movq	(%0), %%mm0\n\t"
		"paddd	%%mm4, %%mm5\n\t"
		"add	$8, %0\n\t"
		"dec	%1\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"punpcklbw	%%mm2, %%mm0\n\t"
		"punpckhbw	%%mm2, %%mm1\n\t"
		"paddw	%%mm0, %%mm6\n\t"
		"paddw	%%mm1, %%mm0\n\t"
		"paddw	%%mm1, %%mm7\n\t"
		"pmaddwd	%%mm3, %%mm0\n\t"
		"paddd	%%mm0, %%mm4\n\t"
		"jnz	1b\n\t"
		"movq	(%%esp), %%mm0\n\t"
		"cmp	$7, %3\n\t"
		"pmaddwd	32+%5, %%mm6\n\t"
		"pmaddwd	40+%5, %%mm7\n\t"
		"paddd	%%mm6, %%mm0\n\t"
		"paddd	%%mm7, %%mm0\n\t"
		"jg	3b\n\t"
		"movq	%%mm0, %%mm2\n\t"
		"movq	%%mm5, %%mm0\n\t"
		"call	mmx_reduce\n\t"
		"pslld	$3, %%mm0\n\t"
		"paddd	%%mm2, %%mm0\n\t"
		"call	mmx_reduce\n\t"
		"movq	%%mm0, %%mm2\n\t"
		"movq	%%mm4, %%mm0\n\t"
		"call	mmx_reduce\n\t"
		"movq	%%mm0, %%mm4\n\t"
		"add	%3, %4\n\t"
		"mov	%6, %3\n\t"
		"cmp	%3, %4\n\t"
		"jae	22f\n\t"
		"mov	%4, %3\n"
		"22:\n\t"
		"sub	%3, %4\n\t"
		"cmp	$7, %3\n\t"
		"jg	33b\n\t"
		"add	$8, %%esp\n\t"
		"movd	%%mm4, %1\n\t"
		"psrlq	$32, %%mm4\n\t"
		"movd	%%mm2, %2\n\t"
		"psrlq	$32, %%mm2\n\t"
		"movd	%%mm4, %4\n\t"
		"add	%4, %1\n\t"
		"movd	%%mm2, %4\n\t"
		"add	%4, %2\n"
		"88:\n\t"
	: /* %0 */ "=r" (buf),
	  /* %1 */ "=r" (s1),
	  /* %2 */ "=r" (s2),
	  /* %3 */ "=r" (k),
	  /* %4 */ "=r" (len)
	: /* %5 */ "m" (vord),
	  /* %6 */ "i" (4*NMAX),
	  /*    */ "0" (buf),
	  /*    */ "1" (s1),
	  /*    */ "2" (s2),
	  /*    */ "3" (k),
	  /*    */ "4" (len)
	: "cc", "memory"
#ifdef __MMX__
	, "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
#endif
	);

	if(unlikely(k))
		buf = adler32_jumped(buf, &s1, &s2, k);
	reduce(s1);
	reduce(s2);
	return (s2 << 16) | s1;
}
#endif

static uint32_t adler32_x86(uint32_t adler, const uint8_t *buf, unsigned len)
{
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	unsigned n;

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
			 * into i386 register set with no framepointer.
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
		reduce_full(s1);
		reduce_full(s2);
	}
	return (s2 << 16) | s1;
}

static __init_cdata const struct test_cpu_feature tfeat_adler32_vec[] =
{
	{.func = (void (*)(void))adler32_SSSE3, .features = {[1] = CFB(CFEATURE_SSSE3), [0] = CFB(CFEATURE_CMOV)}},
	{.func = (void (*)(void))adler32_SSE2,  .features = {[0] = CFB(CFEATURE_SSE2)|CFB(CFEATURE_CMOV)}},
#ifndef __x86_64__
	{.func = (void (*)(void))adler32_SSE,   .features = {[0] = CFB(CFEATURE_SSE)|CFB(CFEATURE_CMOV)}},
	{.func = (void (*)(void))adler32_MMX,   .features = {[0] = CFB(CFEATURE_MMX)}},
#endif
	{.func = (void (*)(void))adler32_x86,   .features = {}, .flags = CFF_DEFAULT},
};

DYN_JMP_DISPATCH_ST(uint32_t, adler32_vec, (uint32_t adler, const uint8_t *buf, unsigned len), (adler, buf, len))

static char const rcsid_a32x86[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
