/*
 * mempopcnt.c
 * popcount a mem region, ppc implementation
 *
 * Copyright (c) 2009-2011 Jan Seiffert
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
 * $Id:$
 */

#if defined(__ALTIVEC__) && defined(__GNUC__)
/* We use the GCC vector internals, to make things simple for us. */
# include <altivec.h>
# include "ppc_altivec.h"

/*
 * Deppending on the field length, this is 3 to 2 times faster
 * then the generic version, and prop. only capped by mem bandwith
 */

# define vec_popcnt(x) \
	vec_add(vec_perm(lutl, v_0, vec_and(x, v_15)), vec_perm(lutl, v_0, vec_sr(x, v_4)))

size_t mempopcnt(const void *s, size_t len)
{
	vector unsigned char v_0, v_4, v_15;
	vector unsigned char lutl;
	vector unsigned char c, v_perm;
	vector unsigned int v_sum;
	unsigned char *p;
	size_t r, block_num;
	uint32_t ret;
	unsigned shift, f;

	if(HOST_IS_BIGENDIAN)
		lutl  = (vector unsigned char){0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
	else
		lutl  = (vector unsigned char){4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0};

	block_num = DIV_ROUNDUP(len, 512);
	f  = 512;
	f |= block_num >= 256 ? 0 : block_num << 16;
	vec_dst((const unsigned char *)s, f, 3);

	v_4   = vec_splat_u8(4);
	v_0   = vec_splat_u8(0);
	v_15  = vec_splat_u8(15);
	v_sum = (vector unsigned int)v_0;
	/*
	 * Sometimes you need a new perspective, like the altivec
	 * way of handling things.
	 * Lower address bits? Totaly overestimated.
	 *
	 * We don't precheck for alignment.
	 * Instead we "align hard", do one load "under the address",
	 * mask the excess info out and afterwards we are fine to go.
	 */
	p = (unsigned char *)ALIGN_DOWN(s, SOVUC);
	shift = ALIGN_DOWN_DIFF(s, SOVUC);
	c = vec_ldl(0, (const vector unsigned char *)p);
	v_perm = vec_lvsl(0, (unsigned char *)(uintptr_t)s);
	c = vec_perm(c, v_0, v_perm);
	if(len >= SOVUC || len + shift >= SOVUC)
	{
		p    += SOVUC;
		len  -= SOVUC - shift;
		v_sum = vec_sum4s(vec_popcnt(c), v_sum);

		if(len >= SOVUC * 8)
		{
			vector unsigned char v_ones, v_twos, v_fours, v_eights;
			vector unsigned int v_sum_t;

			v_sum_t = (vector unsigned int)v_0;;
			v_fours = v_twos = v_ones = v_0;
			while(len >= SOVUC * 8)
			{
				vector unsigned char v_sumb = v_0;

				f  = 512;
				f |= block_num >= 256 ? 0 : block_num << 16;
				vec_dst(p, f, 3);

				r    = len / (SOVUC * 8);
				r    = r > 31 ? 31 : r;
				len -= r * SOVUC * 8;
				/*
				 * Altivec has it's wonderfull vec_perm. This way one can build
				 * a popcnt quite fast. Build a lut for 4 to 5 bits at once,
				 * tada. Still this is not the fasted way...
				 * You have to split the upper and lower bits in a byte, mask,
				 * shift, add, and you stress the permutation engine. The permutation
				 * engine is powerfull, but has it's limits, and is prop. single issue.
				 * Esp. since they made the perm one cycle slower on >= G5.
				 * Also you still have to do the horizontal addition because you add
				 * up only bytes. Cool vector ops for the rescue again (sums4s, etc.)
				 * but they are also not the fasted, and frequently requiered,
				 * breaking the loop. And ppc is not the greatest fan of conditional
				 * branches....
				 * So some trick for the rescue: compression (Harley's Method).
				 * We shove several vector (8) into one vector and count that (its
				 * like counting the carry of that). With some simple bin ops
				 * (very fast, multiple issue) and a few regs (several instructions
				 * in flight, scheduling) we may get a lot better.
				 *
				 * This is only a win when the vector engine for bin ops is better
				 * than the permutation engine, not on some cut down embedded chip,
				 * to make up for the additional ops.
				 *
				 * Also GCC like to use all regs of Altivec, pushing nearly 10 regs
				 * to the stack, which means this could be a loss on small length
				 * (the push is done in the prologue/epilogue, not when we near the
				 * inner loop...)
				 */
				for(; r; r--, p += SOVUC * 8)
				{
					vector unsigned char v_twos_l, v_twos_h, v_fours_l, v_fours_h, c1, c2;

# define CSA(h,l, a,b,c) \
	{vector unsigned char u = vec_xor(a, b); \
	 vector unsigned char v = c; \
	 h = vec_or(vec_and(a, b), vec_and(u, v)); \
	 l = vec_xor(u, v);}
					c1 = vec_ldl(0 * SOVUC, (const vector unsigned char *)p);
					c2 = vec_ldl(1 * SOVUC, (const vector unsigned char *)p);
					CSA(v_twos_l, v_ones, v_ones, c1, c2)
					c1 = vec_ldl(2 * SOVUC, (const vector unsigned char *)p);
					c2 = vec_ldl(3 * SOVUC, (const vector unsigned char *)p);
					CSA(v_twos_h, v_ones, v_ones, c1, c2)
					CSA(v_fours_l, v_twos, v_twos, v_twos_l, v_twos_h)
					c1 = vec_ldl(4 * SOVUC, (const vector unsigned char *)p);
					c2 = vec_ldl(5 * SOVUC, (const vector unsigned char *)p);
					CSA(v_twos_l, v_ones, v_ones, c1, c2)
					c1 = vec_ldl(6 * SOVUC, (const vector unsigned char *)p);
					c2 = vec_ldl(7 * SOVUC, (const vector unsigned char *)p);
					CSA(v_twos_h, v_ones, v_ones, c1, c2)
					CSA(v_fours_h, v_twos, v_twos, v_twos_l, v_twos_h)
					CSA(v_eights, v_fours, v_fours, v_fours_l, v_fours_h)
# undef CSA
					v_sumb = vec_add(v_sumb, vec_popcnt(v_eights));
				}
				v_sum_t = vec_sum4s(v_sumb, v_sum_t);
				block_num = DIV_ROUNDUP(len, 512);
			}
			v_sum_t = vec_sl(v_sum_t, vec_splat_u32(3)); /* *8 */
			v_sum_t = vec_msum(vec_popcnt(v_fours), vec_splat_u8(4), v_sum_t);
			v_sum_t = vec_msum(vec_popcnt(v_twos),  vec_splat_u8(2), v_sum_t);
			v_sum_t = vec_sum4s(vec_popcnt(v_ones), v_sum_t);
			v_sum   = vec_add(v_sum_t, v_sum);
		}
		if(len >= SOVUC)
		{
			vector unsigned char v_sumb = v_0;

			r    = len / SOVUC; /* can only be 1...7 here */
			len -= r * SOVUC;
			for(; r; r--, p += SOVUC) {
				c      = vec_ldl(0, (const vector unsigned char *)p);
				v_sumb = vec_add(v_sumb, vec_popcnt(c));
			}
			v_sum = vec_sum4s(v_sumb, v_sum);
		}

		if(len)
			c = vec_ldl(0, (const vector unsigned char *)p);
	}
	vec_dss(3);
	if(len) {
		v_perm = vec_lvsr(0, (unsigned char *)(uintptr_t)(SOVUC - len));
		c      = vec_perm(v_0, c, v_perm);
		v_sum  = vec_sum4s(vec_popcnt(c), v_sum);
	}
// TODO: uff, shit, does saturation and signed harm here?
	/* sure, this way we can only count to 0x7fffffff??? */
	v_sum = (vector unsigned int)vec_sums((vector signed int)v_sum, (vector signed int)v_0);
	v_sum = vec_splat(v_sum, 3); /* splat */
	vec_ste(v_sum, 0, &ret); /* transfer */
	return ret;
}
#else
# if defined(_ARCH_PWR5)
/*
 * POWER 5 (ISA 2.02) has a popcntb instruction.
 * (Category "Phased in v2.05", whatever...)
 * But:
 * popcnt instructions, even if nice, are seldomly the fasted
 * instructions. Often they also have another knack, like only
 * one of several ALUs (the "complex ALU" or somehting like that)
 * can do it (so your throughput is capped). PowerPC is RISC, so
 * its popcnt is maybe "fast", but i guess it has some limitation,
 * because it is not the most important instruction.
 *
 * So let's do less poping and use another nice trick: compression
 * (Harley's Method).
 * We shove several words (8) into one word and count that (its
 * like counting the carry of that). With some simple bin ops
 * (often very fast, multiple issue) and a few regs (several
 * instructions in flight, scheduling) we may get a lot better.
 * This is only a win on big iron (several deep ALUs, etc.) not
 * on some cut down embedded chip, to make up for the additional
 * ops, but hopefully ppc64 qualifys for that...
 * This way we can also stay longer in the loop, less sideway
 * addition.
 */
static inline size_t popcountst_int1(size_t n)
{
	size_t tmp;
	__asm__ ("popcntb	%0, %1" : "=r" (tmp) : "r" (n));
	return (tmp * MK_C(0x01010101UL)) >> (SIZE_T_BITS - 8);
}

static inline size_t popcountst_int2(size_t n, size_t m)
{
	size_t tmp1, tmp2;
	__asm__ ("popcntb	%0, %1" : "=r" (tmp1) : "r" (n));
	__asm__ ("popcntb	%0, %1" : "=r" (tmp2) : "r" (m));
	return ((tmp1 + tmp2) * MK_C(0x01010101UL)) >> (SIZE_T_BITS - 8);
}

static inline size_t popcountst_int4(size_t n, size_t m, size_t o, size_t p)
{
	size_t tmp1, tmp2, tmp3, tmp4;
	__asm__ ("popcntb	%0, %1" : "=r" (tmp1) : "r" (n));
	__asm__ ("popcntb	%0, %1" : "=r" (tmp2) : "r" (m));
	__asm__ ("popcntb	%0, %1" : "=r" (tmp3) : "r" (o));
	__asm__ ("popcntb	%0, %1" : "=r" (tmp4) : "r" (p));
	return (((tmp1 + tmp2) * MK_C(0x01010101UL)) >> (SIZE_T_BITS - 8)) +
	       (((tmp3 + tmp4) * MK_C(0x01010101UL)) >> (SIZE_T_BITS - 8));
}

static inline size_t popcountst_b(size_t n)
{
	size_t tmp;
	__asm__ ("popcntb	%0, %1" : "=r" (tmp) : "r" (n));
	return tmp;
}
#  define NO_GEN_POPER
# endif
# include "../generic/mempopcnt.c"
#endif

static char const rcsid_mpp[] GCC_ATTR_USED_VAR = "$Id:$";
