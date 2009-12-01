/*
 * flsst.c
 * find last set in size_t
 *
 * Copyright (c) 2004-2008 Jan Seiffert
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
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/flsst.c"
# elif defined(__mips)
#  include "mips/flsst.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
	/* works for both */
#  include "ppc/flsst.c"
# elif defined(__alpha__)
#  include "alpha/flsst.c"
# elif defined(__ARM_ARCH_5__) || defined(__ARM_ARCH_5T__) || \
	defined(__ARM_ARCH_5TE__) || defined(__ARM_ARCH_5TEJ__) || \
	defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || \
	defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || \
	defined(__ARM_ARCH_7A__)
#  include "arm/flsst.c"
# else
#  include "generic/flsst.c"
# endif
#else
# include "generic/flsst.c"
#endif
