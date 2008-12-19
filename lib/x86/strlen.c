/*
 * strlen.c
 * strlen, x86 implementation
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

size_t strlen(const char *s)
{
	const char *p = s;
	/* don't step over the page boundery */
	size_t cycles;
	prefetch(s);

	if(unlikely(!s))
		return 0;

	cycles = (const char *)ALIGN(p, 4096) - p;
	/*
	 * we don't precheck for alignment, 16 is very unlikely
	 * instead go unaligned until a page boundry
	 */
	for(; likely(SOV16M1 < cycles); cycles -= SOV16, p += SOV16)
	{
		unsigned r = v16_equal(__builtin_ia32_loaddqu(p), (v16c){0ULL, 0ULL});
		if(r) {
			p += __builtin_ctz(r);
			goto OUT;
		}
	}
	for(; likely(SO32M1 < cycles); cycles -= SO32, p += SO32)
	{
		uint32_t r = has_nul_byte32(*(const uint32_t *)p);
		if(r) {
			p += __builtin_ctz(r) / 8;
			goto OUT;
		}
	}
	/* slowly encounter page boundery */
	while(cycles && *p)
		p++, cycles--;

	if(likely(*p))
	{
		register const v16c *d = (const v16c *)p;
		unsigned r;
		while(!(r = v16_equal(*d, (v16c){0ULL, 0ULL})))
			d++;
		p  = (const char *)d;
		p += __builtin_ctz(r);
	}
OUT:
	return p - s;
}

# else
/*
 * SSE version
 */

typedef char v8c __attribute__((vector_size (8)));
#define SOV8M1	(sizeof(v8c) - 1)
#define SOV8	(sizeof(v8c))

static inline unsigned v8_equal(v8c a, v8c b)
{
	v8c compare = __builtin_ia32_pcmpeqb(a, b);
	return __builtin_ia32_pmovmskb(compare);
}

size_t strlen(const char *s)
{
	const char *p = s;
	/* don't step over the page boundery */
	size_t cycles;
	prefetch(s);

	if(unlikely(!s))
		return 0;

	cycles = (const char *)ALIGN(p, 4096) - p;
	/*
	 * we don't precheck for alignment, 8 is unlikely
	 * instead go unaligned until a page boundry
	 */
	for(; likely(SOV8M1 < cycles); cycles -= SOV8, p += SOV8)
	{
		unsigned r = v8_equal(*(v8c *)p, (v8c){0ULL});
		if(r) {
			p += __builtin_ctz(r);
			goto OUT;
		}
	}
	if(likely(SO32M1 < cycles))
	{
		uint32_t r = has_nul_byte32(*(uint32_t *)p);
		if(r) {
			p += __builtin_ctz(r) / 8;
			goto OUT;
		}
		cycles -= SO32;
		p += SO32;
	}
	while(cycles && *p)
		p++, cycles--;

	if(*p)
	{
		register const v8c *d = (const v8c *)p;
		unsigned r;
		while(!(r = v8_equal(*d, (v8c){0ULL})))
			d++;
		p  = (const char *)d;
		p += __builtin_ctz(r);
	}
OUT:
	return p - s;
}

# endif
static char const rcsid_sl[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/strlen.c"
#endif
/* EOF */
