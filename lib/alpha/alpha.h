/*
 * alpha.h
 * special alpha instructions
 *
 * Copyright (c) 2009-2012 Jan Seiffert
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

#ifndef ALPHA_ALPHA_H
# define ALPHA_ALPHA_H

# define SOUL (sizeof(unsigned long))
# define SOULM1 (SOUL - 1)

# include "../other.h"

# if _GNUC_PREREQ(3, 3)
#  define cmpbge	__builtin_alpha_cmpbge
#  define zapnot	__builtin_alpha_zapnot
#  define extbl	__builtin_alpha_extbl
#  define extqh	__builtin_alpha_extqh
#  define extql	__builtin_alpha_extql
# else
static inline unsigned long cmpbge(unsigned long a, unsigned long b)
{
	unsigned long r;
	asm ("cmpbge	%r1, %2, %0" : "=r" (r) : "rJ" (a), "rI" (b));
	return r;
}
static inline unsigned long zapnot(unsigned long a, unsigned long mask)
{
	unsigned long r;
	asm ("zapnot	%r1, %2, %0" : "=r" (r) : "rJ" (a), "rI" (mask));
	return r;
}
static inline unsigned long extbl(unsigned long a, unsigned long mask)
{
	unsigned long r;
	asm("extbl	%r1, %2, %0" : "=r" (r) : "rJ" (a), "rI" (mask));
	return r;
}
static inline unsigned long extqh(unsigned long a, unsigned long mask)
{
	unsigned long r;
	asm("extqh	%r1, %2, %0" : "=r" (r) : "rJ" (a), "rI" (mask));
	return r;
}
static inline unsigned long extql(unsigned long a, unsigned long mask)
{
	unsigned long r;
	asm("extql	%r1, %2, %0" : "=r" (r) : "rJ" (a), "rI" (mask));
	return r;
}
# endif
static inline unsigned long ldq_u(const unsigned long *a)
{
	unsigned long r;
	asm("ldq_u	%0, %1" : "=r" (r) : "m" (*a));
	return r;
}

# ifdef __alpha_cix__
#  if _GNUC_PREREQ(3, 3)
#   define ctlz(x) ((size_t)__builtin_alpha_ctlz(x))
#   define cttz(x) ((size_t)__builtin_alpha_cttz(x))
#   define ctpop(x) ((size_t)__builtin_alpha_ctpop(x))
#  else
static inline size_t ctlz(unsigned long a)
{
	size_t r;
	asm ("ctlz	%1, %0" : "=r" (r) : "r" (a));
	return r;
}

static inline size_t ctpop(unsigned long a)
{
	size_t r;
	asm ("ctpop	%1, %0" : "=r" (r) : "r" (a));
	return r;
}

static inline size_t cttz(unsigned long a)
{
	size_t r;
	asm ("cttz	%1, %0" : "=r" (r) : "r" (a));
	return r;
}
#  endif
#  define alpha_nul_byte_index_b(x) ((HOST_IS_BIGENDIAN) ? ctlz(x) : cttz(x))
#  define alpha_nul_byte_index_e(x) ((HOST_IS_BIGENDIAN) ? ctlz(x) - (SOULM1 * BITS_PER_CHAR) : cttz(x))
#  define alpha_nul_byte_index_e_last(x) ((HOST_IS_BIGENDIAN) ? (7u-cttz(x)) : (7u-(ctlz(x)) - (SOULM1 * BITS_PER_CHAR)))
# else
/*
 * if CIX is not avail... we need a fallback
 *
 * WARNING: these generic functions have limited value range!
 * do not call with zero as argument (the popcount version is fine)
 * do not use more than the low byte for cttz
 * do not use more than the low byte for ctlz
 */
static inline size_t cttz(unsigned long a)
{
#  if 0
	/* misuse extbl for a table lookup */
	/* extbl inverts the mask on big endian targets, counter by
	 * swapping the constant lookup table around
	 */
	return extbl(le64_to_cpu(0x0506030704020100L), (((a & -a) * 23) & 0xff) >> 5);
	/*
	 * This should compile to something like this:
	 *
	 *   21 05 e2 43     negq    t1,t0
	 *   02 00 22 44     and     t0,t1,t1
	 *   60 05 42 40     s4subq  t1,t1,v0
	 *   62 07 02 40     s8subq  v0,t1,t1
	 *   00 00 3d 24     ldah    t0,0(gp)
	 *   00 f0 5f 44     and     t1,0xff,v0
	 *   80 b6 00 48     srl     v0,0x5,v0
	 *   00 00 21 a4     ldq     t0,0(t0)
	 *   c0 00 20 48     extbl   t0,v0,v0
	 *
	 * No condinional branches, no loops, 9 clean instructions.
	 * Sure, a real cttz is better, but for our usecase (finding
	 * the evil zero byte, thanks to "cmpbge" already simplified)
	 * better then a lot of other painful solutions.
	 */
#  else
	/* turn the lowest 0 bit run into 1 bits */
	a  = a & -a;
	/* do a byte popcount */
	a -= (a & 0xaa) >> 1;
	a  = (a & 0x33) + ((a >> 2) & 0x33);
	return ((a >> 4) + a) & 0x0f;
	/*
	 * This should compile to something like this
	 *
	 *   21 05 e2 43     negq    t1,t0
	 *   02 00 22 44     and     t0,t1,t1
	 *   01 50 55 44     and     t1,0xaa,t0
	 *   81 36 20 48     srl     t0,0x1,t0
	 *   22 05 41 40     subq    t1,t0,t1
	 *   81 56 40 48     srl     t1,0x2,t0
	 *   02 70 46 44     and     t1,0x33,t1
	 *   01 70 26 44     and     t0,0x33,t0
	 *   01 04 22 40     addq    t0,t1,t0
	 *   80 96 20 48     srl     t0,0x4,v0
	 *   01 04 01 40     addq    v0,t0,t0
	 *   00 f0 21 44     and     t0,0xf,v0
	 *
	 * No condinional branches, no loops, 12 clean instructions.
	 * The advantage is that we do not need a constant which has
	 * to be loaded from mem, which would incour some big latency.
	 */
#  endif
}

static inline size_t ctlz(unsigned long a)
{
	/* propagate the most significant set bit */
	a  |= (a >> 1);
	a  |= (a >> 2);
	a  |= (a >> 4);
#  if 0
	/* single it out */
	a   = a & ~(a >> 1);
	/* do a cttz by lookup, only with the lookup values in places
	 * suitable for a ctlz
	 */
	return extbl(le64_to_cpu(0x0201040003050607L), ((a * 23) & 0xff) >> 5);
#  else
	/* do a byte popcount */
	a -= (a & 0xaa) >> 1;
	a  = (a & 0x33) + ((a >> 2) & 0x33);
	return SOUL - (((a >> 4) + a) & 0x0f);
#  endif
}
#  define alpha_nul_byte_index_b(x) ((HOST_IS_BIGENDIAN) ? ctlz((x) >> SOULM1 * BITS_PER_CHAR) : cttz(x))
#  define alpha_nul_byte_index_e(x) ((HOST_IS_BIGENDIAN) ? ctlz(x) : cttz(x))
#  define alpha_nul_byte_index_e_last(x) ((HOST_IS_BIGENDIAN) ? (7u-cttz(x)) : (7u-ctlz(x)))
# endif

# define cmpbeqz(a) (cmpbge(0, a))
# define cmpbeqm(a, m) (cmpbeqz((a) ^ (m)))
# define cmpbgt(a, c) (cmpbge(a, ((c) + 1) * 0x0101010101010101UL))
# define cmpblt(a, c) (cmpbge(((c) - 1) * 0x0101010101010101UL, a))
# define cmpb_between(a, b, c) (cmpbgt(a, b) & cmpblt(a, c))

# ifdef __alpha_max__
#  if _GNUC_PREREQ(3, 3)
#   define unpkbw	__builtin_alpha_unpkbw
#   define perr	__builtin_alpha_perr
#  else
static inline unsigned long unpkbw(unsigned long a)
{
	unsigned long r;
	asm (".arch ev6; unpkbw	%r1, %0" : "=r" (r) : "rJ" (a));
	return r;
}
static inline unsigned long perr(unsigned long a, unsigned long b)
{
	unsigned long r;
	asm (".arch ev6; perr	%r1, %r2, %0" : "=r" (r) : "rJ" (a), "rJ" (b));
	return r;
}
#  endif
# endif
#endif
