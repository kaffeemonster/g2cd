/*
 * memmove.c
 * memmove
 *
 * Copyright (c) 2010-2012 Jan Seiffert
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
 * memmove - memmove handling overlaping copys
 * dst: where to copy to
 * src: from where to copy
 * len: how much to copy
 *
 * return value: a pointer to dst
 */

#define IN_STRWHATEVER
#include "../config.h"
#include "other.h"

# include "my_bitops.h"
# include "my_bitopsm.h"

#ifndef MEMSPECIAL_DONT_DO_IT
void *my_memmove(void *dst, const void *src, size_t len)
{
	/* trick gcc to generate lean stack frame and do a tailcail */
	if(likely(dst <= src)) {
		if((char *)dst < (const char *)src - len)
			return my_memcpy(dst, src, len);
		else
			return my_memcpy_fwd(dst, src, len);
	} else {
		if((const char *)src < (char *)dst - len)
			return my_memcpy(dst, src, len);
		else
			return my_memcpy_rev(dst, src, len);
	}
}

/* memmove as a macro... yeah */
# undef memmove
void *memmove(void *dst, const void *src, size_t len) GCC_ATTR_ALIAS("my_memmove");
#endif

static char const rcsid_mvg[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
