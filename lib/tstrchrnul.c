/*
 * tstrchrnul.c
 * tstrchrnul
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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
 * tstrchrnul - strchr which also returns a pointer to '\0'
 * s: the string to search
 * c: the character to search
 *
 * return value: a pointer to the character c or to the '\0'
 *
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"
#include "tchar.h"

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/tstrchrnul.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/tstrchrnul.c"
# elif defined(__alpha__)
#  include "alpha/tstrchrnul.c"
# elif defined(__arm__)
#  include "arm/tstrchrnul.c"
# elif defined(__ia64__)
#  include "ia64/tstrchrnul.c"
# elif defined(__hppa__) || defined(__hppa64__)
#  include "parisc/tstrchrnul.c"
# else
#  include "generic/tstrchrnul.c"
# endif
#else
# include "generic/tstrchrnul.c"
#endif
