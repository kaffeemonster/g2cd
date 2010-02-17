/*
 * memchr.c
 * memchr
 *
 * Copyright (c) 2010 Jan Seiffert
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
