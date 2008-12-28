/*
 * memneg.c
 * neg a memory region efficient
 *
 * Copyright (c) 2006-2008 Jan Seiffert
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
 * $Id:$
 */

#include "my_bitops.h"
#include "my_bitopsm.h"

/*
 * memneg - neg one chunk of memory inplace or src to dest
 * src: the src, only gets read
 * dst: the dst, or src if no src specified, result gets written to
 * len: length in bytes of chunk
 *
 * return value: the pointer to dst
 *
 * the memory regions src and dst are /not/ allowed to overlap
 * if both are used.
 *
 * Note: This implementation does some overhaed to align the
 * pointer, it is optimized for large chunks, as they normaly
 * apear in an G2 QHT (128k)
 *
 *
 */
/* void *memneg(void *dst, const void *src, size_t len) */

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/memneg.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
	/* works for both */
#  include "ppc/memneg.c"
# else
#  include "generic/memneg.c"
# endif
#else
# include "generic/memneg.c"
#endif
