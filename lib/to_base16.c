/*
 * to_base16.c
 * convert binary string to hex
 *
 * Copyright (c) 2010-2011 Jan Seiffert
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
 * to_base16 - convert a binary string to a hex string
 * dst: memory where to write to
 * src: string to convert
 * num: number of bytes to convert
 *
 * return value: pointer behind the converted string
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/to_base16.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
#  include "ppc/to_base16.c"
# elif defined(__alpha__)
#  include "alpha/to_base16.c"
# elif defined(__arm__)
#  include "arm/to_base16.c"
# elif defined(__ia64__)
#  include "ia64/to_base16.c"
# elif defined(__tile__)
#  include "tile/to_base16.c"
# else
#  include "generic/to_base16.c"
# endif
#else
# include "generic/to_base16.c"
#endif
