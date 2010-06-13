/*
 * memchr.c
 * memchr
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
 * memchr - memchr
 * s: the mem to search
 * c: the byte to search
 * n: the length
 *
 * return value: a pointer to the byte c or NULL
 */

#define IN_STRWHATEVER
#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/memchr.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/memchr.c"
# elif defined(__alpha__)
#  include "alpha/memchr.c"
# elif defined(__arm__)
#  include "arm/memchr.c"
# else
#  include "generic/memchr.c"
# endif
#else
# include "generic/memchr.c"
#endif

#undef memchr
void *memchr(const void *s, int c, size_t n) GCC_ATTR_ALIAS("my_memchr");
/* EOF */
