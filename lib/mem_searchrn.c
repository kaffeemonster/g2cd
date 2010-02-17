/*
 * mem_searchrn.c
 * search mem for a \r\n
 *
 * Copyright (c) 2008-2010 Jan Seiffert
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
 * mem_searchrn - search memory for a \r\n
 * src: where to search
 * len: the maximum length
 *
 * return value: a pointer at the \r\n, or NULL if not found.
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"


#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/mem_searchrn.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/mem_searchrn.c"
# elif defined(__alpha__)
#  include "alpha/mem_searchrn.c"
# elif defined(__arm__)
#  include "arm/mem_searchrn.c"
# else
#  include "generic/mem_searchrn.c"
# endif
#else
# include "generic/mem_searchrn.c"
#endif
