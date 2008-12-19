/*
 * strnpcpy.c
 * strnpcpy for efficient concatination, x86 implementation
 *
 * Copyright (c) 2008 Jan Seiffert
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
 * We use the GCC vector internals, to make things simple for us.
 */

#if defined( __SSE__) && defined(__GNUC__)

# define TYPEM(x) ((x) + ((x)-1))

typedef char v8c __attribute__((vector_size (8)));
# define SOV8M1	(sizeof(v8c) - 1)
# define SOV8	(sizeof(v8c))

static inline unsigned v8_equal(v8c a, v8c b)
{
	v8c compare = __builtin_ia32_pcmpeqb(a, b);
	return __builtin_ia32_pmovmskb(compare);
}

/* don't inline, will only bloat, since its tail called */
static noinline GCC_ATTR_FASTCALL char *cpy_rest(char *dst, const char *src, unsigned i)
{
	uint16_t *dst_w = (uint16_t *)dst;
	const uint16_t *src_w = (const uint16_t *)src;
	uint32_t *dst_dw = (uint32_t *)dst;
	const uint32_t *src_dw = (const uint32_t *)src;
	uint64_t *dst_qw = (uint64_t *)dst;
	const uint64_t *src_qw = (const uint64_t *)src;

	switch(i)
	{
# ifdef __SSE2__
	case 15:
		dst[14] = src[14];
	case 14:
		dst_w[6] = src_w[6];
		goto C12;
	case 13:
		dst[12] = src[12];
	case 12:
C12:
		dst_dw[2] = src_dw[2];
		goto C8;
	case 11:
		dst[10] = src[10];
	case 10:
		dst_w[4] = src_w[4];
		goto C8;
	case  9:
		dst[8] = src[8];
	case  8:
C8:
		dst_qw[0] = src_qw[0];
		break;
#endif /* __SSE2__ */
	case  7:
		dst[6] = src[6];
	case  6:
		dst_w[2] = src_w[2];
		goto C4;
	case  5:
		dst[4] = src[4];
	case  4:
C4:
		dst_dw[0] = src_dw[0];
		break;
	case  3:
		dst[2] = src[2];
	case  2:
		dst_w[0] = src_w[0];
		break;
	case  1:
		dst[0] = src[0];
	case  0:
		break;
	}
	dst[i] = '\0';
	return dst + i;
}

# ifdef __SSE2__
/*
 * SSE2 version
 */

typedef char v16c __attribute__((vector_size (16)));
#  define SOV16M1	(sizeof(v16c) - 1)
#  define SOV16	(sizeof(v16c))

static inline unsigned v16_equal(v16c a, v16c b)
{
	v16c compare = __builtin_ia32_pcmpeqb128(a, b);
	return __builtin_ia32_pmovmskb128(compare);
}

char *strnpcpy(char *dst, const char *src, size_t maxlen)
{
	size_t i = 0, cycles;
	unsigned r;

	prefetch(src);
	prefetchw(dst);
	prefetch((char *)cpy_rest);
	prefetch((char *)cpy_rest + 64);
	prefetch((char *)cpy_rest + 128);
	if(unlikely(!src || !dst || !maxlen))
		return dst;

	/*
	 * We don't check for alignment, 16 is very unlikely.
	 * First we work unaligned until the first page boundery,
	 * so we start doing something for short strings.
	 */
	i = ((char *)ALIGN(src, 4096) - src);
	i = i < maxlen ? i : maxlen;
CPY_NEXT:
	for(r = 0, cycles = i; likely(SOV16M1 < i);
	    i -= SOV16, src += SOV16, dst += SOV16) {
		v16c x = __builtin_ia32_loaddqu(src);
		if(unlikely(r = v16_equal(x, (v16c){0ULL, 0ULL})))
			break;
		__builtin_ia32_storedqu(dst, x);
	}
	barrier();
	if(likely(r))
		return cpy_rest(dst, src, __builtin_ctz(r));
	maxlen -= (cycles - i);
	if(likely(SOV8M1 < i))
	{
	/*
	 * ATM gcc 4.3.2 generates HORRIBLE MMX code (SSE is OK...)
	 * Somehow he is scared to death and spills his operands
	 * like crazy to the stack. Wow, thats cool in a copy loop...
	 * Looks like the loop sheduler fucks it up trying to
	 * generate a loop he can "jump into".
	 */
		v8c x, x2;

		asm(
			"movq	%4, %1\n\t"
			"movq	%1, %0\n\t"
			"pcmpeqb	%6, %1\n\t"
			"pmovmskb	%1, %2\n\t"
			"test	%2, %2\n\t"
			"jnz	1f\n\t"
			"movq	%0, %5\n"
			"1:\n"
		: /* %0 */ "=y" (x),
		  /* %1 */ "=y" (x2),
		  /* %2 */ "=&r" (r),
		  /* %3 */ "=m" (*dst)
		: /* %4 */ "m" (*src),
		  /* %5 */ "m" (*dst),
		  /* %6 */ "y" ((v8c){0ULL})
		);
		if(likely(r))
			return cpy_rest(dst, src, __builtin_ctz(r));
		i -= SOV8;
		maxlen -= SOV8;
		src += SOV8;
		dst += SOV8;
	}
	if(likely(SO32M1 < i))
	{
		uint32_t c = *(const uint32_t *)src;
		r = has_nul_byte32(c);
		if(likely(r))
			return cpy_rest(dst, src, __builtin_ctz(r) / 8);
		*(uint32_t *)dst = c;
		i -= SO32;
		maxlen -= SO32;
		src += SO32;
		dst += SO32;
	}

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

# else
/*
 * SSE version
 */
char *strnpcpy(char *dst, const char *src, size_t maxlen)
{
	size_t i = 0, cycles;
	unsigned r;

	prefetch(src);
	prefetchw(dst);
	if(unlikely(!src || !dst || !maxlen))
		return dst;

	/*
	 * We don't check for alignment, 8 is very unlikely.
	 * First we work unaligned until the first page boundery,
	 * so we start doing something for short strings.
	 */
	i = ((char *)ALIGN(src, 4096) - src);
	i = i < maxlen ? i : maxlen;
CPY_NEXT:
	for(r = 0, cycles = i; likely(SOV8M1 < i);
	    i -= SOV8, src += SOV8, dst += SOV8) {
		v8c x = *(const v8c *)src;
		if(unlikely(r = v8_equal(x, (v8c){0ULL})))
			break;
		*(v8c *)dst = x;
	}
	barrier();
	if(likely(r))
		return cpy_rest(dst, src, __builtin_ctz(r));
	maxlen -= (cycles - i);
	if(likely(SO32M1 < i))
	{
		uint32_t c = *(uint32_t *)src;
		r = unlikely(has_nul_byte32(c));
		if(r)
			return cpy_rest(dst, src, __builtin_ctz(r) / 8);
		*(uint32_t *)dst = c;
		i -= SO32;
		maxlen -= SO32;
		src += SO32;
		dst += SO32;
	}

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
# endif
static char const rcsid_snpcg[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/strnlen.c"
#endif
/* EOF */
