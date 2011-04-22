/*
 * mempopcnt.c
 * popcount a mem region, alpha implementation
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

#ifdef __alpha_cix__
# include "alpha.h"

# define popcountst_b(x) popcountst_int1(x)
static inline size_t popcountst_int1(size_t n)
{
	return ctpop(n);
}

static inline size_t popcountst_int2(size_t n, size_t m)
{
	return popcountst_int1(n) +
	       popcountst_int1(m);
}

static inline size_t popcountst_int4(size_t n, size_t m, size_t o, size_t p)
{
	return popcountst_int1(n) +
	       popcountst_int1(m) +
	       popcountst_int1(o) +
	       popcountst_int1(p);
}

static char const rcsid_mpa[] GCC_ATTR_USED_VAR = "$Id: $";
# define NO_GEN_POPER
# define HAVE_FULL_POPCNT
#elif __alpha_max__
/*
 * the Motion Video Extention (PCA56/57, EV6 and above) is
 * older than the Count Extention (EV67 and above)
 */
# include "alpha.h"
# define NO_SIDEWAYS_ADD
static inline size_t sideways_add(size_t sum, size_t x)
{
	return sum + perr(x, 0);
}
#endif
#include "../generic/mempopcnt.c"
