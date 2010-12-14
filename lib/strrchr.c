/*
 * strrchr.c
 * strrchr
 *
 * Copyright (c) 2010 Jan Seiffert
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
 * strrchr - strchr, but returns the last occurrence
 * s: the string to search
 * c: the character to search
 *
 * return value: a pointer to the last occurrence of character c
 */

#define IN_STRWHATEVER
#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifndef STRRCHR_DEFINED
char *strrchr(const char *s, int c);
# define STRRCHR_DEFINED
#endif

#ifdef I_LIKE_ASM
# if defined(__alpha__)
#  include "alpha/strrchr.c"
# elif defined(__arm__)
#  include "arm/strrchr.c"
# elif defined(__hppa__) || defined(__hppa64__)
#  include "parisc/strrchr.c"
# elif defined(__ia64__)
#  include "ia64/strrchr.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/strrchr.c"
# elif defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/strrchr.c"
# else
#  include "generic/strrchr.c"
# endif
#else
# include "generic/strrchr.c"
#endif

char *rindex(const char *s, int c) GCC_ATTR_ALIAS("strrchr");
/* EOF */
