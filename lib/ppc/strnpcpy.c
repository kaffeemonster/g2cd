/*
 * strnpcpy.c
 * strnpcpy for efficient concatination, ppc implementation
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

#if defined( __ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <altivec.h>
# include "ppc_altivec.h"

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

char *strnpcpy(char *dst, const char *src, size_t maxlen)
{
	size_t i = 0, cycles;

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
	/* align dst for vector ops */
	for(cycles = ALIGN_DIFF(dst, SO32); cycles && i && *src; cycles--, i--, maxlen--)
		*dst++ = *src++;
	if(*src)
		goto OUT;
	for(cycles = ALIGN_DIFF(dst, SOVUC) / SOVUC;
	    cycles && likely(SO32M1 < i); src += SO32, dst += SO32, maxlen -= SO32 i -= SO32, cycles--)
	{
		uint32_t c = *(const uint32_t *)src;
		r = has_nul_byte32(c);
		if(likely(r))
			return cpy_rest(dst, src, 3 - (__builtin_ctz(r) / 8));
		*(uint32_t *)dst = c;
	}

	if(i > (SOVUC * 2))
	{
		vector unsigned char fix_alignment;
		vector unsigned char v0;
		vector unsigned char c, c_ex;

		v0 = vec_splat_u8(0);
		fix_alignment = vec_align(src);
		c_ex = vec_ld(0, (vector const unsigned char *)src);
		for(cycles = i; likely(SOVUC < i); i -= SOVUC, src += SOVUC, dst += SOVUC) {
			c = c_ex;
			c_ex = vec_ld(1 * SOVUC, (vector unsigned char *)src);
			c = vec_perm(c, c_ex, fix_alignment);
			if(vec_any_eq(c, v0)) /* zero byte? */
			{
				vector bool char vr;
				unsigned r;

				vr = vec_cmpeq(vec_perm(c, v0, vec_ident_rev()), v0); /* get mask */
				r = vec_pbmovmask(vr); /* transfer */
				return cpy_rest(dst, src, __builtin_ctz(r)); /* get index */
			}
			vec_st(c, 0, (vector unsigned char *)dst);
		}
		maxlen -= (cycles - i);
	}
	/* approch page boundery */
	for(; likely(SO32M1 < i); src += SO32, dst += SO32, maxlen -= SO32 i -= SO32)
	{
		uint32_t c = *(const uint32_t *)src;
		r = has_nul_byte32(c);
		if(likely(r))
			return cpy_rest(dst, src, 3 - (__builtin_ctz(r) / 8));
		*(uint32_t *)dst = c;
	}
	/* slowly go over the page boundry */
	for(; i && *src; i--, maxlen--)
		*dst++ = *src++;

	/* now src is aligned, life is good... */
	if(likely(*src) && likely(maxlen)) {
		i = 4096 < maxlen ? 4096 : maxlen;
		/* since only src is aligned, simply continue with unaligned copy */
		goto CPY_NEXT;
	}
OUT:
	if(likely(maxlen))
		*dst = '\0';
	return dst;
}

static char const rcsid_snpcg[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/strnlen.c"
#endif
/* EOF */
