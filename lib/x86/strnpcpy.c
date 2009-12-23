/*
 * strnpcpy.c
 * strnpcpy for efficient concatination, x86 implementation
 *
 * Copyright (c) 2008-2009 Jan Seiffert
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
 * $Id: $
 */

/*
 * Intel has a good feeling how to create incomplete and
 * unsymetric instruction sets, so sometimes there is a
 * last "missing link".
 * Hammering on multiple integers was to be implemented with
 * MMX, but SSE finaly brought usefull comparison, but not
 * for 128 Bit SSE datatypes, only the old 64 Bit MMX...
 * SSE2 finaly brought fun for 128Bit.
 *
 * I would have a pure MMX version, but it is slower than the
 * generic version (58000ms vs. 54000ms), MMX was a really
 * braindead idea without a MMX->CPU-Reg move instruction
 * to do things not possible with MMX.
 *
 */
/*
 * We don't check for alignment, 16 or 8 is very unlikely.
 * We always busily start doing something to please short strings.
 * First we work unaligned until the first page boundery,
 * then we can see what to do.
 */
/*
 * Normaly we could generate this with the gcc vector
 * __builtins, but since gcc whines when the -march does
 * not support the operation and we want to always provide
 * them and switch between them...
 */

#include "x86_features.h"

#define SOV8	8
#define SOV8M1	(SOV8-1)
#define SOV16	16
#define SOV16M1	(SOV16-1)

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 218
static char *strnpcpy_SSE42(char *dst, const char *src, size_t maxlen);
# endif
#endif
static char *strnpcpy_SSE2(char *dst, const char *src, size_t maxlen);
#ifndef __x86_64__
static char *strnpcpy_SSE(char *dst, const char *src, size_t maxlen);
#endif
static char *strnpcpy_x86(char *dst, const char *src, size_t maxlen);

#define cpy_one_u32(dst, src, i, maxlen) \
	if(likely(SO32M1 < i)) \
	{ \
		uint32_t c = *(const uint32_t *)src; \
		uint32_t r32 = has_nul_byte32(c); \
		if(likely(r32)) {\
			asm ("bsf %1, %0" : "=r" (r32) : "r" (r32) : "cc"); \
			return cpy_rest0(dst, src, r32 / BITS_PER_CHAR); \
		} \
		*(uint32_t *)dst = c; \
		i -= SO32; \
		maxlen -= SO32; \
		src += SO32; \
		dst += SO32; \
	}

#ifndef __x86_64__
# define cpy_one_u64(dst, src, i, maxlen) \
	if(likely(SOV8M1 < i)) \
	{ \
		unsigned rsse; \
		asm( \
			"pxor	%%mm0, %%mm0\n\t" \
			"movq	%2, %%mm1\n\t" \
			"pcmpeqb	%%mm1, %%mm0\n\t" \
			"pmovmskb	%%mm0, %0\n\t" \
			"test	%0, %0\n\t" \
			"jnz	1f\n\t" \
			"movq	%%mm1, %3\n" \
			"1:\n" \
		: /* %0 */ "=&r" (rsse), \
		  /* %1 */ "=m" (*dst) \
		: /* %2 */ "m" (*src), \
		  /* %3 */ "m" (*dst) \
		); \
		if(likely(rsse)) { \
			asm ("bsf %1, %0" : "=r" (rsse) : "r" (rsse) : "cc"); \
			return cpy_rest0(dst, src, rsse); \
		} \
		i -= SOV8; \
		maxlen -= SOV8; \
		src += SOV8; \
		dst += SOV8; \
	}
#else
# define cpy_one_u64(dst, src, i, maxlen) \
	if(likely(SOSTM1 < i)) \
	{ \
		size_t c = *(const size_t *)src; \
		size_t r64 = has_nul_byte(c); \
		if(likely(r64)) { \
			asm ("bsf %1, %0" : "=r" (r64) : "r" (r64) : "cc"); \
			return cpy_rest0(dst, src, r64 / BITS_PER_CHAR); \
		} \
		*(size_t *)dst = c; \
		i -= SOST; \
		maxlen -= SOST; \
		src += SOST; \
		dst += SOST; \
	}
#endif

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 218
static char *strnpcpy_SSE42(char *dst, const char *src, size_t maxlen)
{
	size_t i;
	size_t r;

	prefetch(src);
	prefetchw(dst);
	if(unlikely(!src || !dst || !maxlen))
		return dst;

	i = ALIGN_DIFF(src, 4096);
	i = i ? i : 4096;
	i = i < maxlen ? i : maxlen;
CPY_NEXT:
	if(likely(SOV16M1 < i))
	{
		size_t cycles = i;
		asm (
				"mov	$0xFF01, %k3\n\t"
				"movd	%k3, %%xmm1\n\t"
				"xor	%3, %3\n\t"
				"jmp	1f\n"
				".p2align 2\n\t"
				"3:\n\t"
				"sub	$16, %0\n\t"
				"add	$16, %1\n\t"
				"movdqu	%%xmm2, (%2)\n\t"
				"add	$16, %2\n\t"
				"cmp	$16, %0\n\t" /* check remaining length */
				"jb	2f\n"
				"1:\n\t"
				"movdqu	(%1), %%xmm2\n\t"
				/* Byte,Masked+,Range,Bytes */
				/*             6543210 */
				"pcmpistrm	$0b1100100, %%xmm2, %%xmm1\n\t"
				"jnz	3b\n\t" /* if no zero byte */
				"movdqa	%%xmm0, %%xmm1\n\t"
				"pmovmskb	%%xmm0, %3\n\t"
				"pslldq	$1, %%xmm1\n\t"
				"por	%%xmm1, %%xmm0\n\t"
				"maskmovdqu	%%xmm0, %%xmm2\n\t"
				"not	%3\n\t"
				"bsf	%3, %3\n\t"
				"add	%3, %2\n"
				"2:"
			: /* %0 */ "=r" (i),
			  /* %1 */ "=r" (src),
			  /* %2 */ "=D" (dst),
			  /* %3 */ "=r" (r)
			: /* %4 */ "0" (i),
			  /* %5 */ "1" (src),
			  /* %6 */ "2" (dst),
			  /* %7 */ "3" (0)
#  ifdef __SSE__
			: "xmm0", "xmm1", "xmm2"
#  endif
		);
		if(likely(r))
			return dst;
		maxlen -= (cycles - i);
	}
	cpy_one_u64(dst, src, i, maxlen);
	cpy_one_u32(dst, src, i, maxlen);

	/* slowly go over the page boundry */
	for(; i && *src; i--, maxlen--)
		*dst++ = *src++;

	/* src is aligned, life is good... */
	if(likely(*src) && likely(maxlen)) {
		i = maxlen;
		/* since only src is aligned, simply continue with movdqu */
		goto CPY_NEXT;
	}

	if(likely(maxlen))
		*dst = '\0';
	return dst;
}
# endif
#endif

static char *strnpcpy_SSE2(char *dst, const char *src, size_t maxlen)
{
	size_t i;
	unsigned r;

	prefetch(src);
	prefetchw(dst);
	if(unlikely(!src || !dst || !maxlen))
		return dst;

	i = ALIGN_DIFF(src, 4096);
	i = i ? i : 4096;
	i = i < maxlen ? i : maxlen;
CPY_NEXT:
	if(likely(SOV16M1 < i))
	{
		size_t cycles = i;
		asm (
			"pxor	%%xmm2, %%xmm2\n\t"
			"jmp	1f\n"
			".p2align 2\n\t"
			"3:\n\t"
			"sub	$16, %0\n\t"
			"add	$16, %1\n\t"
			"movdqu	%%xmm1, (%2)\n\t"
			"add	$16, %2\n\t"
			"cmp	$15, %0\n\t"
			"jbe	2f\n"
			"1:\n\t"
			"movdqu	(%1), %%xmm1\n\t"
			"movdqa	%%xmm1, %%xmm0\n\t"
			"pcmpeqb	%%xmm2, %%xmm0\n\t"
			"pmovmskb	%%xmm0, %3\n\t"
			"test	%3, %3\n\t"
			"jz	3b\n"
		/* there is maskmovdqu since sse2, but creating the mask is a PITA */
			"2:"
		: /* %0 */ "=r" (i),
		  /* %1 */ "=r" (src),
		  /* %2 */ "=r" (dst),
		  /* %3 */ "=r" (r)
		: /* %4 */ "0" (i),
		  /* %5 */ "1" (src),
		  /* %6 */ "2" (dst)
#ifdef __SSE__
		: "xmm0", "xmm1", "xmm2"
#endif
		);
		if(likely(r)) {
			asm ("bsf %1, %0" : "=r" (r) : "r" (r) : "cc");
			return cpy_rest0(dst, src, r);
		}
		maxlen -= (cycles - i);
	}
	cpy_one_u64(dst, src, i, maxlen);
	cpy_one_u32(dst, src, i, maxlen);

	/* slowly go over the page boundry */
	for(; i && *src; i--, maxlen--)
		*dst++ = *src++;

	/* src is aligned, life is good... */
	if(likely(*src) && likely(maxlen)) {
		i = maxlen;
		/* since only src is aligned, simply continue with movdqu */
		goto CPY_NEXT;
	}

	if(likely(maxlen))
		*dst = '\0';
	return dst;
}

#ifndef __x86_64__
static char *strnpcpy_SSE(char *dst, const char *src, size_t maxlen)
{
	size_t i;
	unsigned r;

	prefetch(src);
	prefetchw(dst);
	if(unlikely(!src || !dst || !maxlen))
		return dst;

	i = ALIGN_DIFF(src, 4096);
	i = i ? i : 4096;
	i = i < maxlen ? i : maxlen;
CPY_NEXT:
	if(SOV8M1 < i)
	{
		size_t cycles = i;
		asm (
			"pxor	%%mm2, %%mm2\n\t"
			"jmp	1f\n"
			".p2align 2\n\t"
			"3:\n\t"
			"sub	$8, %0\n\t"
			"add	$8, %1\n\t"
			"movq	%%mm1, (%2)\n\t"
			"add	$8, %2\n\t"
			"cmp	$7, %0\n\t"
			"jbe	2f\n"
			"1:\n\t"
			"movq	(%1), %%mm1\n\t"
			"movq	%%mm1, %%mm0\n\t"
			"pcmpeqb	%%mm2, %%mm0\n\t"
			"pmovmskb	%%mm0, %3\n\t"
			"test	%3, %3\n\t"
			"jz	3b\n"
			"2:"
		: /* %0 */ "=r" (i),
		  /* %1 */ "=r" (src),
		  /* %2 */ "=r" (dst),
		  /* %3 */ "=r" (r)
		: /* %4 */ "0" (i),
		  /* %5 */ "1" (src),
		  /* %6 */ "2" (dst)
#ifdef __SSE__
		: "mm0", "mm1", "mm2"
#endif
		);
		if(likely(r)) {
			asm ("bsf %1, %0" : "=r" (r) : "r" (r) : "cc");
			return cpy_rest0(dst, src, r);
		}
		maxlen -= (cycles - i);
	}
	cpy_one_u32(dst, src, i, maxlen);

	/* slowly go over the page boundry */
	for(; i && *src; i--, maxlen--)
		*dst++ = *src++;

	/* src is aligned, life is good... */
	if(likely(*src) && likely(maxlen)) {
		i = maxlen;
		goto CPY_NEXT;
	}

	if(likely(maxlen))
		*dst = '\0';
	return dst;
}
#endif

static char *strnpcpy_x86(char *dst, const char *src, size_t maxlen)
{
	size_t i;

	prefetch(src);
	prefetchw(dst);
	if(unlikely(!src || !dst || !maxlen))
		return dst;

CPY_NEXT:
	i = ALIGN_DIFF(src, 4096);
	i = i ? i : 4096;
	i = i < maxlen ? i : maxlen;
	if(SOSTM1 < i)
	{
		size_t cycles = i;
		size_t r = 0;
		do
		{
			size_t c = *(const size_t *)src;
			r = has_nul_byte(c);
			if(r)
				break;
			*(size_t *)dst = c;
			i -= SOST;
			src += SOST;
			dst += SOST;
		} while(SOSTM1 < i);
		if(likely(r)) {
			asm ("bsf %1, %0" : "=r" (r) : "r" (r) : "cc");
			return cpy_rest0(dst, src, r / BITS_PER_CHAR);
		}
		maxlen -= (cycles - i);
	}
#ifdef __x86_64__
	cpy_one_u32(dst, src, i, maxlen);
#endif

	/* slowly go over the page boundry */
	for(; i && *src; i--, maxlen--)
		*dst++ = *src++;

	/* src is aligned, life is good... */
	if(likely(*src) && likely(maxlen)) 
	{
		/*
		 * we have done one page, maybe the string is longer.
		 * Now align dst (faster write) and work till next page
		 * boundery.
		 */
		i = ALIGN_DIFF(dst, SOST);
		i = i < maxlen ? i : maxlen;
#ifdef __x86_64__
		cpy_one_u32(dst, src, i, maxlen);
#endif
		for(; i && *src; i--, maxlen--)
			*dst++ = *src++;
		goto CPY_NEXT;
	}

	if(likely(maxlen))
		*dst = '\0';
	return dst;
}

static const struct test_cpu_feature t_feat[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 218
	{.func = (void (*)(void))strnpcpy_SSE42, .flags_needed = CFEATURE_SSE4_2, .callback = NULL},
# endif
#endif
	{.func = (void (*)(void))strnpcpy_SSE2, .flags_needed = CFEATURE_SSE2, .callback = NULL},
#ifndef __x86_64__
	{.func = (void (*)(void))strnpcpy_SSE, .flags_needed = CFEATURE_SSE, .callback = NULL},
#endif
	{.func = (void (*)(void))strnpcpy_x86, .flags_needed = -1, .callback = NULL},
};

static char *strnpcpy_runtime_sw(char *dst, const char *src, size_t maxlen);
/*
 * Func ptr
 */
static char *(*strnpcpy_ptr)(char *dst, const char *src, size_t maxlen) = strnpcpy_runtime_sw;

/*
 * constructor
 */
static GCC_ATTR_CONSTRUCT void strnpcpy_select(void)
{
	strnpcpy_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer failes
 */
static char *strnpcpy_runtime_sw(char *dst, const char *src, size_t maxlen)
{
	strnpcpy_select();
	return strnpcpy_ptr(dst, src, maxlen);
}

char *strnpcpy(char *dst, const char *src, size_t maxlen)
{
	return strnpcpy_ptr(dst, src, maxlen);
}

static char const rcsid_snpcg[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
