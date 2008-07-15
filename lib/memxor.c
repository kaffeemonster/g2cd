/*
 * memxor.c
 * xor two memory region efficient
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
 * $Id:$
 */

#include "my_bitops.h"
#include "my_bitopsm.h"

/*
 * memxor - xor two chunks of memory
 * src: the one chunk, only gets read
 * dst: the other chunk, result gets written to
 * len: length in bytes of smallest chunk
 *
 * return value: the pointer to dst
 *
 * the memory regions src and dst are /not/ allowed to overlap
 * 
 * Note: This implementation does some overhaed to align the
 * pointer, it is optimized for large chunks, as they normaly
 * apear in an G2 QHT (128k)
 *
 * 
 * Sidenote: This function was a clear candidate for the new
 * "restrict" keyword, as the original version also didn't
 * allow overlaping reagions, to speed up the xor.
 * Unfortunatly, with recent gcc's, the hand down written align
 * foo clashes with the magic the compiler is capable of. With
 * "restict" the compiler has exactly the same information we
 * have and does exactly the same magic. So evectivly it is
 * done twice.
 * So basicaly we can shrink down the funktion to:
 * 
 * inline void *memxor(void *restrict dst,
 * 	const void *restrict src, size_t len)
 * {
 * 	char *restrict dst_c = dst;
 * 	const char *restrict src_c = src;
 * 	void *old_dst = dst;
 * 	
 * 	while(len--)
 * 		*(dst_c++) ^= *(src_c++);
 * 	return old_dst;
 * }
 * 
 * and the compiler would do the Right Thing[TM], with gcc 4.x
 * maybe autovectorize it.
 * 
 * But what's with the compilers not optimizing "restrict"?
 * Introduce a magic Flag HAVE_COMPILER_OPT_RESTRICT? It's
 * difficult to test automaticaly, and if "Joe Average" would
 * set it right...
 * So no "restrict" here.
 */
/* void *memxor(void *dst, const void *src, size_t len) */

#ifdef I_LIKE_ASM
# if defined(__i386__) | defined(__x86_64__)
	/* works for both */
#  include "x86/memxor.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
	/* works for both */
#  include "ppc/memxor.c"
# else
#  include "generic/memxor.c"
# endif
#else
# include "generic/memxor.c"
#endif
