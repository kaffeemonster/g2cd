/*
 * flsst.c
 * find last set in size_t, x86 implementation
 *
 * Copyright (c) 2004-2010 Jan Seiffert
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

size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL flsst(size_t find)
{
	size_t found;
#ifdef __x86_64__
	/*
	 * Ahhh, nice to see someone still going down on low level
	 * bit fruggle things. News from the kernel, they at least have
	 * the connections to get info like this:
	 *
	 * The AMD AMD64 handbook says BSF/BSR won't clobber the dest reg if
	 * find==0;
	 * The Intel handbooks fall back to the moderate tone inheriated from
	 * x86_32, the dest reg is undefined if find==0.
	 *
	 * But their CPU architect says its value is written to set it to the
	 * same as before (in the 32 Bit case the top bits would be cleared).
	 * And they are "unlikely to get away with changing the behaviour.",
	 * their words...
	 * The x86_64 decision is to lock out all the old 32 Bit clones, which
	 * maybe _really_ did something funny because it's undefined.
	 */
	__asm__ __volatile__(
		"or	$-1, %0\n\t"
		"bsr	%1, %0\n\t"
		"inc	%0\n"
		: /* %0 */ "=&r" (found),
		: /* %1 */ "rm" (find)
		: "cc"
	);
#else
	size_t t;
// TODO: did i really wanted to dump everything < P-II and < Via C3-2 (Nehemiah)
	/*
	 * a.k.a < i686
	 * I think so, because all that stuff is old! Earlier than 2004 (?)
	 * for Via and 1999 (when P1 prod. stopped) for the rest, which then
	 * raises the question if these machines are beefy enough in general
	 * to run G2CD.
	 * (either RAM, or MHz, or both, < P-II prop. fails in both, the Via
	 * stuff prop. at RAM, because being low-cost also often means getting
	 * low-ram (or what you get for low money), and if that is < 256 Mb,
	 * i see problems with servicing 300 connections (the official G2
	 * default, needs ~50Mb) + headroom + System)
	 */
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
#endif
	return found;
}

static char const rcsid_fl[] GCC_ATTR_USED_VAR = "$Id:$";
