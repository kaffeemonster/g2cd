/*
 * popcountst.c
 * calculate popcount in size_t, RISC-V implementation
 *
 * Copyright (c) 2004-2026 Jan Seiffert
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
 * $Id:$
 */

#ifdef __riscv_zbb
size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL popcountst(size_t n)
{
	size_t result;
	__asm__ ("cpop %0, %1" : "=r"(result) : "r"(n));
	return result;
}

static char const rcsid_pcrv[] GCC_ATTR_USED_VAR = "$Id:$";
#else
# include "../generic/popcountst.c"
#endif
