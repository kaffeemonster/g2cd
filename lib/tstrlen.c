/*
 * tstrlen.c
 * strlen for tchar
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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
 * tstrlen - strlen, you know...
 * s: string you need the length
 *
 * return value: the string length in tchars
 *
 */

#include "../config.h"
#include "tchar.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/tstrlen.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/tstrlen.c"
# elif defined(__alpha__)
#  include "alpha/tstrlen.c"
# elif defined(__arm__)
#  include "arm/tstrlen.c"
# elif defined(__ia64__)
#  include "ia64/tstrlen.c"
# else
#  include "generic/tstrlen.c"
# endif
#else
# include "generic/tstrlen.c"
#endif
