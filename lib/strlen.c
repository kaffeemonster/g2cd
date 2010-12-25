/*
 * strlen.c
 * strlen just for fun
 *
 * Copyright (c) 2008-2010 Jan Seiffert
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
 * strlen - strlen, you know...
 * s: string you need the length
 *
 * return value: the string length
 *
 * since strlen is a crucial function (not in my code, but
 * else where) a slow implementation sucks.
 * So we always provide one.
 */

#define IN_STRWHATEVER
#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifndef STRLEN_DEFINED
size_t strlen(const char *s);
# define STRLEN_DEFINED
#endif

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/strlen.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/strlen.c"
# elif defined(__alpha__)
#  include "alpha/strlen.c"
# elif defined(__arm__)
#  include "arm/strlen.c"
# elif defined(__mips__)
#  include "mips/strlen.c"
# elif defined(__ia64__)
#  include "ia64/strlen.c"
# elif defined(__hppa__) || defined(__hppa64__)
#  include "parisc/strlen.c"
# else
#  include "generic/strlen.c"
# endif
#else
# include "generic/strlen.c"
#endif
