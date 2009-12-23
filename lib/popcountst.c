/*
 * popcountst.c
 * calculate popcount in size_t
 *
 * Copyright (c) 2004-2009 Jan Seiffert
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

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

/*
 * popcountst - count the bits set in a size_t
 * n: the size_t to count
 *
 * return value: number of bits set
 */
/* inline size_t popcountst(size_t n) */

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/popcountst.c"
# elif defined(__IA64__)
#  include "ia64/popcountst.c"
# elif defined(__sparc) || defined(__sparc__)
	/* works for both */
#  include "sparc/popcountst.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
	/* only available > POWER5, doesn't talk about 32Bit */
#  include "ppc/popcountst.c"
# elif defined(__alpha__)
#  include "alpha/popcountst.c"
# elif defined(__arm__)
#  include "arm/popcountst.c"
# else
#  include "generic/popcountst.c"
# endif
#else
# include "generic/popcountst.c"
#endif
