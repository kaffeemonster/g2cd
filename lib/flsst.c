/*
 * flsst.c
 * find last set in size_t
 *
 * Copyright (c) 2004,2005,2006 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2
 * of the License, or any later version.
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
 * $Id: flsst.c,v 1.1 2006/01/20 02:42:58 redbully Exp redbully $
 */

#include "my_bitops.h"
#include "my_bitopsm.h"

/*
 * flsst - find last set (or find most significant bit)
 *         in size_t
 * find: number to crawl
 *
 * return value: index of the most significant one bit, index
 *               starts at one, gives zero if no bit was found.
 */
/* size_t flsst(size_t find) */

#ifdef I_LIKE_ASM
# ifdef __i386__
#  include "i386/flsst.c"
# elif __x86_64__
#  include "x86_64/flsst.c"
# elif __powerpc__
#  include "ppc/flsst.c"
# elif __powerpc64__
#  include "ppc64/flsst.c"
# elif __alpha__
#  include "alpha/flsst.c"
# else
#  include "generic/flsst.c"
# endif
#else
# include "generic/flsst.c"
#endif