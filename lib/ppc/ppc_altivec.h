/*
 * ppc_altivec.h
 * little ppc altivec helper
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

#ifndef PPC_ALTIVEC_H
# define PPC_ALTIVEC_H

# define SOVUC (sizeof(vector unsigned char))
# define SOVUCM1 (SOVUC - 1)

/* you have to love GCC, it does not take NULL on vec_lvsl ... */
static inline vector unsigned char vec_identl(unsigned level)
{
	vector unsigned char ret;
	asm("lvsl %0,%1,%2" : "=v" (ret) : "r" (level), "r" (0));
	return ret;
}

static inline vector unsigned char vec_ident_rev()
{
	return vec_xor(vec_identl(0), vec_splat_u8(15));
}

static inline vector unsigned char vec_align(const char *p)
{
	return vec_lvsl(0, (const volatile unsigned char *)p);
}

static inline vector unsigned char vec_align_and_rev(const char *p)
{
	vector unsigned char ret = vec_align(p);
	ret = vec_perm(ret, vec_splat_u8(0), vec_ident_rev());
	return ret;
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
	vector unsigned short vr_h, vr_l;

// TODO: Transfer variable aligned to 16???
	uint32_t r;

	vr = vec_and(vr, vec_splat_u8(1)); /* single bit */
	factor = vec_xor(vec_identl(0), vec_splat_u8(15)); /* get reverse ident */
//	factor = vec_add(swap_ident, vec_splat_u8(1));
	vr_h = vec_sl((vector unsigned short)vec_unpackh(vr), (vector unsigned short)vec_unpackh((vector signed char)factor)); /* shift */
	vr_l = vec_sl((vector unsigned short)vec_unpackl(vr), (vector unsigned short)vec_unpackl((vector signed char)factor));
	vri = (vector unsigned int)vec_sums((vector int)vr_l, (vector int)vec_splat_u8(0)); /* consolidate */
	vri = (vector unsigned int)vec_sums((vector int)vr_h, (vector int)vri);
	vri = vec_splat(vri, 3); /* splat */
	vec_ste(vri, 0, &r); /* transfer */
	return r;
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
