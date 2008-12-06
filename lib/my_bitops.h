/*
 * my_bitops.h
 * header-file for some global usefull bitbanging
 * functions
 *
 * Copyright (c) 2004-2008 Jan Seiffert
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
 * $Id: my_bitops.h,v 1.7 2005/11/05 10:17:17 redbully Exp redbully $
 */

#ifndef LIB_MY_BITOPS_H
#define LIB_MY_BITOPS_H

# include <sys/types.h>
# include <string.h>
# include "other.h"

# define LIB_MY_BITOPS_EXTRN(x) x GCC_ATTR_VIS("hidden")
# if defined(I_LIKE_ASM)
#  define LIB_MY_BITOPS_EXTRN_P(x, y ,z) extern x (*y) z GCC_ATTR_VIS("hidden")
# else
#  define LIB_MY_BITOPS_EXTRN_P(x, y ,z) x y z GCC_ATTR_VIS("hidden")
# endif
# ifdef ADLER32_C
LIB_MY_BITOPS_EXTRN(uint32_t adler32(uint32_t adler, const uint8_t *buf, unsigned len));
# endif
LIB_MY_BITOPS_EXTRN_P(size_t, popcountst, (size_t n) GCC_ATTR_CONST);
LIB_MY_BITOPS_EXTRN(size_t flsst(size_t find) GCC_ATTR_CONST);

LIB_MY_BITOPS_EXTRN_P(void *, memxor, (void *dst, const void *src, size_t len));
LIB_MY_BITOPS_EXTRN_P(void *, memand, (void *dst, const void *src, size_t len));
LIB_MY_BITOPS_EXTRN_P(void *, memneg, (void *dst, const void *src, size_t len));
LIB_MY_BITOPS_EXTRN(void *mem_searchrn(void *src, size_t len));
# ifndef HAVE_MEMPCPY
LIB_MY_BITOPS_EXTRN(void *mempcpy(void *restrict dst, const void *restrict src, size_t len));
#  define MEMPCPY_DEFINED
# endif
LIB_MY_BITOPS_EXTRN(char *strpcpy(char *restrict dst, const char *restrict src));
LIB_MY_BITOPS_EXTRN(char *strnpcpy(char *restrict dst, const char *restrict src, size_t maxlen));
# ifndef HAVE_STRNLEN
LIB_MY_BITOPS_EXTRN(size_t strnlen(const char *s, size_t maxlen) GCC_ATTR_PURE);
#  define STRNLEN_DEFINED
# endif

# define strlitcpy(x, y)	(memcpy((x), (y), str_size(y)))
# define strplitcpy(x, y)	((char *)memcpy((x), (y), str_size(y)) + str_size(y))

static inline void strreverse(char *begin, char *end)
{
	char tchar;

	while(end > begin)
		tchar = *end, *end-- = *begin, *begin++ = tchar;
}

struct test_cpu_feature
{
	void (*func)(void);
	int (*callback)(void);
	long flags_needed;
};

LIB_MY_BITOPS_EXTRN(void test_cpu_feature(void *, const struct test_cpu_feature *, size_t));
LIB_MY_BITOPS_EXTRN(int test_cpu_feature_avx_callback(void));
LIB_MY_BITOPS_EXTRN(int test_cpu_feature_3dnow_callback(void));

# endif
