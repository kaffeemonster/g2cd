/*
 * mempcpy.c
 * mempcpy for non-GNU platforms
 *
 * Copyright (c) 2008-2010 Jan Seiffert
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
 * mempcpy - memcpy which returns the end of the copied region
 * dst: where to copy to
 * src: from where to copy
 * len: how much to copy
 *
 * return value: a pointer behind the last copied byte
 *
 * since mempcpy is a GNU extension we have to provide our own.
 * We simply always provide one.
 */

#define MEMPCPY_DEFINED
#include "../config.h"
#include "other.h"
#undef mempcpy
void *mempcpy(void *restrict dst, const void *restrict src, size_t len) GCC_ATTR_ALIAS("my_mempcpy");
#define IN_STRWHATEVER

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifdef I_LIKE_ASM
# if defined(__sparc) || defined(__sparc__)
	/* works for both */
#  include "sparc/mempcpy.c"
# else
#  include "generic/mempcpy.c"
# endif
#else
# include "generic/mempcpy.c"
#endif
/* EOF */
