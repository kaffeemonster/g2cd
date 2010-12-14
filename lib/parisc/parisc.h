/*
 * parisc.h
 * special parisc instructions
 *
 * Copyright (c) 2010 Jan Seiffert
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

#ifndef PARISC_PARISC_H
# define PARISC_PARISC_H

# define SOUL (sizeof(unsigned long))
# define SOULM1 (SOUL-1)

# ifdef _PA_RISC2_0
#  define PREFETCH(x) "ldw " x ", %%r0\n\t"
# else
#  define PREFETCH(x) "nop\n\t"
# endif

# ifdef  __hppa64__
#  define PA_LD "ldd"
#  define PA_SBZ "*sbz"
#  define PA_SHZ "*shz"
#  define PA_NBZ "*nbz"
#  define PA_NHZ "*nhz"
#  define PA_TZ "8"
# else
#  define PA_LD "ldw"
#  define PA_SBZ "sbz"
#  define PA_SHZ "shz"
#  define PA_NBZ "nbz"
#  define PA_NHZ "nhz"
#  define PA_TZ "4"
# endif

static inline int pa_find_z(unsigned long x)
{
	int r = -1;
	unsigned long t;

	if(HOST_IS_BIGENDIAN)
	{
		x = (x << BITS_PER_CHAR) | (x >> (SOULM1 * BITS_PER_CHAR));
		do {
			t = x & 0xff;
			r++;
			x = (x << BITS_PER_CHAR) | (x >> (SOULM1 * BITS_PER_CHAR));
		} while(t);
	}
	else
	{
		do {
			t = x & 0xff;
			r++;
			x >>= BITS_PER_CHAR;
		} while(t);
	}
	return r;
}

static inline int pa_find_z_last(unsigned long x)
{
	int r = -1;
	unsigned long t;

	if(HOST_IS_BIGENDIAN)
	{
		do {
			t = x & 0xff;
			r++;
			x >>= BITS_PER_CHAR;
		} while(t);
	}
	else
	{
		x = (x << BITS_PER_CHAR) | (x >> (SOULM1 * BITS_PER_CHAR));
		do {
			t = x & 0xff;
			r++;
			x = (x << BITS_PER_CHAR) | (x >> (SOULM1 * BITS_PER_CHAR));
		} while(t);
	}
	return r;
}

static inline int pa_find_zw(unsigned long x)
{
	int r = -1;
	unsigned long t;

	if(HOST_IS_BIGENDIAN)
	{
		x = (x << BITS_PER_TCHAR) | (x >> ((SOUL-SOTC) * BITS_PER_CHAR));
		do {
			t = x & 0xffff;
			r++;
			x = (x << BITS_PER_TCHAR) | (x >> ((SOUL-SOTC) * BITS_PER_CHAR));
		} while(t);
	}
	else
	{
		do {
			t = x & 0xffff;
			r++;
			x >>= BITS_PER_TCHAR;
		} while(t);
	}
	return r;
}

static inline int pa_find_zw_last(unsigned long x)
{
	int r = -1;
	unsigned long t;

	if(HOST_IS_BIGENDIAN)
	{
		do {
			t = x & 0xffff;
			r++;
			x >>= BITS_PER_TCHAR;
		} while(t);
	}
	else
	{
		x = (x << BITS_PER_TCHAR) | (x >> ((SOUL-SOTC) * BITS_PER_CHAR));
		do {
			t = x & 0xffff;
			r++;
			x = (x << BITS_PER_TCHAR) | (x >> ((SOUL-SOTC) * BITS_PER_CHAR));
		} while(t);
	}
	return r;
}

static inline int pa_is_z(unsigned long x)
{
# if _GNUC_PREREQ (4,5)
	asm goto (
		"uxor,"PA_NBZ"	%0, %%r0, %%r0\n\t"
		"b,n	%l[some_z]\n\t"
		: /* we can not have outputs */
		: /* %0 */ "r" (x)
		: /* clobber */ "cc"
		: some_z
	);
	return 0;
some_z:
	return -1;
# else
	int r;
	asm(
		"uxor,"PA_SBZ"	%1, %%r0, %%r0\n\t"
		"b,n	1f\n\t"
		"ldi	-1, %0\n"
		"1:"
		: /* %0 */ "=r" (r)
		: /* %1 */ "r" (x),
		  /* %2 */ "0" (0)
	);
	return r;
# endif
}

static inline int pa_is_zw(unsigned long x)
{
# if _GNUC_PREREQ (4,5)
	asm goto (
		"uxor,"PA_NHZ"	%0, %%r0, %%r0\n\t"
		"b,n	%l[some_z]\n\t"
		: /* we can not have outputs */
		: /* %0 */ "r" (x)
		: /* clobber */ "cc"
		: some_z
	);
	return 0;
some_z:
	return -1;
# else
	int r;
	asm(
		"uxor,"PA_SHZ"	%1, %%r0, %%r0\n\t"
		"b,n	1f\n\t"
		"ldi	-1, %0\n"
		"1:"
		: /* %0 */ "=r" (r)
		: /* %1 */ "r" (x),
		  /* %2 */ "0" (0)
	);
	return r;
# endif
}

static inline unsigned long pcmp1gt(unsigned long a, unsigned long b)
{
	unsigned long r;
	asm(
		"uaddcm	%1, %2, %%r0\n\t"
		"dcor,i	%%r0, %0\n\t"
		: /* %0 */ "=&r" (r)
		: /* %1 */ "r" (a),
		  /* %2 */ "r" (b)
	);
	/*
	 * Sigh, parisc has 8/16 nibble carrys, for BCD arith.
	 * And all this uxor,sbz works with these "excess" carry.
	 * The problem is to "get" them, for a mask, since
	 * there are no real "use these carrys" instructions.
	 * Ecxept: dcor. Which is to correct BCD arith.
	 * dcor gave us a 6 in every nibble which overflowed,
	 * now we have to create a byte wise carry...
	 * Lets only take the upper carry. If there would be
	 * a wiring diagram how "some byte carry" works.
	 */
	r = (r >> 5) & MK_C(0x01010101UL);
	return r;
}

#endif
