/*
 * strlen.c
 * strlen, ia64 implementation
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

#include "ia64.h"

size_t strlen(const char *s)
{
	const char *p;
	unsigned long long r, t, u;
	unsigned shift;
	prefetch(s);

	/*
	 * Sometimes you need a new perspective, like the altivec
	 * way of handling things.
	 * Lower address bits? Totaly overestimated.
	 *
	 * We don't precheck for alignment.
	 * Instead we "align hard", do one load "under the address",
	 * mask the excess info out and afterwards we are fine to go.
	 */
	p = (const char *)ALIGN_DOWN(s, SOULL);
	shift = ALIGN_DOWN_DIFF(s, SOULL);
	r = *(const unsigned long long *)p;
	if(!HOST_IS_BIGENDIAN)
		r |= (~0ULL) >> ((SOULL - shift) * BITS_PER_CHAR);
	else
		r |= (~0ULL) << ((SOULL - shift) * BITS_PER_CHAR);
	r = czx1(r);
	if(r < SOULL)
		return r;

	/*
	 * All this ILP stuff is a pain in the ass.
	 *
	 * The compiler generates foo code, esp. with
	 * small inline asm in the mix, which nullifies
	 * my most important tool (let the compiler do
	 * the lifting, only give a little jolt into
	 * the right direction).
	 *
	 * It can be great what gets generated, because
	 * the compiler is busy as a bee unrolling,
	 * scheduling and whatnot. On the other hand the
	 * compiler has to asumme the worst when he sees
	 * an inline asm and to top it off, since GCC is
	 * not the greatest ILP compiler, left alone he
	 * still does not drive the arch to its full
	 * capabilities, which means human interception
	 * is needed, which means inline asm, which means...
	 *
	 * Say no to ILP, it's a pipe dream as so many
	 * other "super performant future" stuff.
	 */
	asm(
			"ld8	%1 = [%3], 8;;\n"
			"1:\n\t"
			"ld8.s	%2 = [%3], 8\n\t"
#if HOST_IS_BIGENDIAN == 0
			"czx1.r	%0 = %1;;\n\t"
#else
			"czx1.l	%0 = %1;;\n\t"
#endif
			"cmp.ne	p6, p0 = 8, %0\n\t"
			"(p6)	br.cond.spnt 2f\n\t"
			"chk.s	%2, 3f\n\t"
			"mov	%1 = %2\n\t"
			"br.cond.dptk	1b\n"
			"3:\n\t"
			"adds	%3 = -8, %3;;\n\t"
			"ld8	%2 = [%3], 8;;\n\t" /* speculative load failed, die */
			"mov	%1 = %2\n\t"
			"br.cond.sptk	1b\n"
			"2:\n\t"
		: /* %0 */ "=r" (r),
		  /* %1 */ "=r" (t),
		  /* %2 */ "=r" (u),
		  /* %3 */ "=r" (p)
		: /* %4 */ "2" (p)
		: "p6"
	);
	return (p - 16) - s + r;
}

static char const rcsid_slia64[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
