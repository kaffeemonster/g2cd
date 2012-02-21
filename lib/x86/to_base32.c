/*
 * to_base32.c
 * convert binary string to base 32, x86 implementation
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
 * $Id: $
 */

#define ARCH_NAME_SUFFIX _generic
#define ONLY_REMAINDER
#include "../generic/to_base32.c"

#include "x86_features.h"

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 219
static unsigned char *to_base32_AVX(unsigned char *dst, const unsigned char *src, unsigned len);
# endif
# if HAVE_BINUTILS >= 218
static unsigned char *to_base32_SSE41(unsigned char *dst, const unsigned char *src, unsigned len);
# endif
# if HAVE_BINUTILS >= 217
static unsigned char *to_base32_SSE3(unsigned char *dst, const unsigned char *src, unsigned len);
# endif
#endif
static unsigned char *to_base32_SSE2(unsigned char *dst, const unsigned char *src, unsigned len);
#ifndef __x86_64__
static unsigned char *to_base32_SSE(unsigned char *dst, const unsigned char *src, unsigned len);
#endif

static const unsigned char vals[][16] GCC_ATTR_ALIGNED(16) =
{
	/* 1    2    3    4    5    6    7    8    9   10   11   12   13   14   15   16 */
	{0x0f,0x0e,0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00},
	{0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF},
	{0x00,0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF},
	{0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF},
	{0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F},
	{0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61},
	{0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A},
	{0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49},
};

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
#  ifndef __i386__
static unsigned char *do_40bit_BMI2(unsigned char *dst, uint64_t d1)
{
	asm ("pdep %2, %1, %0" : "=r" (d1) : "r" (d1), "m" (vals[4][0]));
	d1 = __swab64(d1);

	/* convert */
	asm (
			"movq	%1, %%xmm0\n\t"
			"paddb	%2, %%xmm0\n\t"
			"movdqa	%%xmm0, %%xmm1\n\t"
			"pcmpgtb	%3, %%xmm1\n\t"
			"pand	%4, %%xmm1\n\t"
			"psubb	%%xmm1, %%xmm0\n\t"
			"movq	%%xmm0, %0"
		: /* %0 */ "=m" (*dst)
		: /* %1 */ "r" (d1),
		  /* %2 */ "m" (vals[5][0]),
		  /* %3 */ "m" (vals[6][0]),
		  /* %4 */ "m" (vals[7][0])
#  ifdef __SSE__
		: "xmm0", "xmm1"
#  endif
	);
	return dst + 8;
}
# else
static unsigned char *do_40bit_BMI2(unsigned char *dst, uint32_t a1, uint32_t a2)
{
	asm ("pdep %2, %1, %0" : "=r" (a1) : "r" (a1), "m" (vals[4][0]));
	asm ("pdep %2, %1, %0" : "=r" (a2) : "r" (a2), "m" (vals[4][0]));
	a1 = __swab32(a1);
	a2 = __swab32(a2);

	/* convert */
	asm (
			"movd	%1, %%xmm0\n\t"
			"pinsrd	$1, %2, %%xmm0\n\t"
			"paddb	%3, %%xmm0\n\t"
			"movdqa	%%xmm0, %%xmm1\n\t"
			"pcmpgtb	%4, %%xmm1\n\t"
			"pand	%5, %%xmm1\n\t"
			"psubb	%%xmm1, %%xmm0\n\t"
			"movq	%%xmm0, %0"
		: /* %0 */ "=m" (*dst)
		: /* %1 */ "r" (a1),
		  /* %2 */ "r" (a2),
		  /* %2 */ "m" (vals[5][0]),
		  /* %3 */ "m" (vals[6][0]),
		  /* %4 */ "m" (vals[7][0])
#  ifdef __SSE__
		: "xmm0", "xmm1"
#  endif
	);
	return dst + 8;
}
#  endif

static unsigned char *to_base32_BMI2(unsigned char *dst, const unsigned char *src, unsigned len)
{
#  ifndef __i386__
	while(len >= sizeof(uint64_t))
	{
		uint64_t d = get_unaligned_be64(src);
		src += 5;
		len -= 5;
		dst = do_40bit_BMI2(dst, d >> (64-40));
	}
#  endif
	while(len >= 5)
	{
#  ifndef __i386__
		uint64_t d = ((uint64_t)get_unaligned_be32(src) << 8) |
		              (uint64_t)src[4];
		dst = do_40bit_BMI2(dst, d);
#  else
		uint32_t a1 = get_unaligned_be32(src);
		uint32_t a2 = src[4];
		dst = do_40bit_BMI2(dst, a1 >> (32 - 20), a2 | (a1 << 8));
#  endif
		src += 5;
		len -= 5;
	}
	if(len)
		return to_base32_generic(dst, src, len);
	return dst;
}
# endif

# if HAVE_BINUTILS >= 219
/*
 * This code does not use any AVX feature, it only uses the new
 * v* opcodes, so the upper half of the register gets 0-ed,
 * and the CPU is not caught with lower/upper half merges
 */
static unsigned char *to_base32_AVX(unsigned char *dst, const unsigned char *src, unsigned len)
{
	asm (
			"cmp	$8, %2\n\t"
			"jb	2f\n\t"
			"vmovdqa	-64+%3, %%xmm3\n\t"
			"vmovdqa	-16+%3, %%xmm0\n\t"
			"vmovdqa	%3, %%xmm7\n\t"
			"vmovdqa	16+%3, %%xmm6\n\t"
			"vmovdqa	32+%3, %%xmm4\n\t"
			"vmovdqa	48+%3, %%xmm5\n\t"
			"cmp	$16, %2\n\t"
			"jb	3f\n\t"
			".p2align 2\n"
			"1:\n\t"
			"vlddqu	(%1), %%xmm1\n\t"            /* fetch input data */
			"sub	$10, %2\n\t"
			"add	$10, %1\n\t"
			/* swab endianess */
			"vpshufb	%%xmm1, %%xmm3, %%xmm1\n\t"
			/* partionate */
			"vpslldq	$5, %%xmm1, %%xmm2\n\t"               /* shift & copy */
			"vpunpckhqdq	%%xmm1, %%xmm2, %%xmm1\n\t"     /* eliminate & join */
			"vpsrlq	$0xc, %%xmm1, %%xmm2\n\t"             /* shift & copy */
			"vpblendw	$0x33, %%xmm1, %%xmm2, %%xmm1\n\t" /* eleminate & join */
			"vpsrld	$0x6, %%xmm1, %%xmm2\n\t"             /* shift & copy */
			"vpblendw	$0x55, %%xmm2, %%xmm1, %%xmm1\n\t" /* eleminate & join */
			"vpsrlw	$0x3, %%xmm1, %%xmm2\n\t"             /* shift & copy */
			"vpblendvb %%xmm0, %%xmm1, %%xmm2, %%xmm2\n\t" /* eliminate & join */
			"vpsrlw	$0x3, %%xmm2, %%xmm2\n\t"             /* bring it down */
			"vpand	%%xmm7, %%xmm2, %%xmm2\n\t"           /* eliminate */
			/* convert */
			"vpaddb	%%xmm6, %%xmm2, %%xmm2\n\t"
			"vpcmpgtb	%%xmm2, %%xmm4, %%xmm1\n\t"
			"vpand	%%xmm5, %%xmm1, %%xmm1\n\t"
			"vpsubb	%%xmm2, %%xmm1, %%xmm1\n\t"
			/* write out */
			"cmp	$15, %2\n\t"
			"vpshufb	%%xmm1, %%xmm3, %%xmm1\n\t"
			"vmovdqu	%%xmm1,     (%0)\n\t"
			"lea	16(%0), %0\n\t"
			"ja	1b\n\t"
			"cmp	$8, %2\n\t"
			"jb	2f\n"
			"3:\n\t"
			"vmovq	(%1), %%xmm1\n\t"       /* fetch input data */
			"sub	$5, %2\n\t"
			"add	$5, %1\n\t"
			"cmp	$5, %2\n\t"
			"jb	4f\n\t"
			"vpinsrw	$4, 3(%1), %%xmm1, %%xmm1\n\t"
			"4:\n\t"
			/* swab endianess */
			"vpshufb	%%xmm1, %%xmm3, %%xmm1\n\t"
			/* partionate */
			"vpslldq	$5, %%xmm1, %%xmm2\n\t"               /* shift & copy */
			"vpunpckhqdq	%%xmm1, %%xmm2, %%xmm1\n\t"     /* eliminate & join */
			"vpsrlq	$0xc, %%xmm1, %%xmm2\n\t"             /* shift & copy */
			"vpblendw	$0x33, %%xmm1, %%xmm2, %%xmm1\n\t" /* eleminate & join */
			"vpsrld	$0x6, %%xmm1, %%xmm2\n\t"             /* shift & copy */
			"vpblendw	$0x55, %%xmm2, %%xmm1, %%xmm1\n\t" /* eleminate & join */
			"vpsrlw	$0x3, %%xmm1, %%xmm2\n\t"             /* shift & copy */
			"vpblendvb %%xmm0, %%xmm1, %%xmm2, %%xmm2\n\t" /* eliminate & join */
			"vpsrlw	$0x3, %%xmm2, %%xmm2\n\t"             /* bring it down */
			"vpand	%%xmm7, %%xmm2, %%xmm2\n\t"           /* eliminate */
			/* convert */
			"vpaddb	%%xmm6, %%xmm2, %%xmm2\n\t"
			"vpcmpgtb	%%xmm2, %%xmm4, %%xmm1\n\t"
			"vpand	%%xmm5, %%xmm1, %%xmm1\n\t"
			"vpsubb	%%xmm2, %%xmm1, %%xmm1\n\t"
			/* write out */
			"vpshufb	%%xmm1, %%xmm3, %%xmm1\n\t"
			"cmp	$5, %2\n\t"
			"jb	6f\n\t"
			"sub	$5, %2\n\t"
			"add	$5, %1\n\t"
			"vmovdqu	%%xmm1, (%0)\n\t"
			"lea	16(%0), %0\n\t"
			"jmp	5f\n"
			"6:\n\t"
			"vmovq	%%xmm1, (%0)\n\t"
			"lea	8(%0), %0\n\t"
			"5:\n\t"
			"cmp	$7, %2\n\t"
			"ja	3b\n"
			"2:"
		: /* %0 */ "=r" (dst),
		  /* %1 */ "=r" (src),
		  /* %2 */ "=r" (len)
		: /* %3 */ "m" (vals[4][0]),
		  /*    */ "0" (dst),
		  /*    */ "1" (src),
		  /*    */ "2" (len)
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#  endif
	);
	if(len)
		return to_base32_generic(dst, src, len);
	return dst;
}
# endif

# if HAVE_BINUTILS >= 218
static unsigned char *to_base32_SSE41(unsigned char *dst, const unsigned char *src, unsigned len)
{
	asm (
			"cmp	$8, %2\n\t"
			"jb	2f\n\t"
			"movdqa	-64+%3, %%xmm3\n\t"
			"movdqa	-16+%3, %%xmm0\n\t"
			"movdqa	%3, %%xmm7\n\t"
			"movdqa	16+%3, %%xmm6\n\t"
			"movdqa	32+%3, %%xmm4\n\t"
			"movdqa	48+%3, %%xmm5\n\t"
			"cmp	$16, %2\n\t"
			"jb	3f\n\t"
			".p2align 2\n"
			"1:\n\t"
			"lddqu	(%1), %%xmm1\n\t"            /* fetch input data */
			"sub	$10, %2\n\t"
			"add	$10, %1\n\t"
			/* swab endianess */
			"pshufb	%%xmm3, %%xmm1\n\t"
			/* partionate */
			"movdqa	%%xmm1, %%xmm2\n\t"          /* copy */
			"pslldq	$5, %%xmm1\n\t"              /* shift copy */
			"punpckhqdq	%%xmm2, %%xmm1\n\t"       /* eliminate & join */
			"movdqa	%%xmm1, %%xmm2\n\t"          /* copy */
			"psrlq	$0xc, %%xmm1\n\t"            /* shift copy */
			"pblendw	$0x33, %%xmm1, %%xmm2\n\t"   /* eleminate & join */
			"movdqa	%%xmm2, %%xmm1\n\t"          /* copy */
			"psrld	$0x6, %%xmm2\n\t"            /* shift copy */
			"pblendw	$0x55, %%xmm2, %%xmm1\n\t"   /* eleminate & join */
			"movdqa	%%xmm1, %%xmm2\n\t"          /* copy */
			"psrlw	$0x3, %%xmm1\n\t"            /* shift copy */
			"pblendvb %%xmm0, %%xmm1, %%xmm2\n\t" /* eliminate & join */
			"psrlw	$0x3, %%xmm2\n\t"            /* bring it down */
			"pand	%%xmm7, %%xmm2\n\t"             /* eliminate */
			/* convert */
			"paddb	%%xmm6, %%xmm2\n\t"
			"movdqa	%%xmm2, %%xmm1\n\t"
			"pcmpgtb	%%xmm4, %%xmm2\n\t"
			"pand	%%xmm5, %%xmm2\n\t"
			"psubb	%%xmm2, %%xmm1\n\t"
			/* write out */
			"cmp	$15, %2\n\t"
			"pshufb	%%xmm3, %%xmm1\n\t"
			"movdqu	%%xmm1,     (%0)\n\t"
			"lea	16(%0), %0\n\t"
			"ja	1b\n\t"
			"cmp	$8, %2\n\t"
			"jb	2f\n"
			"3:\n\t"
			"movq	(%1), %%xmm1\n\t"       /* fetch input data */
			"sub	$5, %2\n\t"
			"add	$5, %1\n\t"
			"cmp	$5, %2\n\t"
			"jb	4f\n\t"
			"pinsrw	$4, 3(%1), %%xmm1\n\t"
			"4:\n\t"
			/* swab endianess */
			"pshufb	%%xmm3, %%xmm1\n\t"
			/* partionate */
			"movdqa	%%xmm1, %%xmm2\n\t"          /* copy */
			"pslldq	$5, %%xmm1\n\t"              /* shift copy */
			"punpckhqdq	%%xmm2, %%xmm1\n\t"       /* eliminate & join */
			"movdqa	%%xmm1, %%xmm2\n\t"          /* copy */
			"psrlq	$0xc, %%xmm1\n\t"            /* shift copy */
			"pblendw	$0x33, %%xmm1, %%xmm2\n\t"   /* eleminate & join */
			"movdqa	%%xmm2, %%xmm1\n\t"          /* copy */
			"psrld	$0x6, %%xmm2\n\t"            /* shift copy */
			"pblendw	$0x55, %%xmm2, %%xmm1\n\t"   /* eleminate & join */
			"movdqa	%%xmm1, %%xmm2\n\t"          /* copy */
			"psrlw	$0x3, %%xmm1\n\t"            /* shift copy */
			"pblendvb %%xmm0, %%xmm1, %%xmm2\n\t" /* eliminate & join */
			"psrlw	$0x3, %%xmm2\n\t"            /* bring it down */
			"pand	%%xmm7, %%xmm2\n\t"             /* eliminate */
			/* convert */
			"paddb	%%xmm6, %%xmm2\n\t"
			"movdqa	%%xmm2, %%xmm1\n\t"
			"pcmpgtb	%%xmm4, %%xmm2\n\t"
			"pand	%%xmm5, %%xmm2\n\t"
			"psubb	%%xmm2, %%xmm1\n\t"
			/* write out */
			"pshufb	%%xmm3, %%xmm1\n\t"
			"cmp	$5, %2\n\t"
			"jb	6f\n\t"
			"sub	$5, %2\n\t"
			"add	$5, %1\n\t"
			"movdqu	%%xmm1, (%0)\n\t"
			"lea	16(%0), %0\n\t"
			"jmp	5f\n"
			"6:\n\t"
			"movq	%%xmm1, (%0)\n\t"
			"lea	8(%0), %0\n\t"
			"5:\n\t"
			"cmp	$7, %2\n\t"
			"ja	3b\n"
			"2:"
		: /* %0 */ "=r" (dst),
		  /* %1 */ "=r" (src),
		  /* %2 */ "=r" (len)
		: /* %3 */ "m" (vals[4][0]),
		  /*    */ "0" (dst),
		  /*    */ "1" (src),
		  /*    */ "2" (len)
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#  endif
	);
	if(len)
		return to_base32_generic(dst, src, len);
	return dst;
}
# endif

# if HAVE_BINUTILS >= 217
static unsigned char *to_base32_SSE3(unsigned char *dst, const unsigned char *src, unsigned len)
{
	asm (
			"cmp	$8, %2\n\t"
			"jb	2f\n\t"
			"movdqa	-48+%3, %%xmm7\n\t"
			"movdqa	-32+%3, %%xmm6\n\t"
			"movdqa	-16+%3, %%xmm5\n\t"
#   ifdef __x86_64__
			"movdqa	%3, %%xmm8\n\t"
			"movdqa	16+%3, %%xmm9\n\t"
#   endif
			"movdqa	32+%3, %%xmm3\n\t"
			"movdqa	48+%3, %%xmm4\n\t"
			"cmp	$16, %2\n\t"
			"jb	3f\n\t"
			".p2align 2\n"
			"1:\n\t"
			"lddqu	(%1), %%xmm0\n\t"       /* fetch input data */
			"sub	$10, %2\n\t"
			"add	$10, %1\n\t"
			/* swab endianess */
			"movdqa	%%xmm0, %%xmm1\n\t"
			"psrlw	$8, %%xmm0\n\t"
			"psllw	$8, %%xmm1\n\t"
			"por	%%xmm1, %%xmm0\n\t"
			"pshuflw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufhw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufd	$0x4e, %%xmm0, %%xmm0\n\t"
			/* partionate */
			"movdqa	%%xmm0, %%xmm1\n\t"    /* copy */
			"pslldq	$5, %%xmm0\n\t"        /* shift copy */
			"punpckhqdq	%%xmm1, %%xmm0\n\t" /* eliminate & join */
			"movdqa	%%xmm7, %%xmm2\n\t"
			"movdqa	%%xmm0, %%xmm1\n\t"    /* copy */
			"psrlq	$0xc, %%xmm0\n\t"      /* shift copy */
			"pand	%%xmm7, %%xmm1\n\t"       /* eliminate */
			"pandn	%%xmm0, %%xmm2\n\t"
			"por	%%xmm2, %%xmm1\n\t"       /* join */
			"movdqa	%%xmm6, %%xmm2\n\t"
			"movdqa	%%xmm1, %%xmm0\n\t"    /* copy */
			"pand	%%xmm6, %%xmm1\n\t"       /* eliminate */
			"psrld	$0x6, %%xmm0\n\t"      /* shift copy */
			"pandn	%%xmm0, %%xmm2\n\t"
			"por	%%xmm2, %%xmm1\n\t"       /* join */
			"movdqa	%%xmm5, %%xmm2\n\t"
			"movdqa	%%xmm1, %%xmm0\n\t"    /* copy */
			"pand	%%xmm5, %%xmm1\n\t"       /* eliminate */
			"psrlw	$0x3, %%xmm0\n\t"      /* shift copy */
			"pandn	%%xmm0, %%xmm2\n\t"
#   ifndef __x86_64__
			"movdqa	%3, %%xmm0\n\t"
#   endif
			"por	%%xmm2, %%xmm1\n\t"       /* join */
#   ifndef __x86_64__
			"movdqa	16+%3, %%xmm2\n\t"
#   endif
			"psrlw	$0x3, %%xmm1\n\t"      /* bring it down */
#   ifdef __x86_64__
			"pand	%%xmm8, %%xmm1\n\t"       /* eliminate */
#   else
			"pand	%%xmm0, %%xmm1\n\t"       /* eliminate */
#   endif
			/* convert */
#   ifdef __x86_64__
			"paddb	%%xmm9, %%xmm1\n\t"
#   else
			"paddb	%%xmm2, %%xmm1\n\t"
#   endif
			"movdqa	%%xmm1, %%xmm0\n\t"
			"pcmpgtb	%%xmm3, %%xmm0\n\t"
			"pand	%%xmm4, %%xmm0\n\t"
			"psubb	%%xmm0, %%xmm1\n\t"
			/* write out */
			"cmp	$15, %2\n\t"
			"movdqa	%%xmm1, %%xmm0\n\t"
			"psrlw	$8, %%xmm0\n\t"
			"psllw	$8, %%xmm1\n\t"
			"por	%%xmm1, %%xmm0\n\t"
			"pshuflw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufhw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufd	$0x4e, %%xmm0, %%xmm0\n\t"
			"movdqu	%%xmm0,     (%0)\n\t"
			"lea	16(%0), %0\n\t"
			"ja	1b\n\t"
			"cmp	$8, %2\n\t"
			"jb	2f\n"
			"3:\n\t"
			"movq	(%1), %%xmm0\n\t"       /* fetch input data */
			"sub	$5, %2\n\t"
			"add	$5, %1\n\t"
			"cmp	$5, %2\n\t"
			"jb	4f\n\t"
			"pinsrw	$4, 3(%1), %%xmm0\n\t"
			"4:\n\t"
			/* swab endianess */
			"movdqa	%%xmm0, %%xmm1\n\t"
			"psrlw	$8, %%xmm0\n\t"
			"psllw	$8, %%xmm1\n\t"
			"por	%%xmm1, %%xmm0\n\t"
			"pshuflw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufhw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufd	$0x4e, %%xmm0, %%xmm0\n\t"
			/* partionate */
			"movdqa	%%xmm0, %%xmm1\n\t"    /* copy */
			"pslldq	$5, %%xmm0\n\t"        /* shift copy */
			"punpckhqdq	%%xmm1, %%xmm0\n\t" /* eliminate & join */
			"movdqa	%%xmm7, %%xmm2\n\t"
			"movdqa	%%xmm0, %%xmm1\n\t"    /* copy */
			"psrlq	$0xc, %%xmm0\n\t"      /* shift copy */
			"pand	%%xmm7, %%xmm1\n\t"       /* eliminate */
			"pandn	%%xmm0, %%xmm2\n\t"
			"por	%%xmm2, %%xmm1\n\t"       /* join */
			"movdqa	%%xmm6, %%xmm2\n\t"
			"movdqa	%%xmm1, %%xmm0\n\t"    /* copy */
			"pand	%%xmm6, %%xmm1\n\t"       /* eliminate */
			"psrld	$0x6, %%xmm0\n\t"      /* shift copy */
			"pandn	%%xmm0, %%xmm2\n\t"
			"por	%%xmm2, %%xmm1\n\t"       /* join */
			"movdqa	%%xmm5, %%xmm2\n\t"
			"movdqa	%%xmm1, %%xmm0\n\t"    /* copy */
			"pand	%%xmm5, %%xmm1\n\t"       /* eliminate */
			"psrlw	$0x3, %%xmm0\n\t"      /* shift copy */
			"pandn	%%xmm0, %%xmm2\n\t"
#   ifndef __x86_64__
			"movdqa	%3, %%xmm0\n\t"
#   endif
			"por	%%xmm2, %%xmm1\n\t"       /* join */
#   ifndef __x86_64__
			"movdqa	16+%3, %%xmm2\n\t"
#   endif
			"psrlw	$0x3, %%xmm1\n\t"      /* bring it down */
#   ifdef __x86_64__
			"pand	%%xmm8, %%xmm1\n\t"       /* eliminate */
#   else
			"pand	%%xmm0, %%xmm1\n\t"       /* eliminate */
#   endif
			/* convert */
#   ifdef __x86_64__
			"paddb	%%xmm9, %%xmm1\n\t"
#   else
			"paddb	%%xmm2, %%xmm1\n\t"
#   endif
			"movdqa	%%xmm1, %%xmm0\n\t"
			"pcmpgtb	%%xmm3, %%xmm0\n\t"
			"pand	%%xmm4, %%xmm0\n\t"
			"psubb	%%xmm0, %%xmm1\n\t"
			/* write out */
			"movdqa	%%xmm1, %%xmm0\n\t"
			"psrlw	$8, %%xmm0\n\t"
			"psllw	$8, %%xmm1\n\t"
			"por	%%xmm1, %%xmm0\n\t"
			"pshuflw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufhw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufd	$0x4e, %%xmm0, %%xmm0\n\t"
			"cmp	$5, %2\n\t"
			"jb	5f\n\t"
			"sub	$5, %2\n\t"
			"add	$5, %1\n\t"
			"movdqu	%%xmm0, (%0)\n\t"
			"lea	16(%0), %0\n\t"
			"jmp	6f\n"
			"5:\n\t"
			"movq	%%xmm0, (%0)\n\t"
			"lea	8(%0), %0\n"
			"6:\n\t"
			"cmp	$7, %2\n\t"
			"ja	3b\n"
			"2:"
		: /* %0 */ "=r" (dst),
		  /* %1 */ "=r" (src),
		  /* %2 */ "=r" (len)
		: /* %3 */ "m" (vals[4][0]),
		  /*    */ "0" (dst),
		  /*    */ "1" (src),
		  /*    */ "2" (len)
#  ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#   ifdef __x86_64__
		  , "xmm8", "xmm9"
#   endif
#  endif
	);
	if(len)
		return to_base32_generic(dst, src, len);
	return dst;
}
# endif
#endif

static unsigned char *to_base32_SSE2(unsigned char *dst, const unsigned char *src, unsigned len)
{
	asm (
			"cmp	$8, %2\n\t"
			"jb	2f\n\t"
			"movdqa	-48+%3, %%xmm7\n\t"
			"movdqa	-32+%3, %%xmm6\n\t"
			"movdqa	-16+%3, %%xmm5\n\t"
#ifdef __x86_64__
			"movdqa	%3, %%xmm8\n\t"
			"movdqa	16+%3, %%xmm9\n\t"
#endif
			"movdqa	32+%3, %%xmm3\n\t"
			"movdqa	48+%3, %%xmm4\n\t"
			"cmp	$16, %2\n\t"
			"jb	3f\n\t"
			".p2align 2\n"
			"1:\n\t"
			"movdqu	(%1), %%xmm0\n\t"       /* fetch input data */
			"sub	$10, %2\n\t"
			"add	$10, %1\n\t"
			/* swab endianess */
			"movdqa	%%xmm0, %%xmm1\n\t"
			"psrlw	$8, %%xmm0\n\t"
			"psllw	$8, %%xmm1\n\t"
			"por	%%xmm1, %%xmm0\n\t"
			"pshuflw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufhw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufd	$0x4e, %%xmm0, %%xmm0\n\t"
			/* partionate */
			"movdqa	%%xmm0, %%xmm1\n\t"    /* copy */
			"pslldq	$5, %%xmm0\n\t"        /* shift copy */
			"punpckhqdq	%%xmm1, %%xmm0\n\t" /* eliminate & join */
			"movdqa	%%xmm7, %%xmm2\n\t"
			"movdqa	%%xmm0, %%xmm1\n\t"    /* copy */
			"psrlq	$0xc, %%xmm0\n\t"      /* shift copy */
			"pand	%%xmm7, %%xmm1\n\t"       /* eliminate */
			"pandn	%%xmm0, %%xmm2\n\t"
			"por	%%xmm2, %%xmm1\n\t"       /* join */
			"movdqa	%%xmm6, %%xmm2\n\t"
			"movdqa	%%xmm1, %%xmm0\n\t"    /* copy */
			"pand	%%xmm6, %%xmm1\n\t"       /* eliminate */
			"psrld	$0x6, %%xmm0\n\t"      /* shift copy */
			"pandn	%%xmm0, %%xmm2\n\t"
			"por	%%xmm2, %%xmm1\n\t"       /* join */
			"movdqa	%%xmm5, %%xmm2\n\t"
			"movdqa	%%xmm1, %%xmm0\n\t"    /* copy */
			"pand	%%xmm5, %%xmm1\n\t"       /* eliminate */
			"psrlw	$0x3, %%xmm0\n\t"      /* shift copy */
			"pandn	%%xmm0, %%xmm2\n\t"
#ifndef __x86_64__
			"movdqa	%3, %%xmm0\n\t"
#endif
			"por	%%xmm2, %%xmm1\n\t"       /* join */
#ifndef __x86_64__
			"movdqa	16+%3, %%xmm2\n\t"
#endif
			"psrlw	$0x3, %%xmm1\n\t"      /* bring it down */
#ifdef __x86_64__
			"pand	%%xmm8, %%xmm1\n\t"       /* eliminate */
#else
			"pand	%%xmm0, %%xmm1\n\t"       /* eliminate */
#endif
			/* convert */
#ifdef __x86_64__
			"paddb	%%xmm9, %%xmm1\n\t"
#else
			"paddb	%%xmm2, %%xmm1\n\t"
#endif
			"movdqa	%%xmm1, %%xmm0\n\t"
			"pcmpgtb	%%xmm3, %%xmm0\n\t"
			"pand	%%xmm4, %%xmm0\n\t"
			"psubb	%%xmm0, %%xmm1\n\t"
			/* write out */
			"cmp	$15, %2\n\t"
			"movdqa	%%xmm1, %%xmm0\n\t"
			"psrlw	$8, %%xmm0\n\t"
			"psllw	$8, %%xmm1\n\t"
			"por	%%xmm1, %%xmm0\n\t"
			"pshuflw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufhw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufd	$0x4e, %%xmm0, %%xmm0\n\t"
			"movdqu	%%xmm0,     (%0)\n\t"
			"lea	16(%0), %0\n\t"
			"ja	1b\n\t"
			"cmp	$8, %2\n\t"
			"jb	2f\n"
			"3:\n\t"
			"movq	(%1), %%xmm0\n\t"       /* fetch input data */
			"sub	$5, %2\n\t"
			"add	$5, %1\n\t"
			"cmp	$5, %2\n\t"
			"jb	4f\n\t"
			"pinsrw	$4, 3(%1), %%xmm0\n\t"
			"4:\n\t"
			/* swab endianess */
			"movdqa	%%xmm0, %%xmm1\n\t"
			"psrlw	$8, %%xmm0\n\t"
			"psllw	$8, %%xmm1\n\t"
			"por	%%xmm1, %%xmm0\n\t"
			"pshuflw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufhw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufd	$0x4e, %%xmm0, %%xmm0\n\t"
			/* partionate */
			"movdqa	%%xmm0, %%xmm1\n\t"    /* copy */
			"pslldq	$5, %%xmm0\n\t"        /* shift copy */
			"punpckhqdq	%%xmm1, %%xmm0\n\t" /* eliminate & join */
			"movdqa	%%xmm7, %%xmm2\n\t"
			"movdqa	%%xmm0, %%xmm1\n\t"    /* copy */
			"psrlq	$0xc, %%xmm0\n\t"      /* shift copy */
			"pand	%%xmm7, %%xmm1\n\t"       /* eliminate */
			"pandn	%%xmm0, %%xmm2\n\t"
			"por	%%xmm2, %%xmm1\n\t"       /* join */
			"movdqa	%%xmm6, %%xmm2\n\t"
			"movdqa	%%xmm1, %%xmm0\n\t"    /* copy */
			"pand	%%xmm6, %%xmm1\n\t"       /* eliminate */
			"psrld	$0x6, %%xmm0\n\t"      /* shift copy */
			"pandn	%%xmm0, %%xmm2\n\t"
			"por	%%xmm2, %%xmm1\n\t"       /* join */
			"movdqa	%%xmm5, %%xmm2\n\t"
			"movdqa	%%xmm1, %%xmm0\n\t"    /* copy */
			"pand	%%xmm5, %%xmm1\n\t"       /* eliminate */
			"psrlw	$0x3, %%xmm0\n\t"      /* shift copy */
			"pandn	%%xmm0, %%xmm2\n\t"
#ifndef __x86_64__
			"movdqa	%3, %%xmm0\n\t"
#endif
			"por	%%xmm2, %%xmm1\n\t"       /* join */
#ifndef __x86_64__
			"movdqa	16+%3, %%xmm2\n\t"
#endif
			"psrlw	$0x3, %%xmm1\n\t"      /* bring it down */
#ifdef __x86_64__
			"pand	%%xmm8, %%xmm1\n\t"       /* eliminate */
#else
			"pand	%%xmm0, %%xmm1\n\t"       /* eliminate */
#endif
			/* convert */
#ifdef __x86_64__
			"paddb	%%xmm9, %%xmm1\n\t"
#else
			"paddb	%%xmm2, %%xmm1\n\t"
#endif
			"movdqa	%%xmm1, %%xmm0\n\t"
			"pcmpgtb	%%xmm3, %%xmm0\n\t"
			"pand	%%xmm4, %%xmm0\n\t"
			"psubb	%%xmm0, %%xmm1\n\t"
			/* write out */
			"movdqa	%%xmm1, %%xmm0\n\t"
			"psrlw	$8, %%xmm0\n\t"
			"psllw	$8, %%xmm1\n\t"
			"por	%%xmm1, %%xmm0\n\t"
			"pshuflw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufhw	$0x1b, %%xmm0, %%xmm0\n\t"
			"pshufd	$0x4e, %%xmm0, %%xmm0\n\t"
			"cmp	$5, %2\n\t"
			"jb	5f\n\t"
			"sub	$5, %2\n\t"
			"add	$5, %1\n\t"
			"movdqu	%%xmm0, (%0)\n\t"
			"lea	16(%0), %0\n\t"
			"jmp	6f\n"
			"5:\n\t"
			"movq	%%xmm0, (%0)\n\t"
			"lea	8(%0), %0\n"
			"6:\n\t"
			"cmp	$7, %2\n\t"
			"ja	3b\n"
			"2:"
		: /* %0 */ "=r" (dst),
		  /* %1 */ "=r" (src),
		  /* %2 */ "=r" (len)
		: /* %3 */ "m" (vals[4][0]),
		  /*    */ "0" (dst),
		  /*    */ "1" (src),
		  /*    */ "2" (len)
#ifdef __SSE__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
# ifdef __x86_64__
		  , "xmm8", "xmm9"
# endif
#endif
	);
	if(len)
		return to_base32_generic(dst, src, len);
	return dst;
}

#ifndef __x86_64__
static unsigned char *to_base32_SSE(unsigned char *dst, const unsigned char *src, unsigned len)
{
	asm (
			"cmp	$8, %2\n\t"
			"jb	2f\n\t"
			"movq	-48+%3, %%mm7\n\t"
			"movq	-32+%3, %%mm6\n\t"
			"movq	-16+%3, %%mm5\n\t"
			"movq	32+%3, %%mm3\n\t"
			"movq	48+%3, %%mm4\n\t"
			".p2align 2\n"
			"1:\n\t"
			"movq	(%1), %%mm0\n\t"       /* fetch input data */
			"sub	$5, %2\n\t"
			"add	$5, %1\n\t"
			/* swab endianess */
			"movq	%%mm0, %%mm1\n\t"
			"psrlw	$8, %%mm0\n\t"
			"psllw	$8, %%mm1\n\t"
			"por	%%mm1, %%mm0\n\t"
			"pshufw	$0x1b, %%mm0, %%mm0\n\t"
			/* partionate */
			"movq	%%mm0, %%mm1\n\t"      /* copy */
			"movq	%%mm7, %%mm2\n\t"
			"psrlq	$0xc, %%mm0\n\t"    /* shift copy */
			"pand	%%mm7, %%mm1\n\t"      /* eliminate */
			"pandn	%%mm0, %%mm2\n\t"
			"por	%%mm2, %%mm1\n\t"      /* join */
			"movq	%%mm6, %%mm2\n\t"
			"movq	%%mm1, %%mm0\n\t"      /* copy */
			"pand	%%mm6, %%mm1\n\t"      /* eliminate */
			"psrld	$0x6, %%mm0\n\t"    /* shift copy */
			"pandn	%%mm0, %%mm2\n\t"
			"por	%%mm2, %%mm1\n\t"      /* join */
			"movq	%%mm5, %%mm2\n\t"
			"movq	%%mm1, %%mm0\n\t"      /* copy */
			"pand	%%mm5, %%mm1\n\t"      /* eliminate */
			"psrlw	$0x3, %%mm0\n\t"    /* shift copy */
			"pandn	%%mm0, %%mm2\n\t"
			"movq	%3, %%mm0\n\t"
			"por	%%mm2, %%mm1\n\t"      /* join */
			"movq	16+%3, %%mm2\n\t"
			"psrlw	$0x3, %%mm1\n\t"    /* bring it down */
			"pand	%%mm0, %%mm1\n\t"      /* eliminate */
			/* convert */
			"paddb	%%mm2, %%mm1\n\t"
			"movq	%%mm1, %%mm0\n\t"
			"pcmpgtb	%%mm3, %%mm0\n\t"
			"pand	%%mm4, %%mm0\n\t"
			"psubb	%%mm0, %%mm1\n\t"
			/* write out */
			"cmp	$0x7, %2\n\t"
			"movq	%%mm1, %%mm0\n\t"
			"psrlw	$8, %%mm0\n\t"
			"psllw	$8, %%mm1\n\t"
			"por	%%mm1, %%mm0\n\t"
			"pshufw	$0x1b, %%mm0, %%mm0\n\t"
			"movq	%%mm0, (%0)\n\t"
			"lea	8(%0), %0\n\t"
			"ja	1b\n"
			"2:"
		: /* %0 */ "=r" (dst),
		  /* %1 */ "=r" (src),
		  /* %2 */ "=r" (len)
		: /* %3 */ "m" (vals[4][0]),
		  /*    */ "0" (dst),
		  /*    */ "1" (src),
		  /*    */ "2" (len)
# ifdef __MMX__
		: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
# endif
	);
	return to_base32_generic(dst, src, len);
}
#endif

static __init_cdata const struct test_cpu_feature tfeat_to_base32[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 222
	{.func = (void (*)(void))to_base32_BMI2,    .features = {
#  ifdef __i386__
			[1] = CFB(CFEATURE_SSE4_1),
#  else
			[0] = CFB(CFEATURE_SSE2),
#  endif
			[4] = CFB(CFEATURE_BMI2)
		}
	},
# endif
# if HAVE_BINUTILS >= 219
	{.func = (void (*)(void))to_base32_AVX,     .features = {[1] = CFB(CFEATURE_AVX)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 218
	{.func = (void (*)(void))to_base32_SSE41,   .features = {[1] = CFB(CFEATURE_SSE4_1)}},
# endif
# if HAVE_BINUTILS >= 217
	{.func = (void (*)(void))to_base32_SSE3,    .features = {[1] = CFB(CFEATURE_SSE3)}},
# endif
#endif
	{.func = (void (*)(void))to_base32_SSE2,    .features = {[0] = CFB(CFEATURE_SSE2)}},
#ifndef __x86_64__
	{.func = (void (*)(void))to_base32_SSE,     .features = {[0] = CFB(CFEATURE_SSE)}},
	{.func = (void (*)(void))to_base32_SSE,     .features = {[2] = CFB(CFEATURE_MMXEXT)}},
#endif
	{.func = (void (*)(void))to_base32_generic, .features = {}, .flags = CFF_DEFAULT},
};

DYN_JMP_DISPATCH(unsigned char *, to_base32, (unsigned char *dst, const unsigned char *src, unsigned len), (dst, src, len))

/*@unused@*/
static char const rcsid_tb32x[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
