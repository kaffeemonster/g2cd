/*
 * strlen.c
 * strlen, mips implementation
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

#if defined(__mips_loongson_vector_rev)

# include <loongson.h>
// #include "my_mips.h"
size_t strlen(const char *s)
{
	const char *p;
	uint8x8_t vt1, vt2;
	unsigned r, x;
	prefetch(s);

	asm (
		".set noreorder\n\t"
		"ldc1	%0, (%3)\n\t"
		"xor	%1, %1, %1\n\t"
		"pcmpeqb	%0, %0, %1\n\t"
		"pmovmskb	%0, %0\n\t"
		"mfc1	%2, %0\n\t"
		"srlv	%2, %2, %4\n\t"
		"bnez	%2, 2f\n\t"
		"sllv	%2, %2, %4\n\t"
		"1:\n\t"
		"ldc1	%0, 8(%3)\n\t"
		"pcmpeqb	%0, %0, %1\n\t"
		"pmovmskb	%0, %0\n\t"
		"mfc1	%2, %0\n\t"
		"beqz	%2, 1b\n\t"
		"addiu	%3, %3, 8\n\t"
		"2:\n\t"
		".set reorder\n\t"
	: /* %0 */ "=f" (vt1),
	  /* %1 */ "=f" (vt2),
	  /* %2 */ "=&r" (r),
	  /* %3 */ "=&d" (p)
	: /* %4 */ "r" (ALIGN_DOWN_DIFF(s, 8)),
	  /* %5 */ "3" (ALIGN_DOWN(s, 8))
	);
	x = (size_t)(p - s);
	if(r & 0x01)
		return x;
	if(r & 0x02)
		return x + 1;
	if(r & 0x04)
		return x + 2;
	if(r & 0x08)
		return x + 3;
	if(r & 0x10)
		return x + 4;
	if(r & 0x20)
		return x + 5;
	if(r & 0x40)
		return x + 6;
	return x + 7;
}

static char const rcsid_slml[] GCC_ATTR_USED_VAR = "$Id: $";
#else
# include "../generic/strlen.c"
#endif
