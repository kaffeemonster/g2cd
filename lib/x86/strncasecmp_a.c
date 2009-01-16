/*
 * strncasecmp_a.c
 * strncasecmp ascii only, x86 implementation
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

#include "x86_features.h"

static const unsigned char tab[256] =
{
/*	  0     1     2     3     4     5     6     7         */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 07 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 17 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 1F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 27 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 2F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 37 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 3F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 47 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 4F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 57 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 5F */
	0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, /* 67 */
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, /* 6F */
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, /* 77 */
	0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, /* 7F */
};
static const char sma[16] GCC_ATTR_ALIGNED(16) =
{
	0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
	0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60
};
static const char smz[16] GCC_ATTR_ALIGNED(16) =
{
	0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A,
	0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A
};
static const char ucd[16] GCC_ATTR_ALIGNED(16) =
{
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
};

static int strncasecmp_a_SSE42(const char *s1, const char *s2, size_t n)
{
	size_t m1;
	size_t i, j, cycles;
	unsigned c1, c2;

	prefetch(ucd);
	prefetch(s1);
	prefetch(s2);

	if(unlikely(!n))
		return 0;

LOOP_AGAIN:
	i = ALIGN_DIFF(s1, 4096);
	i = i ? i : 4096;
	j = ALIGN_DIFF(s2, 4096);
	j = j ? j : i;
	i = i < j ? i : j;
	j = ROUND_ALIGN(n, 16);
	i = i < j ? i : j;

// TODO: untested
	cycles = i;
	asm (
		"cmp	$16, %2\n\t"
		"jb	3f\n\t"
		"pxor	%%xmm0, %%xmm1\n\t"
		"mov	0x7A60, %k3\n\t"
		"pinsrw	$0, %3, %%xmm1\n\t"
		"movdqa	%4, %%xmm2\n\t"
		"jmp	2f\n"
		"1:\n\t"
		"add	$16, %0\n\t"
		"add	$16, %1\n"
		"cmp	$16, %2\n\t"
		"jbe	3f\n\t"
		"2:\n\t"
		"sub	$16, %2\n\t"
		/* s1 */
		"movdqu	(%0), %%xmm7\n\t"
		"pcmpistrm	$0b1100100, %%xmm1, %%xmm7\n\t"
		"pand	%%xmm2, %%xmm0\n\t"
		"psubb	%%xmm0, %%xmm7\n\t"
		/* s2 */
		"movdqu	(%1), %%xmm6\n\t"
		"pcmpistrm	$0b1100100, %%xmm1, %%xmm6\n\t"
		"pand	%%xmm2, %%xmm0\n\t"
		"psubb	%%xmm0, %%xmm6\n\t"
		/* s1 ^ s2 */
		"pcmpistri	$0b0111000, %%xmm7, %%xmm6\n\t"
		"js	3f\n\t"
		"jz	3f\n\t"
		"jnc	1b\n"
		"3:\n\t"
	: /* %0 */ "=r" (s1),
	  /* %1 */ "=r" (s2),
	  /* %2 */ "=r" (i),
	  /* %3 */ "=c" (m1)
	: /* %4 */ "m" (*ucd),
	  /*  */ "0" (s1),
	  /*  */ "1" (s2),
	  /*  */ "2" (i),
	  /*  */ "3" (16)
	);
	cycles = ((cycles - i) / 16) * 16;
	if(likely(m1 < 16)) {
		if(cycles >= n)
			cycles -= 16;
		n -= cycles;
		m1 = m1 < n - 1 ? m1 : n - 1;
		return s1[m1] - s2[m1];
	}
	if(cycles >= n)
		return 0;
	n -= cycles;

	i = ALIGN_DIFF(s1, 4096);
	i = i ? i : 4096;
	j = ALIGN_DIFF(s2, 4096);
	j = j ? j : i;
	i = i < j ? i : j;
	i = i < n ? i : n;

	for(; i; i--, n--)
	{
		c1 = (unsigned) *s1++;
		c2 = (unsigned) *s2++;
		n--;
		c1 -= tab[c1];
		c2 -= tab[c2];
		if(!(c1 && c2 && c1 == c2))
			return (int)c1 - (int)c2;
	}

	if(n)
		goto LOOP_AGAIN;

	return 0;
}

static int strncasecmp_a_SSE2(const char *s1, const char *s2, size_t n)
{
	size_t m1;
	size_t i, j, cycles;
	unsigned c1, c2;

	prefetch(ucd);
	prefetch(s1);
	prefetch(s2);

	if(unlikely(!n))
		return 0;

LOOP_AGAIN:
	i = ALIGN_DIFF(s1, 4096);
	i = i ? i : 4096;
	j = ALIGN_DIFF(s2, 4096);
	j = j ? j : i;
	i = i < j ? i : j;
	j = ROUND_ALIGN(n, 16);
	i = i < j ? i : j;

	cycles = i;
	asm (
		"xor	%3, %3\n\t"
		"cmp	$16, %2\n\t"
		"jb	3f\n\t"
		"pxor	%%xmm0, %%xmm0\n\t"
		"movdqa	%5, %%xmm3\n\t"
		"movdqa	%6, %%xmm4\n\t"
		"movdqa	%4, %%xmm5\n\t"
		"jmp	2f\n"
		"1:\n\t"
		"add	$16, %0\n\t"
		"add	$16, %1\n"
		"cmp	$16, %2\n\t"
		"jb	3f\n\t"
		"2:\n\t"
		"sub	$16, %2\n\t"
		/* s1 */
		"movdqu	(%0), %%xmm7\n\t"
		"movdqa	%%xmm7, %%xmm1\n\t"
		"movdqa	%%xmm7, %%xmm2\n\t"
		"pcmpgtb	%%xmm5, %%xmm1\n\t"
		"psubsb	%%xmm3, %%xmm2\n\t"
		"pcmpgtb	%%xmm0, %%xmm2\n\t"
		"pandn	%%xmm1, %%xmm2\n\t"
		"pand	%%xmm4, %%xmm2\n\t"
		"psubb	%%xmm2, %%xmm7\n\t"
		/* s2 */
		"movdqu	(%1), %%xmm6\n\t"
		"movdqa	%%xmm6, %%xmm1\n\t"
		"movdqa	%%xmm6, %%xmm2\n\t"
		"pcmpgtb	%%xmm5, %%xmm1\n\t"
		"psubsb	%%xmm3, %%xmm2\n\t"
		"pcmpgtb	%%xmm0, %%xmm2\n\t"
		"pandn	%%xmm1, %%xmm2\n\t"
		"pand	%%xmm4, %%xmm2\n\t"
		"psubb	%%xmm2, %%xmm6\n\t"
		/* s1 ^ s2 */
		"movdqa	%%xmm7, %%xmm1\n\t"
		"pxor	%%xmm6, %%xmm1\n\t"
		"pcmpgtb	%%xmm0, %%xmm1\n\t"
		"pcmpeqb	%%xmm0, %%xmm6\n\t"
		"pcmpeqb	%%xmm0, %%xmm7\n\t"
		"por	%%xmm1, %%xmm6\n\t"
		"por	%%xmm6, %%xmm7\n\t"
		"pmovmskb	%%xmm7, %3\n\t"
		"test	%3, %3\n\t"
		"jz	1b\n\t"
		"bsf	%3, %3\n\t"
		"inc	%3\n"
		"3:\n\t"
	: /* %0 */ "=r" (s1),
	  /* %1 */ "=r" (s2),
	  /* %2 */ "=r" (i),
	  /* %3 */ "=r" (m1)
	: /* %4 */ "m" (*sma),
	  /* %5 */ "m" (*smz),
	  /* %6 */ "m" (*ucd),
	  /*  */ "0" (s1),
	  /*  */ "1" (s2),
	  /*  */ "2" (i)
	);
	cycles = ((cycles - i) / 16) * 16;
	if(likely(m1)) {
		if(cycles >= n)
			cycles -= 16;
		n -= cycles;
		m1--;
		m1 = m1 < n - 1 ? m1 : n - 1;
		return s1[m1] - s2[m1];
	}
	if(cycles >= n)
		return 0;
	n -= cycles;

	i = ALIGN_DIFF(s1, 4096);
	i = i ? i : 4096;
	j = ALIGN_DIFF(s2, 4096);
	j = j ? j : i;
	i = i < j ? i : j;
	i = i < n ? i : n;

	for(; i; i--, n--)
	{
		c1 = (unsigned) *s1++;
		c2 = (unsigned) *s2++;
		n--;
		c1 -= tab[c1];
		c2 -= tab[c2];
		if(!(c1 && c2 && c1 == c2))
			return (int)c1 - (int)c2;
	}

	if(n)
		goto LOOP_AGAIN;

	return 0;
}

#ifndef __x86_64__
static int strncasecmp_a_SSE(const char *s1, const char *s2, size_t n)
{
	size_t m1;
	size_t i, j, cycles;
	unsigned c1, c2;

	prefetch(ucd);
	prefetch(s1);
	prefetch(s2);

	if(unlikely(!n))
		return 0;

LOOP_AGAIN:
	i = ALIGN_DIFF(s1, 4096);
	i = i ? i : 4096;
	j = ALIGN_DIFF(s2, 4096);
	j = j ? j : i;
	i = i < j ? i : j;
	j = ROUND_ALIGN(n, 8);
	i = i < j ? i : j;

	cycles = i;
	asm (
		"xor	%3, %3\n\t"
		"cmp	$8, %2\n\t"
		"jb	3f\n\t"
		"pxor	%%mm0, %%mm0\n\t"
		"movq	%5, %%mm3\n\t"
		"movq	%6, %%mm4\n\t"
		"movq	%4, %%mm5\n\t"
		"jmp	2f\n"
		"1:\n\t"
		"add	$8, %0\n\t"
		"add	$8, %1\n"
		"cmp	$8, %2\n\t"
		"jbe	3f\n\t"
		"2:\n\t"
		"sub	$8, %2\n\t"
		/* s1 */
		"movq	(%0), %%mm7\n\t"
		"movq	%%mm7, %%mm1\n\t"
		"movq	%%mm7, %%mm2\n\t"
		"pcmpgtb	%%mm5, %%mm1\n\t"
		"psubsb	%%mm3, %%mm2\n\t"
		"pcmpgtb	%%mm0, %%mm2\n\t"
		"pandn	%%mm1, %%mm2\n\t"
		"pand	%%mm4, %%mm2\n\t"
		"psubb	%%mm2, %%mm7\n\t"
		/* s2 */
		"movq	(%1), %%mm6\n\t"
		"movq	%%mm6, %%mm1\n\t"
		"movq	%%mm6, %%mm2\n\t"
		"pcmpgtb	%%mm5, %%mm1\n\t"
		"psubsb	%%mm3, %%mm2\n\t"
		"pcmpgtb	%%mm0, %%mm2\n\t"
		"pandn	%%mm1, %%mm2\n\t"
		"pand	%%mm4, %%mm2\n\t"
		"psubb	%%mm2, %%mm6\n\t"
		/* s1 ^ s2 */
		"movq	%%mm7, %%mm1\n\t"
		"pxor	%%mm6, %%mm1\n\t"
		"pcmpgtb	%%mm0, %%mm1\n\t"
		"pcmpeqb	%%mm0, %%mm6\n\t"
		"pcmpeqb	%%mm0, %%mm7\n\t"
		"por	%%mm1, %%mm6\n\t"
		"por	%%mm6, %%mm7\n\t"
		"pmovmskb	%%mm7, %3\n\t"
		"test	%3, %3\n\t"
		"jz	1b\n\t"
		"bsf	%3, %3\n\t"
		"inc	%3\n"
		"3:\n\t"
	: /* %0 */ "=r" (s1),
	  /* %1 */ "=r" (s2),
	  /* %2 */ "=r" (i),
	  /* %3 */ "=r" (m1)
	: /* %4 */ "m" (*sma),
	  /* %5 */ "m" (*smz),
	  /* %6 */ "m" (*ucd),
	  /*  */ "0" (s1),
	  /*  */ "1" (s2),
	  /*  */ "2" (i)
	);
	cycles = ((cycles - i) / 8) * 8;
	if(likely(m1)) {
		if(cycles >= n)
			cycles -= 8;
		n -= cycles;
		m1--;
		m1 = m1 < n - 1 ? m1 : n - 1;
		return s1[m1] - s2[m1];
	}
	if(cycles >= n)
		return 0;
	n -= cycles;

	i = ALIGN_DIFF(s1, 4096);
	i = i ? i : 4096;
	j = ALIGN_DIFF(s2, 4096);
	j = j ? j : i;
	i = i < j ? i : j;
	i = i < n ? i : n;

	for(; i; i--, n--)
	{
		c1 = (unsigned) *s1++;
		c2 = (unsigned) *s2++;
		n--;
		c1 -= tab[c1];
		c2 -= tab[c2];
		if(!(c1 && c2 && c1 == c2))
			return (int)c1 - (int)c2;
	}

	if(n)
		goto LOOP_AGAIN;

	return 0;
}
#endif

static int strncasecmp_a_x86(const char *s1, const char *s2, size_t n)
{
	size_t w1, w2;
	size_t m1, m2;
	size_t i, j, cycles;
	unsigned c1, c2;

	if(unlikely(!n))
		return 0;

	prefetch(s1);
	prefetch(s2);

LOOP_AGAIN:
	i = ALIGN_DIFF(s1, 4096);
	i = i ? i : 4096;
	j = ALIGN_DIFF(s2, 4096);
	j = j ? j : i;
	i = i < j ? i : j;
	j = ROUND_ALIGN(n, SOST);
	i = i < j ? i : j;

	for(cycles = i; likely(SOSTM1 < i); i -= SOST)
	{
		w1   = *(const size_t *)s1;
		w2   = *(const size_t *)s2;
		s1  += SOST;
		s2  += SOST;
		m1   = has_between(w1, 0x60, 0x7B);
	/*	m1  &= ~(c1 & MK_C(0x80808080)); */
		m1 >>= 2;
		w1  -= m1;
		m2   = has_between(w2, 0x60, 0x7B);
	/*	m2  &= ~(c2 & MK_C(0x80808080)); */
		m2 >>= 2;
		w2  -= m2;
		m1   = w1 ^ w2;
		m2   = has_nul_byte(w1) | has_nul_byte(w2);
		if(m1 || m2) {
			unsigned r1, r2;
			m1 = has_greater(m1, 0);
			r1 = nul_byte_index(m1);
			r2 = nul_byte_index(m2);
			r1 = r1 < r2 ? r1 : r2;
			cycles = (((cycles - i)) / SOST) * SOST;
			n -= cycles;
			r1 = r1 < n - 1 ? r1 : n - 1;
			r1 = r1 * BITS_PER_CHAR;
			return (int)((w1 >> r1) & 0xFF) - (int)((w2 >> r1) & 0xFF);
		}
	}
	cycles = ((cycles - i) / SOST) * SOST;
	if(cycles >= n)
		return 0;
	n -= cycles;

	i = ALIGN_DIFF(s1, 4096);
	i = i ? i : 4096;
	j = ALIGN_DIFF(s2, 4096);
	j = j ? j : i;
	i = i < j ? i : j;
	i = i < n ? i : n;

	for(; i; i--, n--)
	{
		c1 = (unsigned) *s1++;
		c2 = (unsigned) *s2++;
		n--;
		c1 -= tab[c1];
		c2 -= tab[c2];
		if(!(c1 && c2 && c1 == c2))
			return (int)c1 - (int)c2;
	}

	if(n)
		goto LOOP_AGAIN;

	return 0;
}


static const struct test_cpu_feature t_feat[] =
{
	{.func = (void (*)(void))strncasecmp_a_SSE42, .flags_needed = CFEATURE_SSE4_2, .callback = NULL},
	{.func = (void (*)(void))strncasecmp_a_SSE2, .flags_needed = CFEATURE_SSE2, .callback = NULL},
#ifndef __x86_64__
	{.func = (void (*)(void))strncasecmp_a_SSE, .flags_needed = CFEATURE_SSE, .callback = NULL},
#endif
	{.func = (void (*)(void))strncasecmp_a_x86, .flags_needed = -1, .callback = NULL},
};

static int strncasecmp_a_runtime_sw(const char *s1, const char *s2, size_t n);
/*
 * Func ptr
 */
static int (*strncasecmp_a_ptr)(const char *s1, const char *s2, size_t n) = strncasecmp_a_runtime_sw;

/*
 * constructor
 */
static GCC_ATTR_CONSTRUCT void strncasecmp_a_select(void)
{
	strncasecmp_a_ptr = test_cpu_feature(t_feat, anum(t_feat));
}

/*
 * runtime switcher
 *
 * this is inherent racy, we only provide it if the constructer failes
 */
static int strncasecmp_a_runtime_sw(const char *s1, const char *s2, size_t n)
{
	strncasecmp_a_select();
	return strncasecmp_a_ptr(s1, s2, n);
}

int strncasecmp_a(const char *s1, const char *s2, size_t n)
{
	return strncasecmp_a_ptr(s1, s2, n);
}

static char const rcsid_sncca[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
