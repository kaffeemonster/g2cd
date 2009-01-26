/*
 * memxorcpy.c
 * xor two memory region efficient and cpy to dest
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
 * memxorcpy - xor two chunks of memory and cpy to dest
 * dst: the target result gets written to
 * src1: the one chunk
 * src2: the other chunk
 * len: length in bytes of smallest chunk
 *
 * return value: the pointer to dst
 *
 * The memory regions src2 and dst are /not/ allowed to overlap.
 * The memory regions src1 and dst are /not/ allowed to overlap,
 *  but maybe the same.
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
 * So basicaly we could shrink down the funktion to:
 *
 * void *memxorcpy(void *dst, const void src1
 * 	const void *restrict src2, size_t len)
 * {
 * 	char *dst_c = dst;
 * 	const char *src_c1 = src1;
 * 	const char *restrict src_c2 = src2;
 * 	void *old_dst = dst;
 *
 * 	while(len--)
 * 		*dst_c++ = *src_c1 ^ *src_c2++;
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
/* void *memxor(void *dst, const void *src, size_t len) */

#ifdef I_LIKE_ASM
# if defined(__i386__) | defined(__x86_64__)
	/* works for both */
#  include "x86/memxorcpy.c"
# elif defined(__powerpc__) || defined(__powerpc64__)
	/* works for both */
#  include "ppc/memxorcpy.c"
# else
#  include "generic/memxorcpy.c"
# endif
#else
# include "generic/memxorcpy.c"
#endif
