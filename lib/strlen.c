/*
 * strlen.c
 * strlen just for fun
 *
 * Copyright (c) 2008 Jan Seiffert
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
# else
#  include "generic/strlen.c"
# endif
#else
# include "generic/strlen.c"
#endif
