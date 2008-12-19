/*
 * mem_searchrn.c
 * search mem for a \r\n, ppc implementation
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

void *mem_searchrn(void *src, size_t len)
{
	static const vector unsigned char y =
		{'\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r',
		 '\r', '\r', '\r', '\r', '\r', '\r', '\r', '\r'};
	static const vector unsigned char z =
		{'\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n',
		 '\n', '\n', '\n', '\n', '\n', '\n', '\n', '\n'};
	char *src_c = src;

	prefetch(src_c);
	if(unlikely(!src_c || !len))
		return NULL;

	/*
	 * we work our way unaligned forward, this func
	 * is not meant for too long mem regions
	 */
	if(len >= (SOVUC * 2))
	{
		vector unsigned char fix_alignment;
		vector unsigned char v0;
		vector unsigned char c, c_ex;
		vector bool char last_rr;

		v0 = vec_splat_u8(0);
		fix_alignment = vec_align_and_rev(src_c);
		c_ex = vec_ld(0, (vector const unsigned char *)src_c);
		last_rr = v0;
		for(; likely(SOVUC < len); src_c += SOVUC, len -= SOVUC)
		{
			vector bool char rr, rn;

			c = c_ex;
			c_ex = vec_ld(1 * SOVUC, (vector const unsigned char *)src_c);
			c = vec_perm(c, c_ex, fix_alignment);

			rr = vec_cmpeq(c, y);
			rn = vec_cmpeq(c, z);
			if(vec_any_eq(last_rr, rn))
				return src_c - 1;
			last_rr = (vector bool char)vec_sld(v0, (vector unsigned char)rr, 1);
			rn = (vector bool char)vec_sld(v0, (vector unsigned char)rn, 15);
			rr = vec_and(rr, rn); /* get mask */
			if(vec_any_ne(rr, v0)) {
				unsigned r;
				r = vec_pbmovmsk(rr);
				return src_c + __builtin_ctz(r);
			}
		}
		if(vec_any_ne(last_rr, v0)) {
			src_c -= 1;
			goto OUT;
		}
	}
	for(; SO32M1 < len; src_c += SO32, len -= SO32)
	{
		uint32_t v = *((uint32_t *)src_c) ^ 0x0D0D0D0D; /* '\r\r\r\r' */
		uint32_t r = has_nul_byte32(v);
		if(r) {
			unsigned i  = 3 - (__builtin_ctz(r) / 8);
			len   -= i;
			src_c += i;
			break;
		}
	}

OUT:
	for(; len; len--, src_c++) {
		if('\r' == *src_c && len-1 && '\n' == src_c[1])
			return src_c;
	}
	return NULL;
}

static char const rcsid_msrn[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/mem_searchrn.c"
#endif
/* EOF */
