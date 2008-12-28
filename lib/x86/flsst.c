/*
 * flsst.c
 * find last set in size_t, x86 implementation
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
 * $Id: $
 */

/*
 * Make sure to avoid asm operant size suffixes, this is
 * used in 32Bit & 64Bit
 */
size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL flsst(size_t find)
{
	size_t t;
	size_t found;
	__asm__ __volatile__(
		"or	$-1, %1\n\t"
		"bsr	%2, %0\n\t"
		"cmovz	%1, %0\n\t"
		"inc	%0\n"
		"1:"
		: /* %0 */ "=r" (found),
		  /* %1 */ "=&r" (t)
		: /* %2 */ "mr" (find)
		: "cc"
	);
	return found;
}

static char const rcsid_fl[] GCC_ATTR_USED_VAR = "$Id:$";
