/*
 * strpcpy.c
 * strpcpy for efficient concatenation
 *
 * Copyright (c) 2008-2025 Jan Seiffert
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
 * strpcpy - strcpy which returns the end of the copied region
 * dst: where to copy to
 * src: from where to copy
 *
 * return value: a pointer behind the last copied char
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

/*
 * let the compiler get fancy at optimizing this.
 * if he doesn't know "restrict": it's not that often used
 * and propaply still better than strcpys and strcats
 * normaly used in this situation.
 */
char *strpcpy(char *restrict dst, const char *restrict src)
{
#if 1
	while('\0' != (*dst++ = *src++))
		/* nothing */;
	return dst - 1;
#else
	size_t i = 0;

	for(; src[i]; i++)
		dst[i] = src[i];
	dst += i;
	*dst = '\0';
	return dst;
#endif
}

static char const rcsid_spcg[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
