/*
 * ppc_altivec.h
 * little ppc altivec helper
 *
 * Copyright (c) 2008-2011 Jan Seiffert
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

#ifndef PPC_ALTIVEC_H
# define PPC_ALTIVEC_H

# define SOVUC (sizeof(vector unsigned char))
# define SOVUCM1 (SOVUC - 1)
# define SOVUI (sizeof(vector unsigned int))

/* you have to love GCC, it does not take NULL on vec_lvsl ... */
static inline vector unsigned char vec_identl(unsigned level)
{
	return vec_lvsl(0, (const unsigned char *)(uintptr_t)level);
}

static inline vector unsigned char vec_ident_rev(void)
{
	return vec_xor(vec_lvsl(0, (const unsigned char *)0), vec_splat_u8(15));
}

static inline vector unsigned char vec_align(const char *p)
{
	return vec_lvsl(0, (const unsigned char *)p);
}

static inline vector unsigned char vec_align_and_rev(const char *p)
{
	vector unsigned char ret = vec_align(p);
	ret = vec_perm(ret, vec_splat_u8(0), vec_ident_rev());
	return ret;
}

static inline vector unsigned int vector_load_one_u32(unsigned int x)
{
	vector unsigned int val = vec_lde(0, &x);
	vector unsigned char vperm, mask;

	mask = (vector unsigned char)vec_cmpgt(vec_identl(0), vec_splat_u8(3));
	vperm = (vector unsigned char)vec_splat((vector unsigned int)vec_lvsl(0, &x), 0);
	val = vec_perm(val, val, vperm);
	val = vec_sel(val, vec_splat_u32(0), (vector unsigned int)mask);
	return val;
}

/* multiply two 32 bit ints, return the low 32 bit */
static inline vector unsigned int vec_mullw(vector unsigned int a, vector unsigned int b)
{
	vector unsigned int v16   = vec_splat_u32(-16);
	vector unsigned int v0_32 = vec_splat_u32(0);
	vector unsigned int swap, low, high;

	swap = vec_rl(b, v16);
	low  = vec_mulo((vector unsigned short)a, (vector unsigned short)b);
	high = vec_msum((vector unsigned short)a, (vector unsigned short)swap, v0_32);
	high = vec_sl(high, v16);
	return vec_add(low, high);
}

/* multiply two 32 bit ints, return the high 32 bit */
static inline vector unsigned int vec_mullwh(vector unsigned int a, vector unsigned int b)
{
	vector unsigned int v16   = vec_splat_u32(-16);
//	vector unsigned int v0_32 = vec_splat_u32(0);
	vector unsigned int swap, low, high;

	swap = vec_rl(b, v16);
	low  = vec_mulo((vector unsigned short)a, (vector unsigned short)b);
	low  = vec_sr(low, v16); /* we only care for the "overflow" */
	low  = vec_msum((vector unsigned short)a, (vector unsigned short)swap, low);
	low  = vec_sr(low, v16); /* we only care for the "overflow" */
	high = vec_mule((vector unsigned short)a, (vector unsigned short)b);
	return vec_add(low, high);
}

/*
 * The ppc guys missed one usefull instruction for altivec:
 * Transfering a logic mask from a SIMD register to a general purpose
 * register. We try to emulate this. But i fear it is not very efficient...
 */
static inline uint32_t vec_pmovmskb(vector bool char vr)
{
//	vector unsigned char swap_ident;
	vector unsigned char factor;
	vector unsigned int vri;
	vector int vr_hi, vr_li, v0;
	vector unsigned short vr_h, vr_l;
	uint32_t r;

	v0 = (vector int)vec_splat_u8(0);
	vr = vec_and(vr, (vector bool char)vec_splat_u8(1)); /* single bit */
	factor = vec_ident_rev(); /* get reverse ident */
	vr_h = vec_sl((vector unsigned short)vec_unpackh(vr), (vector unsigned short)vec_unpackh((vector signed char)factor)); /* shift */
	vr_l = vec_sl((vector unsigned short)vec_unpackl(vr), (vector unsigned short)vec_unpackl((vector signed char)factor));
	vr_li = vec_sum4s((vector short) vr_l, v0); /* consolidate words to dwords */
	vr_hi = vec_sum4s((vector short) vr_h, v0);
	vri = (vector unsigned int)vec_sums(vr_li, v0); /* consolidate dwords on low dword */
	vri = (vector unsigned int)vec_sums(vr_hi, (vector int)vri);
	vri = vec_splat(vri, 3); /* splat */
	vec_ste(vri, 0, &r); /* transfer */
	return r;
}

static inline uint32_t vec_zpos(vector bool char vr)
{
#if 0
	/*
	 * Create a bit mask, move to main CPU and count leading zeros
	 *
	 * this creates less instructions, but:
	 * - those are some compl. vector inst
	 * - contains a move over the stack
	 * - may be broken
	 */
	uint32_t r = vec_pmovmskb(vr);
	return __builtin_clz(r) - 16;
#else
	/*
	 * Bisect the guilty byte.
	 * This creates more instructions, and some cond. jump but:
	 * - simple vector ops
	 * - direct info path over compare instructions
	 */
	vector bool char v0 = (vector bool char)vec_splat_s8(0);
	vector bool char vt, vt1, vt21, vt22, vt31, vt32, vt33, vt34;
	vector unsigned char vswz, vswz1, vswz21, vswz22, vswz31, vswz32, vswz33, vswz34;
	unsigned r;

	vswz1 = vec_lvsl(8, (unsigned char *)0);
	vt1 = vec_perm(v0, vr, vswz1);
	vswz21 = vec_lvsl(12, (unsigned char *)0);
	vswz22 = vec_lvsl( 4, (unsigned char *)0);
	vt21 = vec_perm(v0, vr, vswz21);
	vt22 = vec_perm(v0, vr, vswz22);
	vswz31 = vec_lvsl(14, (unsigned char *)0);
	vswz32 = vec_lvsl(10, (unsigned char *)0);
	vswz33 = vec_lvsl( 6, (unsigned char *)0);
	vswz34 = vec_lvsl( 2, (unsigned char *)0);
	vt31 = vec_perm(v0, vr, vswz31);
	vt32 = vec_perm(v0, vr, vswz32);
	vt33 = vec_perm(v0, vr, vswz33);
	vt34 = vec_perm(v0, vr, vswz34);
	if(vec_all_eq(vt1, v0)) {
		if(vec_all_eq(vt21, v0))
			r = vec_all_eq(vt31, v0) ? 15 : 13;
		else
			r = vec_all_eq(vt32, v0) ? 11 :  9;
	} else {
		if(vec_all_eq(vt22, v0))
			r = vec_all_eq(vt33, v0) ?  7 :  4;
		else
			r = vec_all_eq(vt34, v0) ?  3 :  1;
	}
	vswz = vec_lvsl(r, (unsigned char *)0);
	vt = vec_perm(v0, vr, vswz);
	r += vec_all_eq(vt, v0) ? 0 : -1;
	return r;
#endif
}

static inline uint32_t vec_zpos_last(vector bool char vr)
{
#if 0
	/*
	 * Create a bit mask, move to main CPU and count trailing zeros
	 *
	 * this creates less instructions, but:
	 * - those are some compl. vector inst
	 * - contains a move over the stack
	 * - may be broken
	 */
	uint32_t r = vec_pmovmskb(vr);
	return 15 - __builtin_ctz(r);
#else
	/*
	 * Bisect the guilty byte.
	 * This creates more instructions, and some cond. jump but:
	 * - simple vector ops
	 * - direct info path over compare instructions
	 */
	vector bool char v0 = (vector bool char)vec_splat_s8(0);
	vector bool char vt, vt1, vt21, vt22, vt31, vt32, vt33, vt34;
	vector unsigned char vswz, vswz1, vswz21, vswz22, vswz31, vswz32, vswz33, vswz34;
	unsigned r;

	vswz1 = vec_lvsr(8, (unsigned char *)0);
	vt1 = vec_perm(vr, v0, vswz1);
	vswz21 = vec_lvsr( 4, (unsigned char *)0);
	vswz22 = vec_lvsr(12, (unsigned char *)0);
	vt21 = vec_perm(vr, v0, vswz21);
	vt22 = vec_perm(vr, v0, vswz22);
	vswz31 = vec_lvsr( 2, (unsigned char *)0);
	vswz32 = vec_lvsr( 6, (unsigned char *)0);
	vswz33 = vec_lvsr(10, (unsigned char *)0);
	vswz34 = vec_lvsr(14, (unsigned char *)0);
	vt31 = vec_perm(vr, v0, vswz31);
	vt32 = vec_perm(vr, v0, vswz32);
	vt33 = vec_perm(vr, v0, vswz33);
	vt34 = vec_perm(vr, v0, vswz34);
	if(vec_all_eq(vt1, v0)) {
		if(vec_all_eq(vt21, v0))
			r = vec_all_eq(vt31, v0) ?  1 :  3;
		else
			r = vec_all_eq(vt32, v0) ?  4 :  7;
	} else {
		if(vec_all_eq(vt22, v0))
			r = vec_all_eq(vt33, v0) ?  9 : 11;
		else
			r = vec_all_eq(vt34, v0) ? 13 : 15;
	}
	vswz = vec_lvsr(r, (unsigned char *)0);
	vt = vec_perm(vr, v0, vswz);
	r += vec_all_eq(vt, v0) ? -1 : 0;
	return r;
#endif
}

#if 0
	ident_rev =
		{   15,    14,   13,   12,   11,   10,   9,   8,   7,  6,  5,  4, 3, 2, 1, 0};
	msums =
		{  256,   128,  128,   64,   64,   32,  32,  16,  16,  8,  8,  4, 4, 2, 2, 1};
		{  128,   128,   64,   64,   32,   32,  16,  16,   8,  8,  4,  4, 2, 2, 1, 1};
		{32768, 16384, 8192, 4096, 2048, 1024, 512, 256, 128, 64, 32, 16, 8, 4, 2, 1};
#endif

#endif
