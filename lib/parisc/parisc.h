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

static inline int pa_find_zw(unsigned long x)
{
	int r = -1;
	unsigned long t;

	if(HOST_IS_BIGENDIAN)
	{
		x = (x << BITS_PER_TCHAR) | (x >> ((SOUL-SOTC) * BITS_PER_CHAR));
		do {
			t = x & 0xff;
			r++;
			x = (x << BITS_PER_TCHAR) | (x >> ((SOUL-SOTC) * BITS_PER_CHAR));
		} while(t);
	}
	else
	{
		do {
			t = x & 0xff;
			r++;
			x >>= BITS_PER_TCHAR;
		} while(t);
	}
	return r;
}

static inline int pa_is_z(unsigned long x)
{
	int r;
	asm(
		"uxor,"PA_SBZ"	%1, %%r0, %%r0\n\t"
		"b,n	1f\n\t"
		"ldi	-1, %0\n"
		"1:"
		: "=r" (r)
		: "r" (x),
		  "0" (0)
	);
// TODO: jump label support?
	return r;
}

static inline int pa_is_zw(unsigned long x)
{
	int r;
	asm(
		"uxor,"PA_SHZ"	%1, %%r0, %%r0\n\t"
		"b,n	1f\n\t"
		"ldi	-1, %0\n"
		"1:"
		: "=r" (r)
		: "r" (x),
		  "0" (0)
	);
// TODO: jump label support?
	return r;
}

#endif
