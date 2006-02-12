/*
 * my_bitops.h
 * header-file for some global usefull bitbanging
 * functions
 *
 * Copyright (c) 2004, 2005, 2006 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2
 * of the License, or any later version.
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
 * $Id: my_bitops.h,v 1.7 2005/11/05 10:17:17 redbully Exp redbully $
 */

#ifndef LIB_MY_BITOPS_H
#define LIB_MY_BITOPS_H

# include <sys/types.h>
# include "../other.h"

# define LIB_MY_BITOPS_EXTRN(x) x GCC_ATTR_VIS("hidden")

LIB_MY_BITOPS_EXTRN(inline void *memxor(void *dst, const void *src, size_t len));
LIB_MY_BITOPS_EXTRN(inline size_t popcountst(size_t n) GCC_ATTR_CONST);
LIB_MY_BITOPS_EXTRN(inline size_t flsst(size_t find) GCC_ATTR_CONST);
#ifndef HAVE_STRNLEN
LIB_MY_BITOPS_EXTRN(inline size_t strnlen(const char *s, size_t maxlen) GCC_ATTR_PURE);
#define STRNLEN_DEFINED
#endif

# endif
