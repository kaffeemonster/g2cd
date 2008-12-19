/*
 * mem_searchrn.c
 * search mem for a \r\n, x86 implementation
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

typedef char v8c __attribute__((vector_size (8)));
#define SOV8M1	(sizeof(v8c) - 1)
#define SOV8	(sizeof(v8c))

static inline unsigned v8_equal(v8c a, v8c b)
{
	v8c compare = __builtin_ia32_pcmpeqb(a, b);
	return __builtin_ia32_pmovmskb(compare);
}

# ifdef __SSE2__
/*
 * SSE2 version
 */

typedef char v16c __attribute__((vector_size (16)));
#define SOV16M1	(sizeof(v16c) - 1)
#define SOV16	(sizeof(v16c))

static inline unsigned v16_equal(v16c a, v16c b)
{
	v16c compare = __builtin_ia32_pcmpeqb128(a, b);
	return __builtin_ia32_pmovmskb128(compare);
}

void *mem_searchrn(void *src, size_t len)
{
	static const v16c y = (const v16c) {'\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r',
	                                    '\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r'};
	static const v16c z = (const v16c) {'\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n',
	                                    '\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n'};
	char *src_c = src;
	unsigned last_rr;

	prefetch(src_c);
	if(unlikely(!src_c || !len))
		return NULL;

	/*
	 * we work our way unaligned forward, this func
	 * is not meant for too long mem regions
	 */
	for(last_rr = 0; likely(SOV16M1 < len); src_c += SOV16, len -= SOV16)
	{
		register v16c x = __builtin_ia32_loaddqu(src_c);
		unsigned rr = v16_equal(x, y);
		unsigned rn = v16_equal(x, z);
		last_rr &= rn;
		if(last_rr)
			return src_c - 1;
		last_rr = rr >> 15;
		rr &= rn >> 1;
		if(rr)
			return src_c + __builtin_ctz(rr);
	}
	if(last_rr) {
		src_c -= 1;
		goto OUT;
	}
	if(likely(SOV8M1 < len))
	{
		register v8c x, x2;
		unsigned rr, rn;

		/* force compiler to keep stuff in registers... */
		asm (
			"movq	%6, %2\n\t"
			"movq	%2, %3\n\t"
			"pcmpeqb	%4, %2\n\t"
			"pcmpeqb	%5, %3\n\t"
			"pmovmskb	%2, %0\n\t"
			"pmovmskb	%3, %1\n"
		: /* %0 */ "=r" (rr),
		  /* %1 */ "=r" (rn),
		  /* %2 */ "=y" (x),
		  /* %3 */ "=y" (x2)
		: /* %4 */ "m" (y),
		  /* %5 */ "m" (z),
		  /* %6 */ "m" (*src_c)
		);
		last_rr = rr >> 7;
		rr &= rn >> 1;
		if(rr)
			return src_c + __builtin_ctz(rr);
		len -= SOV8;
		src_c += SOV8;
	}
	if(last_rr) {
		src_c -= 1;
		goto OUT;
	}
	if(SO32M1 < len)
	{
		uint32_t v = *((uint32_t *)src_c) ^ 0x0D0D0D0D; /* '\r\r\r\r' */
		uint32_t r = has_nul_byte32(v);
		unsigned i = SO32;
		if(r)
			i  = __builtin_ctz(r) / 8;
		len   -= i;
		src_c += i;
	}

OUT:
	for(; len; len--, src_c++) {
		if('\r' == *src_c && len-1 && '\n' == src_c[1])
			return src_c;
	}
	return NULL;
}

# else
/*
 * SSE version
 */

void *mem_searchrn_SSE(void *src, size_t len)
{
	static const v8c y = (const v8c){'\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r'};
	static const v8c z = (const v8c){'\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n'};
	char *src_c = src;
	unsigned last_rr;

	prefetch(src_c);
	if(unlikely(!src_c || !len))
		return NULL;

	/*
	 * we work our way unaligned forward, this func
	 * is not meant for too long mem regions
	 */
	for(last_rr = 0; likely(SOV8M1 < len); len -= SOV8, src_c += SOV8)
	{
		register v8c x = *(v8c *)src_c;
		unsigned rr = v8_equal(x, y);
		unsigned rn = v8_equal(x, z);
		last_rr &= rn;
		if(last_rr)
			return src_c - 1;
		last_rr = rr >> 7;
		rr &= rn >> 1;
		if(rr)
			return src_c + __builtin_ctz(rr);
	}
	if(last_rr) {
		src_c -= 1;
		goto OUT;
	}
	if(SO32M1 < len)
	{
		uint32_t v = *((uint32_t *)src_c) ^ 0x0D0D0D0D; /* '\r\r\r\r' */
		uint32_t r = has_nul_byte32(v);
		unsigned i = SO32;
		if(r)
			i  = __builtin_ctz(r) / 8;
		len   -= i;
		src_c += i;
	}

OUT:
	for(; len; len--, src_c++) {
		if('\r' == *src_c && len-1 && '\n' == src_c[1])
			return src_c;
	}
	return NULL;
}
# endif
static char const rcsid_msrn[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/mem_searchrn.c"
#endif
/* EOF */
