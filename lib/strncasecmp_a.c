/*
 * strncasecmp_a.c
 * strncasecmp ascii only
 *
 * Copyright (c) 2008-2012 Jan Seiffert
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
 * strncasecmp_a - strncasecmp ascii only
 * s1: one string you want to compare
 * s2: other string you want to compare
 * n: the maximum length
 *
 * return value: an integer less than, equal to, or greater than zero
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
 *
 * We don't use any fancy tricks like (a[i] ^ b[i]) & ~0x20, because
 * this would be a sledgehammer tolower(). We only want to get rid
 * of locale foo and vectorize it, not making the use of this funktion
 * a PITA (caller must garantee input is ONLY printable characters,
 * otherwise it matches bullsh^weverything and the kitchen sink).
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
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/strncasecmp_a.c"
# elif defined(__alpha__)
#  include "alpha/strncasecmp_a.c"
# elif defined(__arm__)
#  include "arm/strncasecmp_a.c"
# elif defined(__ia64__)
#  include "ia64/strncasecmp_a.c"
# elif defined(__hppa__) || defined(__hppa64__)
#  include "parisc/strncasecmp_a.c"
# elif defined(__tile__)
#  include "tile/strncasecmp_a.c"
# else
#  include "generic/strncasecmp_a.c"
# endif
#else
# include "generic/strncasecmp_a.c"
#endif
