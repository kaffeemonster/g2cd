/*
 * strnlen.c
 * strnlen for non-GNU platforms
 *
 * Copyright (c) 2005-2008 Jan Seiffert
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
 * strnlen - strlen with a n
 * s: string you need the length
 * maxlen: the maximum length
 *
 * return value: the string length or maxlen, what comes first
 *
 * since strnlen is a GNU extension we have to provide our own.
 * We always provide one, since this implementation is only
 * marginaly slower than glibc one.
 *
 * Timing on Athlon X2 2.5GHz, 3944416 bytes, 10000 runs, +-70ms:
 * glibc: 52100ms
 *   our: 54000ms
 *   SSE: 33050ms
 *  SSE2: 27100ms
 *
 * Timing on Athlon X2 2.5GHz, 261 bytes, 10000000 runs, +-10ms:
 * glibc: 3650ms
 *   our: 3530ms
 *   SSE: 1970ms
 *  SSE2: 2440ms
 */

#include "../config.h"
#include "other.h"

#include "my_bitops.h"
#include "my_bitopsm.h"

#ifndef STRNLEN_DEFINED
size_t strnlen(const char *s, size_t maxlen) GCC_ATTR_VIS("hidden");
#define STRNLEN_DEFINED
#endif

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/strnlen.c"
# else
#  include "generic/strnlen.c"
# endif
#else
# include "generic/strnlen.c"
#endif
