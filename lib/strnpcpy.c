/*
 * strnpcpy.c
 * strnpcpy for efficient concatination
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
 * strnpcpy - strncpy which returns the end of the copied region
 * dst: where to copy to
 * src: from where to copy
 * maxlen: the maximum length
 *
 * return value: a pointer behind the last copied byte,
 *               but not more than maxlen.
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
char *strnpcpy(char *restrict dst, const char *restrict src, size_t maxlen)
{
	size_t i = 0;

	for(; i < maxlen && src[i]; i++)
		dst[i] = src[i];
	if(maxlen) {
		dst += i;
		*dst = '\0';
	}
	return dst;
}

static char const rcsid_snpcg[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
