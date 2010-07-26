/*
 * flsst.c
 * find last set in size_t, ia64 implementation
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

#include "../my_bitopsm.h"

/*
 * those funky ia64...
 * from a latency perspective (number of ops)
 * it's faster to convert to floating point and
 * extract the exponent...
 */
#if 1
/*
 * Newer gcc can generate this code fine, but i think
 * that is the result of a recent reorg (gcc 4.4?).
 * So we fiddle to regenerate _exactly_ the same code
 * so it hopefully works with 0, as the generic one
 * should.
 */
size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL flsst(size_t find)
{
	long double d = find;
	int exp;

	__asm__ ("getf.exp %0=%1;;\n\tsub %0=%2,%0" : "=&r"(exp) : "f"(d), "r" (65598));
	return 64LL - exp;
}
#else
/*
 * Older compiler may generate some not so cool code for
 * __builtin_clzll
 */
size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL flsst(size_t find)
{
	size_t found;
	return SIZE_T_BITS - __builtin_clzll(find);
}
#endif

static char const rcsid_flia[] GCC_ATTR_USED_VAR = "$Id:$";
