/*
 * strchrnul.c
 * strchrnul for non-GNU platforms
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
 * strchrnul - strchr which also returns a pointer to '\0'
 * s: the string to search
 * c: the character to search
 *
 * return value: a pointer to the character c or to the '\0'
 *
 * strchrnul is a GNU extension, so we have to provide our own.
 */

#define IN_STRWHATEVER
#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifndef STRCHRNUL_DEFINED
char *strchrnul(const char *s, int c);
# define STRCHRNUL_DEFINED
#endif

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/strchrnul.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/strchrnul.c"
# elif defined(__alpha__)
#  include "alpha/strchrnul.c"
# elif defined(__arm__)
#  include "arm/strchrnul.c"
# elif defined(__ia64__)
#  include "ia64/strchrnul.c"
# else
#  include "generic/strchrnul.c"
# endif
#else
# include "generic/strchrnul.c"
#endif
