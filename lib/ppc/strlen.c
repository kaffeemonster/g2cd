/*
 * strlen.c
 * strlen, ppc implementation
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

#if defined(__ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <altivec.h>
# include "ppc_altivec.h"

size_t strlen(const char *s)
{

	const char *p = s;
	size_t cycles;
	prefetch(s);

	if(unlikely(!s))
		return 0;

	/* don't step over the page boundery */
	cycles = (const char *)ALIGN(p, 4096) - p;
	/*
	 * we don't precheck for alignment, 16 is very unlikely
	 * instead go unaligned until a page boundry
	 */
	if(cycles >= (SOVUC * 2)) /* make shure we have enough space */
	{
		vector unsigned char fix_alignment;
		vector unsigned char v0;
		vector unsigned char c, c_ex;

		v0 = vec_splat_u8(0);
		fix_alignment = vec_align_and_rev(p);
		c_ex = vec_ld(0, (vector const unsigned char *)p);
		for(; likely(SOVUCM1 < cycles); cycles -= SOVUC, p += SOVUC)
		{
			c = c_ex;
			c_ex = vec_ld(1 * SOVUC, (vector const unsigned char *)p);
			c = vec_perm(c, c_ex, fix_alignment);
			if(vec_any_eq(c, v0)) /* zero byte? */
			{
				vector bool char vr;
				unsigned r;

				vr = vec_cmpeq(c, v0); /* get mask */
				r = vec_pbmovmsk(vr); /* tranfer */
				p += __builtin_ctz(r); /* get index */
				goto OUT;
			}
		}
	}
	for(; likely(SO32M1 < cycles); cycles -= SO32, p += SO32)
	{
		uint32_t r = has_nul_byte32(*(const uint32_t *)p);
		if(r) {
			p += 3 - (__builtin_ctz(r) / 8);
			goto OUT;
		}
	}
	/* slowly encounter page boundery */
	while(cycles && *p)
		p++, cycles--;

	if(likely(*p))
	{
		vector unsigned char v0;

		v0 = vec_splat_u8(0);
		while(1)
		{
			vector unsigned char c = vec_ld(0, (vector const unsigned char *)p);
			if(vec_any_eq(c, v0))
			{
				vector bool char vr;
				unsigned r;

				vr = (vector bool char)vec_perm((vector unsigned char)vec_cmpeq(c, v0), v0, vec_ident_rev());
				r = vec_pbmovmsk(vr);
				p += __builtin_ctz(r);
				break;
			}
			p += SOVUC;
		}
	}
OUT:
	return p - s;
}

static char const rcsid_sl[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/strlen.c"
#endif
/* EOF */
