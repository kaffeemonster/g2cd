/*
 * to_base16.c
 * convert binary string to hex, ppc impl.
 *
 * Copyright (c) 2010 Jan Seiffert
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
# define ONLY_REMAINDER
# define ARCH_NAME_SUFFIX _generic
#endif

#include "../generic/to_base16.c"

#if defined(__ALTIVEC__) && defined(__GNUC__)

tchar_t *to_base16(tchar_t *dst, const unsigned char *src, unsigned len)
{
	vector unsigned char v_f;
	vector unsigned char v_0;
	vector unsigned char v_4;
	vector unsigned char lut;
	vector unsigned char in_l, in_h, in, t1, t2;
	vector unsigned short o1, o2, o3, o4;

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
				o1   = (vector unsigned short)vec_mergeh(v_0, t1);
				o2   = (vector unsigned short)vec_mergel(v_0, t1);
				o3   = (vector unsigned short)vec_mergeh(v_0, t2);
				o4   = (vector unsigned short)vec_mergel(v_0, t2);
				vec_st(o1, 0 * SOVUC, dst);
				vec_st(o2, 1 * SOVUC, dst);
				vec_st(o3, 2 * SOVUC, dst);
				vec_st(o4, 3 * SOVUC, dst);
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
				o1   = (vector unsigned short)vec_mergeh(v_0, t1);
				o2   = (vector unsigned short)vec_mergel(v_0, t1);
				o3   = (vector unsigned short)vec_mergeh(v_0, t2);
				o4   = (vector unsigned short)vec_mergel(v_0, t2);
				vec_st(o1, 0 * SOVUC, dst);
				vec_st(o2, 1 * SOVUC, dst);
				vec_st(o3, 2 * SOVUC, dst);
				vec_st(o4, 3 * SOVUC, dst);
			}
		}
	}
	else
	{
		unsigned d_diff;
		vector unsigned short low, high, w_mask;
		vector unsigned char w_perm_r, w_perm_l;

		d_diff = ALIGN_DOWN_DIFF(dst, SOVUC);
		w_perm_r = vec_lvsr(0, dst);
		w_perm_l = vec_lvsl(0, dst);
		w_mask = (vector unsigned short)vec_perm(v_0, (vector unsigned char)vec_splat_s8(-1), w_perm_r);
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
				o1   = (vector unsigned short)vec_mergeh(v_0, t1);
				o2   = (vector unsigned short)vec_mergel(v_0, t1);
				o3   = (vector unsigned short)vec_mergeh(v_0, t2);
				o4   = (vector unsigned short)vec_mergel(v_0, t2);
				high = vec_perm(o1, o1, w_perm_r);
				low  = vec_sel(low, high, w_mask);
				o1   = vec_perm(o1, o2, w_perm_l);
				o2   = vec_perm(o2, o3, w_perm_l);
				o3   = vec_perm(o3, o4, w_perm_l);
				vec_st(low, 0 * SOVUC, dst);
				vec_st( o1, 1 * SOVUC, dst);
				vec_st( o2, 2 * SOVUC, dst);
				vec_st( o3, 3 * SOVUC, dst);
				low = o4;
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
				o1   = (vector unsigned short)vec_mergeh(v_0, t1);
				o2   = (vector unsigned short)vec_mergel(v_0, t1);
				o3   = (vector unsigned short)vec_mergeh(v_0, t2);
				o4   = (vector unsigned short)vec_mergel(v_0, t2);
				high = vec_perm(o1, o1, w_perm_r);
				low  = vec_sel(low, high, w_mask);
				o1   = vec_perm(o1, o2, w_perm_l);
				o2   = vec_perm(o2, o3, w_perm_l);
				o3   = vec_perm(o3, o4, w_perm_l);
				vec_st(low, 0 * SOVUC, dst);
				vec_st( o1, 1 * SOVUC, dst);
				vec_st( o2, 2 * SOVUC, dst);
				vec_st( o3, 3 * SOVUC, dst);
				low = o4;
			}
			high = vec_ld(0, dst);
			w_mask = (vector unsigned short)vec_perm((vector unsigned char)vec_splat_s8(-1), v_0, w_perm_l);
			low  = vec_perm(low, (vector unsigned short)v_0, w_perm_l);
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
