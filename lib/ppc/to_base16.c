/*
 * to_base16.c
 * convert binary string to hex, ppc impl.
 *
 * Copyright (c) 2010 Jan Seiffert
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

#if defined(__ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <altivec.h>
# include "ppc_altivec.h"
# define ONLY_REMAINDER
# define ARCH_NAME_SUFFIX _generic
#endif

#include "../generic/to_base16.c"

#if defined(__ALTIVEC__) && defined(__GNUC__)

unsigned char *to_base16(unsigned char *dst, const unsigned char *src, unsigned len)
{
	vector unsigned char v_f;
	vector unsigned char v_0;
	vector unsigned char v_4;
	vector unsigned char lut;
	vector unsigned char in_l, in_h, in, t1, t2;

	if(unlikely(len < SOVUC))
		return to_base16_generic(dst, src, len);

	v_0 = vec_splat_u8(0);
	v_4 = vec_splat_u8(4);
	v_f = vec_splat_u8(0x0f);
	lut = (vector unsigned char){'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

	if(IS_ALIGN(dst, SOVUC))
	{
		if(IS_ALIGN(src, SOVUC))
		{
			for(; len >= SOVUC; len -= SOVUC, src += SOVUC, dst += 2 * SOVUC)
			{
				in   = vec_ld(0, src);
				in_l = vec_perm(lut, lut, vec_and(in, v_f));
				in_h = vec_perm(lut, lut, vec_sr(in, v_4));
				t1   = vec_mergeh(in_h, in_l);
				t2   = vec_mergel(in_h, in_l);
				vec_st(t1, 0 * SOVUC, dst);
				vec_st(t2, 1 * SOVUC, dst);
			}
		}
		else
		{
			vector unsigned char r_perm, in_x;
			r_perm = vec_lvsl(0, src);
			in_x = vec_ld(0, src);
			for(; len >= SOVUC; len -= SOVUC, src += SOVUC, dst += 2 * SOVUC)
			{
				in   = in_x;
				in_x = vec_ld(SOVUC, src);
				in   = vec_perm(in, in_x, r_perm);
				in_l = vec_perm(lut, lut, vec_and(in, v_f));
				in_h = vec_perm(lut, lut, vec_sr(in, v_4));
				t1   = vec_mergeh(in_h, in_l);
				t2   = vec_mergel(in_h, in_l);
				vec_st(t1, 0 * SOVUC, dst);
				vec_st(t2, 1 * SOVUC, dst);
			}
		}
	}
	else
	{
		vector unsigned char low, high, w_mask;
		vector unsigned char w_perm_r, w_perm_l;

		w_perm_r = vec_lvsr(0, dst);
		w_perm_l = vec_lvsl(0, dst);
		w_mask = vec_perm(v_0, (vector unsigned char)vec_splat_s8(-1), w_perm_r);
		low = vec_ld(0, dst);
		if(IS_ALIGN(src, SOVUC))
		{
			for(; len >= SOVUC; len -= SOVUC, src += SOVUC, dst += 2 * SOVUC)
			{
				in   = vec_ld(0, src);
				in_l = vec_perm(lut, lut, vec_and(in, v_f));
				in_h = vec_perm(lut, lut, vec_sr(in, v_4));
				t1   = vec_mergeh(in_h, in_l);
				t2   = vec_mergel(in_h, in_l);
				high = vec_perm(t1, t1, w_perm_r);
				low  = vec_sel(low, high, w_mask);
				t1   = vec_perm(t1, t2, w_perm_l);
				vec_st(low, 0 * SOVUC, dst);
				vec_st( t1, 1 * SOVUC, dst);
				low = t2;
			}
		}
		else
		{
			vector unsigned char r_perm, in_x;
			r_perm = vec_lvsl(0, src);
			in_x = vec_ld(0, src);
			for(; len >= SOVUC; len -= SOVUC, src += SOVUC, dst += 2 * SOVUC)
			{
				in   = in_x;
				in_x = vec_ld(SOVUC, src);
				in   = vec_perm(in, in_x, r_perm);
				in_l = vec_perm(lut, lut, vec_and(in, v_f));
				in_h = vec_perm(lut, lut, vec_sr(in, v_4));
				t1   = vec_mergeh(in_h, in_l);
				t2   = vec_mergel(in_h, in_l);
				high = vec_perm(t1, t1, w_perm_r);
				low  = vec_sel(low, high, w_mask);
				t1   = vec_perm(t1, t2, w_perm_l);
				vec_st(low, 0 * SOVUC, dst);
				vec_st( t1, 1 * SOVUC, dst);
				low = t2;
			}
			high = vec_ld(0, dst);
			w_mask = vec_perm((vector unsigned char)vec_splat_s8(-1), v_0, w_perm_l);
			low  = vec_perm(low, v_0, w_perm_l);
			low  = vec_sel(low, high, w_mask);
			vec_st(low, 0, dst);
		}
	}

	if(unlikely(len))
		return to_base16_generic(dst, src, len);
	return dst;
}

static char const rcsid_tb16p[] GCC_ATTR_USED_VAR = "$Id:$";
#endif
/* EOF */
