/*
 * strlpcpy.c
 * strlpcpy for efficient concatination
 *
 * Copyright (c) 2008-2011 Jan Seiffert
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
 * strlpcpy - strlcpy which returns the end of the copied region
 * dst: where to copy to
 * src: from where to copy
 * maxlen: the maximum length
 *
 * return value: a pointer behind the last copied char,
 *               but not more than maxlen.
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/strlpcpy.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/strlpcpy.c"
# elif defined(__tile__)
#  include "tile/strlpcpy.c"
# else
#  include "generic/strlpcpy.c"
# endif
#else
# include "generic/strlpcpy.c"
#endif
