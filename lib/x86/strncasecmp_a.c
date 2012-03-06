/*
 * strncasecmp_a.c
 * strncasecmp ascii only, x86 implementation
 *
 * Copyright (c) 2008-2012 Jan Seiffert
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

#include "x86_features.h"

/*
 * My GCC generates a nice instruction sequence for
 *   c1 -= c1 >= 'a' && c1 <= 'z' ? 0x20 : 0;
 *
 *   lea    -0x61(%eax),%edx
 *   cmp    $0x1a,%edx
 *   sbb    %ecx,%ecx
 *   and    $0x20,%ecx
 *   sub    %ecx,%eax
 *
 * so away with the lookuptable, it costs cache and
 * the access latency, even cache hot, is "bad".
 * If your GCC does not manage to do the same, update
 * it.
 */
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

#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 219
/*
 * This code does not use any AVX feature, it only uses the new
 * v* opcodes, so the upper half of the register gets 0-ed,
 * and the CPU is not caught with lower/upper half merges
 */
static int strncasecmp_a_AVX(const char *s1, const char *s2, size_t n)
{
	size_t m1;
	size_t t, i, j, cycles;
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
		"mov	$16, %3\n\t"
		"cmp	%3, %2\n\t"
		"jb	3f\n\t"
		"mov	$0x7A61, %k4\n\t"
		"vmovd	%k4, %%xmm1\n\t"
		"vmovdqa	%5, %%xmm2\n\t"
		"mov	$2, %k4\n\t"
		"jmp	2f\n"
		"1:\n\t"
		"add	%3, %0\n\t"
		"add	%3, %1\n"
		"cmp	%3, %2\n\t"
		"jb	3f\n\t"
		"2:\n\t"
		"sub	%3, %2\n\t"
		"vmovdqu	(%0), %%xmm7\n\t"
		"vmovdqu	(%1), %%xmm6\n\t"
		/* s1 */
		/* ByteM,Masked+,Range,Bytes */
		/*             6543210 */
		"vpcmpestrm	$0b1100100, %%xmm7, %%xmm1\n\t"
		"vpand	%%xmm2, %%xmm0, %%xmm0\n\t"
		"vpsubb	%%xmm0, %%xmm7, %%xmm7\n\t"
		/* s2 */
		/* ByteM,Masked+,Range,Bytes */
		/*             6543210 */
		"vpcmpestrm	$0b1100100, %%xmm6, %%xmm1\n\t"
		"vpand	%%xmm2, %%xmm0, %%xmm0\n\t"
		"vpsubb	%%xmm0, %%xmm6, %%xmm6\n\t"
		/* s1 ^ s2 */
		/* LSB,Masked-,EqualE,Bytes */
		/*             6543210 */
		"vpcmpistri	$0b0111000, %%xmm7, %%xmm6\n\t"
		"js	3f\n\t"
		"jz	3f\n\t"
		"jnc	1b\n"
		"3:\n\t"
	: /* %0 */ "=r" (s1),
	  /* %1 */ "=r" (s2),
	  /* %2 */ "=d" (i),
	  /* %3 */ "=c" (m1),
	  /* %4 */ "=a" (t)
	: /* %5 */ "m" (*ucd),
	  /*  */ "0" (s1),
	  /*  */ "1" (s2),
	  /*  */ "2" (i)
#  ifdef __SSE__
	: "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#  endif
	);
	cycles = ROUND_TO(cycles - i, 16);
	if(likely(m1 < 16))
	{
		if(cycles >= n)
			cycles -= 16;
		n  -= cycles;
		m1  = m1 < n - 1 ? m1 : n - 1;
		c1  = *(const unsigned char *)(s1 + m1);
		c2  = *(const unsigned char *)(s2 + m1);
		c1 -= c1 >= 'a' && c1 <= 'z' ? 0x20 : 0;
		c2 -= c2 >= 'a' && c2 <= 'z' ? 0x20 : 0;
		return (int)c1 - (int)c2;
	}
	if(cycles >= n)
		return 0;
	n -= cycles;

	i  = ALIGN_DIFF(s1, 4096);
	i  = i ? i : 4096;
	j  = ALIGN_DIFF(s2, 4096);
	j  = j ? j : i;
	i  = i < j ? i : j;
	i  = i < n ? i : n;
	n -= i;

	for(; i; i--, s1++, s2++)
	{
		c1  = *(const unsigned char *)s1;
		c2  = *(const unsigned char *)s2;
		c1 -= c1 >= 'a' && c1 <= 'z' ? 0x20 : 0;
		c2 -= c2 >= 'a' && c2 <= 'z' ? 0x20 : 0;
		if(!(c1 && c1 == c2))
			return (int)c1 - (int)c2;
	}

	if(n)
		goto LOOP_AGAIN;

	return 0;
}
# endif

# if HAVE_BINUTILS >= 218
static int strncasecmp_a_SSE42(const char *s1, const char *s2, size_t n)
{
	size_t m1;
	size_t t, i, j, cycles;
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
		"mov	$16, %3\n\t"
		"cmp	%3, %2\n\t"
		"jb	3f\n\t"
		"mov	$0x7A61, %k4\n\t"
		"movd	%k4, %%xmm1\n\t"
		"movdqa	%5, %%xmm2\n\t"
		"mov	$2, %k4\n\t"
		"jmp	2f\n"
		"1:\n\t"
		"add	%3, %0\n\t"
		"add	%3, %1\n"
		"cmp	%3, %2\n\t"
		"jb	3f\n\t"
		"2:\n\t"
		"sub	%3, %2\n\t"
		"movdqu	(%0), %%xmm7\n\t"
		"movdqu	(%1), %%xmm6\n\t"
		/* s1 */
		/* ByteM,Masked+,Range,Bytes */
		/*             6543210 */
		"pcmpestrm	$0b1100100, %%xmm7, %%xmm1\n\t"
		"pand	%%xmm2, %%xmm0\n\t"
		"psubb	%%xmm0, %%xmm7\n\t"
		/* s2 */
		/* ByteM,Masked+,Range,Bytes */
		/*             6543210 */
		"pcmpestrm	$0b1100100, %%xmm6, %%xmm1\n\t"
		"pand	%%xmm2, %%xmm0\n\t"
		"psubb	%%xmm0, %%xmm6\n\t"
		/* s1 ^ s2 */
		/* LSB,Masked-,EqualE,Bytes */
		/*             6543210 */
		"pcmpistri	$0b0111000, %%xmm7, %%xmm6\n\t"
		"js	3f\n\t"
		"jz	3f\n\t"
		"jnc	1b\n"
		"3:\n\t"
	: /* %0 */ "=r" (s1),
	  /* %1 */ "=r" (s2),
	  /* %2 */ "=d" (i),
	  /* %3 */ "=c" (m1),
	  /* %4 */ "=a" (t)
	: /* %5 */ "m" (*ucd),
	  /*  */ "0" (s1),
	  /*  */ "1" (s2),
	  /*  */ "2" (i)
#  ifdef __SSE__
	: "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#  endif
	);
	cycles = ROUND_TO(cycles - i, 16);
	if(likely(m1 < 16))
	{
		if(cycles >= n)
			cycles -= 16;
		n  -= cycles;
		m1  = m1 < n - 1 ? m1 : n - 1;
		c1  = *(const unsigned char *)(s1 + m1);
		c2  = *(const unsigned char *)(s2 + m1);
		c1 -= c1 >= 'a' && c1 <= 'z' ? 0x20 : 0;
		c2 -= c2 >= 'a' && c2 <= 'z' ? 0x20 : 0;
		return (int)c1 - (int)c2;
	}
	if(cycles >= n)
		return 0;
	n -= cycles;

	i  = ALIGN_DIFF(s1, 4096);
	i  = i ? i : 4096;
	j  = ALIGN_DIFF(s2, 4096);
	j  = j ? j : i;
	i  = i < j ? i : j;
	i  = i < n ? i : n;
	n -= i;

	for(; i; i--, s1++, s2++)
	{
		c1  = *(const unsigned char *)s1;
		c2  = *(const unsigned char *)s2;
		c1 -= c1 >= 'a' && c1 <= 'z' ? 0x20 : 0;
		c2 -= c2 >= 'a' && c2 <= 'z' ? 0x20 : 0;
		if(!(c1 && c1 == c2))
			return (int)c1 - (int)c2;
	}

	if(n)
		goto LOOP_AGAIN;

	return 0;
}
# endif
#endif

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
		"movdqu	(%0), %%xmm7\n\t"
		"movdqu	(%1), %%xmm6\n\t"
		/* s1 */
		"movdqa	%%xmm7, %%xmm1\n\t"
		"movdqa	%%xmm7, %%xmm2\n\t"
		"pcmpgtb	%%xmm5, %%xmm1\n\t"
		"psubsb	%%xmm3, %%xmm2\n\t"
		"pcmpgtb	%%xmm0, %%xmm2\n\t"
		"pandn	%%xmm1, %%xmm2\n\t"
		"pand	%%xmm4, %%xmm2\n\t"
		"psubb	%%xmm2, %%xmm7\n\t"
		/* s2 */
		"movdqa	%%xmm6, %%xmm1\n\t"
		"movdqa	%%xmm6, %%xmm2\n\t"
		"pcmpgtb	%%xmm5, %%xmm1\n\t"
		"psubsb	%%xmm3, %%xmm2\n\t"
		"pcmpgtb	%%xmm0, %%xmm2\n\t"
		"pandn	%%xmm1, %%xmm2\n\t"
		"pand	%%xmm4, %%xmm2\n\t"
		"psubb	%%xmm2, %%xmm6\n\t"
		/* s1 ^ s2 */
		"pxor	%%xmm7, %%xmm6\n\t"
		"pcmpeqb	%%xmm1, %%xmm1\n\t"
		"pcmpeqb	%%xmm0, %%xmm6\n\t"
		"pcmpeqb	%%xmm0, %%xmm7\n\t"
		"pxor	%%xmm1, %%xmm6\n\t"
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
#ifdef __SSE__
	: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
	);
	cycles = ROUND_TO(cycles - i, 16);
	if(likely(m1))
	{
		if(cycles >= n)
			cycles -= 16;
		n  -= cycles;
		m1--;
		m1  = m1 < n - 1 ? m1 : n - 1;
		c1  = *(const unsigned char *)(s1 + m1);
		c2  = *(const unsigned char *)(s2 + m1);
		c1 -= c1 >= 'a' && c1 <= 'z' ? 0x20 : 0;
		c2 -= c2 >= 'a' && c2 <= 'z' ? 0x20 : 0;
		return (int)c1 - (int)c2;
	}
	if(cycles >= n)
		return 0;
	n -= cycles;

	i  = ALIGN_DIFF(s1, 4096);
	i  = i ? i : 4096;
	j  = ALIGN_DIFF(s2, 4096);
	j  = j ? j : i;
	i  = i < j ? i : j;
	i  = i < n ? i : n;
	n -= i;

	for(; i; i--, s1++, s2++)
	{
		c1  = *(const unsigned char *)s1;
		c2  = *(const unsigned char *)s2;
		c1 -= c1 >= 'a' && c1 <= 'z' ? 0x20 : 0;
		c2 -= c2 >= 'a' && c2 <= 'z' ? 0x20 : 0;
		if(!(c1 && c1 == c2))
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
		"jb	3f\n\t"
		"2:\n\t"
		"sub	$8, %2\n\t"
		"movq	(%0), %%mm7\n\t"
		"movq	(%1), %%mm6\n\t"
		/* s1 */
		"movq	%%mm7, %%mm1\n\t"
		"movq	%%mm7, %%mm2\n\t"
		"pcmpgtb	%%mm5, %%mm1\n\t"
		"psubsb	%%mm3, %%mm2\n\t"
		"pcmpgtb	%%mm0, %%mm2\n\t"
		"pandn	%%mm1, %%mm2\n\t"
		"pand	%%mm4, %%mm2\n\t"
		"psubb	%%mm2, %%mm7\n\t"
		/* s2 */
		"movq	%%mm6, %%mm1\n\t"
		"movq	%%mm6, %%mm2\n\t"
		"pcmpgtb	%%mm5, %%mm1\n\t"
		"psubsb	%%mm3, %%mm2\n\t"
		"pcmpgtb	%%mm0, %%mm2\n\t"
		"pandn	%%mm1, %%mm2\n\t"
		"pand	%%mm4, %%mm2\n\t"
		"psubb	%%mm2, %%mm6\n\t"
		/* s1 ^ s2 */
		"pxor	%%mm7, %%mm6\n\t"
		"pcmpeqb	%%mm1, %%mm1\n\t"
		"pcmpeqb	%%mm0, %%mm6\n\t"
		"pcmpeqb	%%mm0, %%mm7\n\t"
		"pxor	%%mm1, %%mm6\n\t"
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
# ifdef __MMX__
	: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
# endif
	);
	cycles = ROUND_TO(cycles - i, 8);
	if(likely(m1))
	{
		if(cycles >= n)
			cycles -= 8;
		n  -= cycles;
		m1--;
		m1  = m1 < n - 1 ? m1 : n - 1;
		c1  = *(const unsigned char *)(s1 + m1);
		c2  = *(const unsigned char *)(s2 + m1);
		c1 -= c1 >= 'a' && c1 <= 'z' ? 0x20 : 0;
		c2 -= c2 >= 'a' && c2 <= 'z' ? 0x20 : 0;
		return (int)c1 - (int)c2;
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
	n -= i;

	for(; i; i--, s1++, s2++)
	{
		c1  = *(const unsigned char *)s1;
		c2  = *(const unsigned char *)s2;
		c1 -= c1 >= 'a' && c1 <= 'z' ? 0x20 : 0;
		c2 -= c2 >= 'a' && c2 <= 'z' ? 0x20 : 0;
		if(!(c1 && c1 == c2))
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
		m2   = has_nul_byte(w1);
		if(m1 || m2) {
			unsigned r1, r2;
			m1 = has_greater(m1, 0);
			r1 = nul_byte_index(m1);
			r2 = nul_byte_index(m2);
			r1 = r1 < r2 ? r1 : r2;
			cycles = ROUND_TO(cycles - i, SOST);
			n -= cycles;
			r1 = r1 < n - 1 ? r1 : n - 1;
			r1 = r1 * BITS_PER_CHAR;
			return (int)((w1 >> r1) & 0xFF) - (int)((w2 >> r1) & 0xFF);
		}
	}
	cycles = ROUND_TO(cycles - i, SOST);
	if(cycles >= n)
		return 0;
	n -= cycles;

	i  = ALIGN_DIFF(s1, 4096);
	i  = i ? i : 4096;
	j  = ALIGN_DIFF(s2, 4096);
	j  = j ? j : i;
	i  = i < j ? i : j;
	i  = i < n ? i : n;
	n -= i;

	for(; i; i--, s1++, s2++)
	{
		c1  = *(const unsigned char *)s1;
		c2  = *(const unsigned char *)s2;
		c1 -= c1 >= 'a' && c1 <= 'z' ? 0x20 : 0;
		c2 -= c2 >= 'a' && c2 <= 'z' ? 0x20 : 0;
		if(!(c1 && c1 == c2))
			return (int)c1 - (int)c2;
	}

	if(n)
		goto LOOP_AGAIN;

	return 0;
}


static __init_cdata const struct test_cpu_feature tfeat_strncasecmp_a[] =
{
#ifdef HAVE_BINUTILS
# if HAVE_BINUTILS >= 219
	{.func = (void (*)(void))strncasecmp_a_AVX,   .features = {[1] = CFB(CFEATURE_AVX)}, .flags = CFF_AVX_TST},
# endif
# if HAVE_BINUTILS >= 218
	{.func = (void (*)(void))strncasecmp_a_SSE42, .features = {[1] = CFB(CFEATURE_SSE4_2)}},
# endif
#endif
	{.func = (void (*)(void))strncasecmp_a_SSE2,  .features = {[0] = CFB(CFEATURE_SSE2)}},
#ifndef __x86_64__
	{.func = (void (*)(void))strncasecmp_a_SSE,   .features = {[0] = CFB(CFEATURE_SSE)}},
	{.func = (void (*)(void))strncasecmp_a_SSE,   .features = {[2] = CFB(CFEATURE_MMXEXT)}},
#endif
	{.func = (void (*)(void))strncasecmp_a_x86,   .features = {}, .flags = CFF_DEFAULT},
};

DYN_JMP_DISPATCH(int, strncasecmp_a, (const char *s1, const char *s2, size_t n), (s1, s2, n))

/*@unused@*/
static char const rcsid_sncca[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
