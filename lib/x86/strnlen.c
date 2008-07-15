/*
 * strnlen.c
 * strnlen for non-GNU platforms, x86 implementation
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
 * Intel has a good feeleing how to create incomplete and
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

static inline int v16_equal(v16c a, v16c b)
{
	v16c compare = __builtin_ia32_pcmpeqb128(a, b);
	return __builtin_ia32_pmovmskb128(compare);
}

size_t strnlen(const char *s, size_t maxlen)
{
	const char *p = s;

	if(!IS_ALIGN(p, SOV16))
	{
		size_t i = (const char *)ALIGN(p,SOV16) - p;
		while(maxlen && *p && i)
			maxlen--, p++, i--;
	}
	if(SOV16 > maxlen || !*p)
		goto OUT;

	{
	register const v16c *d = ((const v16c *)p);
	while(!v16_equal(*d, (v16c){0ULL, 0ULL}) && SOV16M1 < maxlen)
		d++, maxlen -= SOV16;
	p = (const char *)d;
	}

OUT:
	while(maxlen && *p)
		maxlen--, p++;

	return p - s;
}

# else
/* 
 * SSE version 
 */

typedef char v8c __attribute__((vector_size (8)));
#define SOV8M1	(sizeof(v8c) - 1)
#define SOV8	(sizeof(v8c))

static inline int v8_equal(v8c a, v8c b)
{
	v8c compare = __builtin_ia32_pcmpeqb(a, b);
	return __builtin_ia32_pmovmskb(compare);
}

size_t strnlen(const char *s, size_t maxlen)
{
	const char *p = s;
	if(!IS_ALIGN(p, SOV8))
	{
		size_t i = (const char *)ALIGN(p, SOV8) - p;
		while(maxlen && *p && i)
			maxlen--, p++, i--;
	}
	if(sizeof(v8c) > maxlen || !*p)
		goto OUT;

	{
	register const v8c *d = ((const v8c *)p);
	while(!v8_equal(*d, (v8c){0ULL}) && SOV8M1 < maxlen)
		d++, maxlen -= SOV8;
	p = (const char *)d;
	}

OUT:
	while(maxlen && *p)
		maxlen--, p++;

	return p - s;
}

# endif
static char const rcsid_snl[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/strnlen.c"
#endif
