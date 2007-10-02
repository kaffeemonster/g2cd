/*
 * my_bitopsm.h
 * bitbanging helber defines
 *
 * Copyright (c) 2006 Jan Seiffert
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
	/* bits in a size_t */
# define SIZE_T_BITS	(SOST * CHAR_BIT)
	/* is pointer x aligned on a power of n */
# define IS_ALIGN(x, n)	(!(((intptr_t)(x)) & ((n) - 1L)))
	/* align pointer x on a power of n */
# define ALIGN(x, n) \
	((intptr_t)((x)+(n) - 1L) & ~((intptr_t)(n) - 1L))
	/* roud up an int to match alignment */
#define ALIGN_SIZE(x, n) \
	(((x) + (n) - 1L) & ~((n) - 1L))
	/* some magic to build a constant for 32 & 64 bit, without
	 * the need for LL suffix
	 */
# define MK_C(x)	((size_t)x | ((size_t)x << SIZE_T_BITS/2))
	/* more magic: map the power of 2 of size_t bits to a linear
	 * index, WARNING: it works inverted, Ex(i386):
	 * L2P(0) = 32, L2P(1) = 16, L2P(2) = 8 ...
	 */
# define L2P(x)	(SIZE_T_BITS / (1 << x))
#endif
