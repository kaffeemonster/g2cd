/*
 * popcountst.c
 * calculate popcount in size_t, sparc/sparc64 implementation
 *
 * Copyright (c) 2005-2009 Jan Seiffert
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
 * $Id:$
 */

#if 0
/* ================================================================
 * do NOT use these sparc instuctions, they are glacialy slow, only
 * for reference. Generic is 50 times faster.
 * ================================================================
 */
/*
 * gcc sets __sparcv8 even if you say "gimme v9" to not confuse solaris
 * tools and signal "32 bit mode". So how to detect a real v9 to do
 * v9ish stuff, mister sun? Great tennis! This will Bomb on a real v8...
 */
# if defined(__sparcv8) || defined(__sparc_v8__) || defined(__sparcv9) || defined(__sparc_v9__)
size_t GCC_ATTR_CONST GCC_ATTR_FASTCALL popcountst(size_t n)
{
	size_t tmp;
	__asm__ ("popc\t%1, %0\n" : "=r" (tmp) : "r" (n));
	return tmp;
}

# else
#  include "../generic/popcountst.c"
# endif
static char const rcsid_pc[] GCC_ATTR_USED_VAR = "$Id:$";
#else
# include "../generic/popcountst.c"
#endif
