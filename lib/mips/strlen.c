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

#include "my_mips.h"

#if defined(__mips_loongson_vector_rev)

size_t strlen(const char *s)
{
	const char *p;
	uint8x8_t vt1, vt2;
	unsigned r;
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
	: /* %4 */ "r" (ALIGN_DOWN_DIFF(s, SOV8)),
	  /* %5 */ "3" (ALIGN_DOWN(s, SOV8))
	);
	return (size_t)(p - s) + nul_byte_index_loongson(r);
}

static char const rcsid_slml[] GCC_ATTR_USED_VAR = "$Id: $";

#elif defined(__mips_dsp)

size_t strlen(const char *s)
{
	const char *p = (const char *)ALIGN_DOWN(s, SOV4);
	v4i8 vt1;
	unsigned r;

	asm (
		".set noreorder\n\t"
		"cmpgu.eq.qb	%1, %4, %0\n\t"
# if HOST_IS_BIGENDIAN != 0
		"sllv	%1, %1, %3\n\t"
		"bnez	%1, 2f\n\t"
		"srlv	%1, %1, %3\n\t"
# else
		"srlv	%1, %1, %3\n\t"
		"bnez	%1, 2f\n\t"
		"sllv	%1, %1, %3\n\t"
# endif
		"1:\n\t"
		"lw	%0, %5(%2)\n\t"
		"cmpgu.eq.qb	%1, %4, %0\n\t"
		"beqz	%1, 1b\n\t"
		"addiu	%2, %2, %5\n\t"
		"2:\n\t"
		".set reorder\n\t"
	: /* %0 */ "=&r" (vt1),
	  /* %1 */ "=&r" (r),
	  /* %2 */ "=&d" (p)
	: /* %3 */ "r" (ALIGN_DOWN_DIFF(s, SOV4) * BITS_PER_CHAR),
	  /* %4 */ "r" (0),
	  /* %5 */ "i" (SOV4),
	  /* %6 */ "2" (p),
	  /* %7 */ "0" (*(const v4i8 *)p)
	);
	return (size_t)(p - s) + nul_byte_index_dsp(r);
}

static char const rcsid_slmd[] GCC_ATTR_USED_VAR = "$Id: $";
#else

size_t strlen(const char *s)
{
	const char *p;
	check_t r;
	uint32_t r1;
	unsigned shift;
	prefetch(s);

	/*
	 * MIPS has, like most RISC archs, proplems
	 * with constants. Long imm. (64 Bit) are very
	 * painfull on MIPS. So do the first short tests
	 * in 32 Bit for short strings
	 */
	p = (const char *)ALIGN_DOWN(s, SO32);
	shift = ALIGN_DOWN_DIFF(s, SO32) * BITS_PER_CHAR;
	r1 = *(const uint32_t *)p;
	if(!HOST_IS_BIGENDIAN)
		r1 >>= shift;
	r1 = has_nul_byte32(r1);
	if(HOST_IS_BIGENDIAN)
		r1 <<= shift;
	if(r1)
		return nul_byte_index_mips(r1);

	p += SO32;
# if __mips == 64 || defined(__mips64)
	if(!IS_ALIGN(p, SOCT))
	{
		r1 = *(const uint32_t *)p;
		r1 = has_nul_byte32(r1);
		r = 0;
		if(r1)
			r = r1;
		else
			p += SO32;
	} else
#endif
		r = 0;

	for(; !r; p += SOCT)
		r = has_nul_byte(*(const check_t *)p);
	return p - s + nul_byte_index_mips(r);
}

static char const rcsid_slm[] GCC_ATTR_USED_VAR = "$Id: $";
#endif
