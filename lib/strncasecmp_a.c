/*
 * strncasecmp_a.c
 * strncasecmp ascii only
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
 * strncasecmp_a - strncasecmp ascii only
 * s1: one string you want to compare
 * s2: other string you want to compare
 * n: the maximum length
 *
 * return value: an integer less than, equal to, or greater than zwero
 * if s1 (or the first n bytes thereof) is found, respectivly, to be
 * less than, to match, or to be greater than s2.
 *
 * NOTE: we asume that the caller garantees that maxlen bytes
 *       are accessible at s
 *
 * Since we only want to compare ascii strings most of the time, don't
 * bother with any fancy locale lookups for the ingnore-case-part, just
 * use some good old math, so we can speed up this operation with vector
 * instructions.
 * This is dirty and shurely violates everything, but as the functions
 * says, "I'm for ascii data".
 */

#define IN_STRWHATEVER
#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

/* int strncasecmp_a(const char *s1, const char *s2, size_t n); */

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/strncasecmp_a.c"
# else
#  include "generic/strncasecmp_a.c"
# endif
#else
# include "generic/strncasecmp_a.c"
#endif
