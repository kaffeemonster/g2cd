/*
 * my_bitopsm.h
 * bitbanging helber defines
 *
 * Copyright (c) 2006-2010 Jan Seiffert
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

#ifndef LIB_MY_BITOPSM_H
# define LIB_MY_BITOPSM_H

#include <limits.h>

/* some nice handy things */
	/* we always need sizeof(size_t) */
# define SOST	(sizeof(size_t))
	/* bytes of size_t - 1 */
# define SOSTM1	(SOST - 1L)
	/* and sometimes sizeof(uint32_t) */
# define SO32	(sizeof(uint32_t))
	/* bytes of size_t - 1 */
# define SO32M1	(SO32 - 1L)
	/* bits in a size_t */
# define SIZE_T_BITS	(SOST * CHAR_BIT)
# define ROUND_ALIGN(x, n) \
	(((x)+(n) - 1L) & ~((n) - 1L))
# define ROUND_TO(x , n) \
	((x) & ~((n) - 1L))
	/* is pointer x aligned on a power of n */
# define IS_ALIGN(x, n)	(!(((intptr_t)(x)) & ((n) - 1L)))
	/* align pointer x on a power of n */
# define ALIGN(x, n) \
	((intptr_t)((x)+(n) - 1L) & ~((intptr_t)(n) - 1L))
	/* get the bytes till alignment is met */
#define ALIGN_DIFF(x, n) \
	(((intptr_t)((x)+(n) - 1L) & ~((intptr_t)(n) - 1L)) - (intptr_t)(x))
	/* roud up an int to match alignment */
#define ALIGN_SIZE(x, n) \
	(((x) + (n) - 1L) & ~((n) - 1L))
	/* yup, sometimes we have to align down */
#define ALIGN_DOWN(x, n) \
	(((intptr_t)(x)) & ~((intptr_t)(n) - 1L))
	/* and the diff to it */
#define ALIGN_DOWN_DIFF(x, n) \
	(((intptr_t)(x)) & ((intptr_t)(n) - 1L))
	/* divide while always rounding up */
#define DIV_ROUNDUP(a, b) \
	(((a) + (b) - 1) / (b))
	/* some magic to build a constant for 32 & 64 bit, without
	 * the need for LL suffix
	 */
# define MK_C(x)	((size_t)x | ((size_t)x << SIZE_T_BITS/2))
	/* more magic: map the power of 2 of size_t bits to a linear
	 * index, WARNING: it works inverted, Ex(i386):
	 * L2P(0) = 32, L2P(1) = 16, L2P(2) = 8 ...
	 */
# define L2P(x)	(SIZE_T_BITS / (1 << x))
	/* Not for the faint at heart
	 * true if any byte in x is between m and n
	 * m and n have to be 7 Bit!
	 */
# define has_between(x, m, n) \
	(((~0UL/255*(127 + (n)) - ((x) & ~0UL/255*127)) & ~(x) & (((x) & ~0UL/255*127) + ~0UL/255*(127-m))) & ~0UL/255*128)
	/* true if any byte in x is greater than n
	 * n has to be 7 Bit!
	 */
# define has_greater(x, n) \
	((((x) + ~0UL/255*(127-(n))) | (x)) & ~0UL/255*128)
	/* true if any word in x is greater than n
	 * n has to be 15 Bit!
	 */
# define has_word_greater(x, n) \
	((((x) + ~0UL/65535*(32767-(n))) | (x)) & ~0UL/65535*32767)
	/* The wonders of binary foo
	 * true if any byte in the number is zero
	 */
# define has_nul_byte32(x) \
	(((x) -  0x01010101) & ~(x) &  0x80808080)
# define has_nul_byte(x) \
	(((x) -  MK_C(0x01010101)) & ~(x) &  MK_C(0x80808080))
# define nul_byte_index_l32(x) \
	((x) & 0x80U ? 0u : (((x) & 0x8000U) ? 1u : ((x) & 0x800000U ? 2u : ((x) & 0x80000000 ? 3u : 0u))))
# define nul_byte_index_b32(x) \
	((x) & 0x80000000U ? 0u : (((x) & 0x800000U) ? 1u : ((x) & 0x8000U ? 2u : ((x) & 0x80 ? 3u : 0u))))
# define nul_byte_index_l64(x) \
	(0x80808080U & (x) ? nul_byte_index_l32(x) : nul_byte_index_l32((x)>>32) + 4u)
# define nul_byte_index_b64(x) \
	(0x80808080U & ((x)>>32) ? nul_byte_index_b32((x)>>32) : nul_byte_index_b32(x) + 4u)
	/*	666655555555554444444444333333333322222222221111111111
	 *	3210987654321098765432109876543210987654321098765432109876543210
	 *	1000000010000000100000001000000010000000100000001000000010000000
	 */
# define has_nul_word(x) \
	(((x) -  MK_C(0x00010001)) & ~(x) &  MK_C(0x80008000))
# define nul_word_index_l32(x) \
	((x) & 0x8000U ? 0u : ((x) & 0x80000000U ? 1u : 0u))
# define nul_word_index_b32(x) \
	((x) & 0x80000000U ? 0u : ((x) & 0x8000U ? 1u : 0u))
# define nul_word_index_l64(x) \
	(0x80008000U & (x) ? nul_word_index_l32(x) : nul_word_index_l32((x)>>32) + 2u)
# define nul_word_index_b64(x) \
	(0x80008000U & ((x)>>32) ? nul_word_index_b32((x)>>32) : nul_word_index_b32(x) + 2u)
# define packedmask_832(x) \
	(((x) >> 7 | (x) >> 14 | (x) >> 21 | (x) >> 28) & 0x0F);
# define packedmask_864(x) \
	(((x) >> 7 | (x) >> 14 | (x) >> 21 | (x) >> 28 | (x) >>  35| (x) >> 42 | (x) >> 49 | (x) >> 56) & 0xFF);

#endif
