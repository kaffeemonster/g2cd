/*
 * strpcpy.c
 * strpcpy for efficient concatenation
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
	size_t i = 0;

	for(; src[i]; i++)
		dst[i] = src[i];
	dst += i;
	*dst = '\0';
	return dst;
}

static char const rcsid_spcg[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
