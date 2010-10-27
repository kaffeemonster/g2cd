/*
 * strreverse_l.c
 * strreverse_l, ppc implementation
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

#if defined(__ALTIVEC__)
# include <altivec.h>
# include "ppc_altivec.h"
# define ARCH_NAME_SUFFIX _ppc
#endif

#include "../generic/strreverse_l.c"

#if defined(__ALTIVEC__)
#define N_BEGIN ((unsigned char *)begin)
#define N_END ((unsigned char *)end - (SOVUC - 1))
void strreverse_l(char *begin, char *end)
{
	vector unsigned char v_swap;
	vector unsigned char v_0;

	if((end - begin) < (ptrdiff_t)((2*SOVUC)-1)) {
		strreverse_l_generic(begin, end);
		return;
	}

	v_swap = vec_ident_rev();
	v_0 = (vector unsigned char)vec_splat_s8(0);
	if(IS_ALIGN(N_BEGIN, SOVUC))
	{
		if(IS_ALIGN(N_END, SOVUC))
		{
			for(; end - ((2*SOVUC)-1) > begin; end -= SOVUC, begin += SOVUC)
			{
				vector unsigned char tb, te;
				tb = vec_ld(0, N_BEGIN);
				te = vec_ld(0, N_END);
				tb = vec_perm(tb, tb, v_swap);
				te = vec_perm(te, te, v_swap);
				vec_st(te, 0, N_BEGIN);
				vec_st(tb, 0, N_END);
			}
		}
		else
		{
			vector unsigned char low, high, te_x, w_mask;
			vector unsigned char w_perm_r, w_perm_l, r_perm;

			r_perm = w_perm_r = vec_lvsr(SOVUC, N_END);
			w_perm_l = vec_lvsl(SOVUC, N_END);
			w_mask = vec_perm(v_0, (vector unsigned char)vec_splat_s8(-1), w_perm_l);

			te_x = high = vec_ld(SOVUC, N_END);
			for(; end - ((2*SOVUC)-1) > begin; end -= SOVUC, begin += SOVUC)
			{
				vector unsigned char tb, te, te_y;
				tb   = vec_ld(0, N_BEGIN);
				te_y = vec_ld(0, N_END);
				te   = vec_perm(te_y, te_x, r_perm);
				tb   = vec_perm(tb, tb, v_swap);
				te   = vec_perm(te, te, v_swap);
				vec_st(te, 0, N_BEGIN);

				low  = vec_perm(tb, tb, w_perm_l);
				high = vec_sel(low, high, w_mask);
				vec_st(high, SOVUC, N_END);
				te_x = te_y;
				high = low;
			}
			low = vec_ld(0, N_END);
			w_mask = vec_perm((vector unsigned char)vec_splat_s8(-1), v_0, w_perm_r);
			high = vec_perm(high, v_0, w_perm_r);
			high = vec_sel(low, high, w_mask);
			vec_st(high, 0, N_END);
		}
	}
	else
	{
		vector unsigned char b_low, b_high, tb_x, b_w_mask;
		vector unsigned char b_w_perm_r, b_w_perm_l, b_r_perm;

		b_w_perm_r = vec_lvsr(0, N_BEGIN);
		b_r_perm = b_w_perm_l = vec_lvsl(0, N_BEGIN);
		b_w_mask = vec_perm(v_0, (vector unsigned char)vec_splat_s8(-1), b_w_perm_r);
		tb_x = b_low = vec_ld(0, N_BEGIN);
		if(IS_ALIGN(N_END, SOVUC))
		{
			for(; end - ((2*SOVUC)-1) > begin; end -= SOVUC, begin += SOVUC)
			{
				vector unsigned char tb, te;
				tb   = tb_x;
				tb_x = vec_ld(SOVUC, N_BEGIN);
				te   = vec_ld(0, N_END);
				tb   = vec_perm(tb, tb_x, b_r_perm);
				tb   = vec_perm(tb, tb, v_swap);
				te   = vec_perm(te, te, v_swap);
				vec_st(tb, 0, N_END);

				b_high = vec_perm(te, te, b_w_perm_r);
				b_low  = vec_sel(b_low, b_high, b_w_mask);
				vec_st(b_low, 0, N_BEGIN);
				b_low = b_high;
			}
		}
		else
		{
			vector unsigned char e_low, e_high, te_x, e_w_mask;
			vector unsigned char e_w_perm_r, e_w_perm_l, e_r_perm;

			e_r_perm = e_w_perm_r = vec_lvsr(SOVUC, N_END);
			e_w_perm_l = vec_lvsl(SOVUC, N_END);
			e_w_mask = vec_perm(v_0, (vector unsigned char)vec_splat_s8(-1), e_w_perm_l);

			te_x = e_high = vec_ld(SOVUC, N_END);
			for(; end - ((2*SOVUC)-1) > begin; end -= SOVUC, begin += SOVUC)
			{

				vector unsigned char tb, te, te_y;
				tb   = tb_x;
				tb_x = vec_ld(SOVUC, N_BEGIN);
				te_y = vec_ld(0, N_END);
				tb   = vec_perm(tb, tb_x, b_r_perm);
				te   = vec_perm(te_y, te_x, e_r_perm);
				tb   = vec_perm(tb, tb, v_swap);
				te   = vec_perm(te, te, v_swap);

				e_low  = vec_perm(tb, tb, e_w_perm_l);
				e_high = vec_sel(e_low, e_high, e_w_mask);
				vec_st(e_high, SOVUC, N_END);

				b_high = vec_perm(te, te, b_w_perm_r);
				b_low  = vec_sel(b_low, b_high, b_w_mask);
				vec_st(b_low, 0, N_BEGIN);
				b_low = b_high;
				te_x = te_y;
				e_high = e_low;
			}
			e_low = vec_ld(0, N_END);
			e_w_mask = vec_perm((vector unsigned char)vec_splat_s8(-1), v_0, e_w_perm_r);
			e_high = vec_perm(e_high, v_0, e_w_perm_r);
			e_high = vec_sel(e_low, e_high, e_w_mask);
			vec_st(e_high, 0, N_END);
		}
		b_high = vec_ld(0, N_BEGIN);
		b_w_mask = vec_perm((vector unsigned char)vec_splat_s8(-1), v_0, b_w_perm_l);

		b_low = vec_perm(b_low, v_0, b_w_perm_l);
		b_low = vec_sel(b_low, b_high, b_w_mask);
		vec_st(b_low, 0, N_BEGIN);
	}

	strreverse_l_generic(begin, end);
}

static char const rcsid_srlp[] GCC_ATTR_USED_VAR = "$Id: $";
#endif
