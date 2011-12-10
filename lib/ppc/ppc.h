/*
 * ppc.h
 * little ppc helper
 *
 * Copyright (c) 2010-2011 Jan Seiffert
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

#ifndef PPC_H
# define PPC_H

# ifdef _ARCH_PWR6
#  undef has_nul_byte
static inline size_t has_nul_byte(size_t a)
{
	size_t res;
	asm("cmpb	%0, %1, %2" : "=r" (res) : "%rO" (a), "rO" (0));
	return res;
}

#  undef has_eq_byte
static inline size_t has_eq_byte(size_t a, size_t b)
{
	size_t res;
	asm("cmpb	%0, %1, %2" : "=r" (res) : "%rO" (a), "rO" (b));
	return res;
}

#  undef has_nul_word
static inline size_t has_nul_word(size_t a)
{
	size_t res;
	asm("cmpb	%0, %1, %2" : "=r" (res) : "%rO" (a), "rO" (0));
	return res & ((res & MK_C(0x00FF00FF)) << BITS_PER_CHAR);
}

#  undef has_eq_word
static inline size_t has_eq_word(size_t a, size_t b)
{
	size_t res;
	asm("cmpb	%0, %1, %2" : "=r" (res) : "%rO" (a), "rO" (b));
	return res & ((res & MK_C(0x00FF00FF)) << BITS_PER_CHAR);
}
# endif
#endif
