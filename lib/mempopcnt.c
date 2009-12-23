/*
 * mempopcnt.c
 * popcount a mem region
 *
 * Copyright (c) 2009 Jan Seiffert
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

/*
 * mempopcnt - count the 1 bits in a memory region
 * s: the pointer to the memory region
 * len: the number of bytes there
 *
 * return value: the number of set bits
 *
 * WARNING: Make sure your memory region is not to large
 *          so the number of possible bits overflows size_t
 *          and sometimes less.
 *          In other words: This is not meant for very large
 *          arrays (rule of thumb: stay within 256MB).
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/mempopcnt.c"
# elif defined(__IA64__)
#  include "ia64/mempopcnt.c"
# elif defined(__sparc) || defined(__sparc_)
#  include "sparc/mempopcnt.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/mempopcnt.c"
# elif defined(__alpha__)
#  include "alpha/mempopcnt.c"
# elif defined(__arm__)
#  include "arm/mempopcnt.c"
# else
#  include "generic/mempopcnt.c"
# endif
#else
# include "generic/mempopcnt.c"
#endif
