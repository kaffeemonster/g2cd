/*
 * alpha.h
 * special alpha instructions
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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

# ifdef __alpha_cix__
/*
 * we could use the __builtin_clzl function and the like,
 * but someone thought their result should be signed...
 * gcc cannot gain _that_ much info from them anyway
 */
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
# else
/*
 * if CIX is not avail... we need a fallback
 *
 * WARNING: the generic functions have limited value range!
 * do not call with zero as argument
 * do not use more than the low byte for cttz
 * do not use more than the highest byte for ctlz
 */
static inline size_t cttz(unsigned long a)
{
	unsigned long r;
	asm("extbl	%1, %2, %0" : "=r" (r) : "r" (0x0506030704020100UL), "r" ((((a & -a) * 23) & 0xff) >> 5));
	return r;
}

static inline size_t ctlz(unsigned long a)
{
	unsigned long r;
	a >>= 56;
	a  |= (a >> 1);
	a  |= (a >> 2);
	a  |= (a >> 4);
	a   = a & ~(a >> 1);
	asm("extbl	%1, %2, %0" : "=r" (r) : "r" (0x0201040003050607UL), "r" (((a * 23) & 0xff) >> 5));
	return r;
}
# endif

# define alpha_nul_byte_index_b(x) ((HOST_IS_BIGENDIAN) ? ctlz((x)) : cttz((x)))
# define alpha_nul_byte_index_e(x) ((HOST_IS_BIGENDIAN) ? ctlz((x) << SOULM1 * BITS_PER_CHAR) : cttz((x)))

# define cmpbeqz(a) (cmpbge(0, a))
# if _GNUC_PREREQ(3, 3)
#  define cmpbge	__builtin_alpha_cmpbge
#  define zapnot	__builtin_alpha_zapnot
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
# endif
# ifdef __alpha_max__
#  if _GNUC_PREREQ(3, 3)
#   define unpkbw	__builtin_alpha_unpkbw
#  else
static inline unsigned long unpkbw(unsigned long a)
{
	unsigned long r;
	asm (".arch ev6; unpkbw	%r1, %0" : "=r" (r) : "rJ" (a));
	return r;
}
#  endif
# endif
#endif
