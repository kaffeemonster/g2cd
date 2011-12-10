/*
 * mem_spn_ff.c
 * count 0xff span length
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
 * $Id: $
 */

/*
 * mem_spn_ff - count the span length of a 0xff run
 * src: where to search
 * len: the maximum length
 *
 * return value: the number of consecutive 0xff bytes.
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"


#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/mem_spn_ff.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/mem_spn_ff.c"
# elif defined(__alpha__)
#  include "alpha/mem_spn_ff.c"
# elif defined(__tile__)
#  include "tile/mem_spn_ff.c"
# else
#  include "generic/mem_spn_ff.c"
# endif
#else
# include "generic/mem_spn_ff.c"
#endif
