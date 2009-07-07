/*
 * cpy_rest.c
 * copy a byte trailer, ppc64 impl.
 *
 * Copyright (c) 2009 Jan Seiffert
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
 * cpy_rest - copy a low byte count from src to dst
 * dst: where to copy to
 * src: where to read from
 * i: how much bytes to copy
 *
 * return value: dst + i
 *
 * NOTE: handles at most 15 bytes!!
 *
 * This function is a little bit wired. On most copy routines
 * (memcpy, strcpy) at some point a trailer emerges. You know
 * the length, but it is odd and does not fit into the main
 * copy loop. Before repeating the trailer handling over and
 * over and over, put it together.
 */
/*
 * cpy_rest0 - copy a low byte count from src to dst and zero term
 * dst: where to copy to
 * src: where to read from
 * i: how much bytes to copy
 *
 * return value: dst + i
 *
 * NOTE: handles at most 15 bytes!!
 */

#if defined(__powerpc64__) && 1 == HOST_IS_BIGENDIAN
// TODO: keep the wrong processors out
/*
 * since we need to write to the high bits in xer,
 * i think this is only available in 64 bit mode.
 * If in little endian mode, this traps (execption ppc740 & ppc750)
 * Normaly this is also only avail on POWER, not PowerPC
 */
# if _GNUC_PREREQ(4, 1)
#  define PPC_MEM_CONSTRAIN	"Z"
# else
#  define PPC_MEM_CONSTRAIN	"V"
# endif
char GCC_ATTR_FASTCALL *cpy_rest(char *dst, const char *src, unsigned i)
{
	register size_t a asm ("r5"); /* inline asm syntax gone wrong... */
	register size_t b asm ("r6"); /* we can not allocate these on the inline asm, */
	register size_t c asm ("r7"); /* r5 is an alternative of r (every register) */
	register size_t d asm ("r8"); /* and positional arg 5, not ppc gen.purp.reg r5 */

	asm volatile ("mtxer %0": : "r" (((size_t)i) << 57));
	asm(
		"lswx	%0, %y4"
	: /* %0 */ "=r" (a),
	  /* %1 */ "=r" (b),
	  /* %2 */ "=r" (c),
	  /* %3 */ "=r" (d)
	: /* %4 */ PPC_MEM_CONSTRAIN (*src));
	asm volatile(
		"stswx	%1, %y0"
	:
	: /* %0 */ PPC_MEM_CONSTRAIN (*dst),
	  /* %1 */ "r" (a),
	  /* %2 */ "r" (b),
	  /* %3 */ "r" (c),
	  /* %4 */ "r" (d));
	return dst + i;
}

char GCC_ATTR_FASTCALL *cpy_rest_o(char *dst, const char *src, unsigned i)
{
	cpy_rest(dst, src, i);
	return dst;
}

char GCC_ATTR_FASTCALL *cpy_rest0(char *dst, const char *src, unsigned i)
{
	dst[i] = '\0';
	return cpy_rest(dst, src, i);
}

#else
# include "../generic/cpy_rest.c"
#endif
