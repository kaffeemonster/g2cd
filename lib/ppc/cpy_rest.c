/*
 * cpy_rest.c
 * copy a byte trailer, ppc impl.
 *
 * Copyright (c) 2009-2011 Jan Seiffert
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

#if 1 == HOST_IS_BIGENDIAN
// TODO: keep the wrong processors out
/*
 * If in little endian mode, this traps (execption ppc740 & ppc750) or does
 * nothing, or in ISA parlance it is "not supported for the server enviroment
 * in little endian mode".
 * Normaly this is also only avail on POWER, not PowerPC, or the other way
 * rounds because these instructions are "category legacy move assist".
 *
 * Unfortunatly they are _exactly_ what we need!
 * (and that in a RISC processor...)
 */
# if _GNUC_PREREQ(4, 1)
#  define PPC_MEM_CONSTRAIN	"Z"
# else
#  define PPC_MEM_CONSTRAIN	"V"
# endif
char GCC_ATTR_FASTCALL *cpy_rest(char *dst, const char *src, unsigned i)
{
// TODO: r4 is used on Darwin?
	register size_t a asm ("r4"); /* inline asm syntax gone wrong... */
	register size_t b asm ("r5"); /* we can not allocate these on the inline asm, */
	register size_t c asm ("r6"); /* r5 is an alternative of r (every register) */
	register size_t d asm ("r7"); /* and positional arg 5, not ppc gen.purp.reg r5 */

	/*
	 * these instructions work as advertised, if gcc is not
	 * to confused to put stuff in the wrong registers...
	 */

	asm volatile ("mtxer %0": : "r" (i));
	asm(
		"lswx	%0, %y4"
	: /* %0 */ "=r" (a),
	  /* %1 */ "=r" (b),
	  /* %2 */ "=r" (c),
	  /* %3 */ "=r" (d)
	: /* %4 */ PPC_MEM_CONSTRAIN (*src));
	asm volatile(
		"stswx	%1, %y0"
	: /* %0 */ "=" PPC_MEM_CONSTRAIN (*dst)
	: /* %1 */ "r" (a),
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

static char const rcsid_cprpp[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/cpy_rest.c"
#endif
/* EOF */
