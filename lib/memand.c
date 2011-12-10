/*
 * memand.c
 * and two memory region efficient
 *
 * Copyright (c) 2006-2011 Jan Seiffert
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
 * $Id:$
 */

#include "my_bitops.h"
#include "my_bitopsm.h"

/*
 * memand - and two chunks of memory
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
 * allow overlaping reagions, to speed up the and.
 * Unfortunatly, with recent gcc's, the hand down written align
 * foo clashes with the magic the compiler is capable of. With
 * "restict" the compiler has exactly the same information we
 * have and does exactly the same magic. So evectivly it is
 * done twice.
 * So basicaly we could shrink down the funktion to:
 *
 * void *memand(void *restrict dst,
 * 	const void *restrict src, size_t len)
 * {
 * 	char *restrict dst_c = dst;
 * 	const char *restrict src_c = src;
 * 	void *old_dst = dst;
 *
 * 	while(len--)
 * 		*(dst_c++) &= *(src_c++);
 * 	return old_dst;
 * }
 *
 * and the compiler would do the Right Thing[TM], with gcc 4.x
 * autovectorize it.
 *
 * But what's with the compilers not optimizing "restrict"?
 * Introduce a magic Flag HAVE_COMPILER_OPT_RESTRICT? It's
 * difficult to test automaticaly, and if "Joe Average" would
 * set it right...
 *
 * So no "restrict" here.
 *
 * It still clashes badly on high optimization since the
 * compiler also wants to show how good he is. He bloats
 * up all those small "handle some bytes"-loops to fully
 * vectorized versions (with alignment handling etc.).
 * But it results mostly in spaceloss, not bad performance.
 * Maybe some future gcc version will use the info that
 * len %= 4 means he only has to make a loop for 4 bytes.
 */
/* void *memand(void *dst, const void *src, size_t len) */

#ifdef I_LIKE_ASM
# if defined(__i386__) || defined(__x86_64__)
	/* works for both */
#  include "x86/memand.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
	/* works for both */
#  include "ppc/memand.c"
# elif defined(__sparc) || defined(__sparc__)
	/* works for both */
#  include "sparc/memand.c"
# elif defined(__arm__)
#  include "arm/memand.c"
# elif defined(__tile__)
#  include "tile/memand.c"
# else
#  include "generic/memand.c"
# endif
#else
# include "generic/memand.c"
#endif
