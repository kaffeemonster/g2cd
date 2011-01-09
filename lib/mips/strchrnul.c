/*
 * strchrnul.c
 * strchrnul for non-GNU platforms, mips implementation
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

#include "my_mips.h"

#if defined(__mips_loongson_vector_rev)
char *strchrnul(const char *s, int c)
{
	const char *p;
	uint8x8_t mask, vt1, vt2, vt3;
	unsigned r;
	prefetch(s);

	asm (
		".set noreorder\n\t"
		"ldc1	%0, (%5)\n\t"
		"xor	%2, %2, %2\n\t"
		"pshufh	%3, %3, %2\n\t"
		"pcmpeqb	%1, %0, %2\n\t"
		"pcmpeqb	%0, %0, %3\n\t"
		"or	%0, %1, %0\n\t"
		"pmovmskb	%0, %0\n\t"
		"mfc1	%4, %0\n\t"
		"srlv	%4, %4, %6\n\t"
		"bnez	%4, 2f\n\t"
		"sllv	%4, %4, %6\n\t"
		"1:\n\t"
		"ldc1	%0, %7(%5)\n\t"
		"pcmpeqb	%1, %0, %2\n\t"
		"pcmpeqb	%0, %0, %3\n\t"
		"or	%0, %1, %0\n\t"
		"pmovmskb	%0, %0\n\t"
		"mfc1	%4, %0\n\t"
		"beqz	%4, 1b\n\t"
		SZPRFX"addiu	%5, %5, %7\n\t"
		"2:\n\t"
		".set reorder\n\t"
	: /* %0 */ "=f" (vt1),
	  /* %1 */ "=f" (vt2),
	  /* %2 */ "=f" (vt3),
	  /* %3 */ "=f" (mask),
	  /* %4 */ "=&r" (r),
	  /* %5 */ "=&d" (p)
	: /* %6 */ "r" (ALIGN_DOWN_DIFF(s, SOV8)),
	  /* %7 */ "i" (SOV8),
	  /* %8 */ "3" ((c & 0xff) * 0x0101),
	  /* %9 */ "5" (ALIGN_DOWN(s, SOV8))
	);
	return (char *)(uintptr_t)p + nul_byte_index_loongson(r);
}

static char const rcsid_scnml[] GCC_ATTR_USED_VAR = "$Id: $";
#elif defined(__mips_dsp)

char *strchrnul(const char *s, int c)
{
	const char *p = (const char *)ALIGN_DOWN(s, SOV4);
	v4i8 vt1, vt2;
	unsigned r;

	asm (
		".set noreorder\n\t"
		"cmpgu.eq.qb	%2, %5, %0\n\t"
		"cmpgu.eq.qb	%1, %6, %0\n\t"
		"or	%2, %1, %2\n\t"
# if HOST_IS_BIGENDIAN != 0
		"sllv	%2, %2, %4\n\t"
		"bnez	%2, 2f\n\t"
		"srlv	%2, %2, %4\n\t"
# else
		"srlv	%2, %2, %4\n\t"
		"bnez	%2, 2f\n\t"
		"sllv	%2, %2, %4\n\t"
# endif
		"1:\n\t"
		"lw	%0, %7(%3)\n\t"
		"cmpgu.eq.qb	%2, %5, %0\n\t"
		"cmpgu.eq.qb	%1, %6, %0\n\t"
		"or	%2, %1, %2\n\t"
		"beqz	%2, 1b\n\t"
		SZPRFX"addiu	%3, %3, %7\n\t"
		"2:\n\t"
		".set reorder\n\t"
	: /* %0 */ "=&r" (vt1),
	  /* %1 */ "=&r" (vt2),
	  /* %2 */ "=&r" (r),
	  /* %3 */ "=&d" (p)
	: /* %4 */ "r" (ALIGN_DOWN_DIFF(s, SOV4) * BITS_PER_CHAR),
	  /* %5 */ "r" (0),
	  /* %6 */ "r" ((c & 0xff) * 0x01010101),
	  /* %7 */ "i" (SOV4),
	  /* %8 */ "3" (p),
	  /* %9 */ "0" (*(const v4i8 *)p)
	);
	return (char *)(uintptr_t)p + nul_byte_index_dsp(r);
}

static char const rcsid_scnmd[] GCC_ATTR_USED_VAR = "$Id: $";
#else

char *strchrnul(const char *s, int c)
{
	const char *p;
	check_t r;
	uint32_t r1, mask1, x1;
	unsigned shift;

	if(unlikely(!c))
		return (char *)(intptr_t)s + strlen(s);
	prefetch(s);

	/*
	 * MIPS has, like most RISC archs, proplems
	 * with constants. Long imm. (64 Bit) are very
	 * painfull on MIPS. So do the first short tests
	 * in 32 Bit for short strings
	 */
	mask1 = (c & 0xFF) * 0x01010101;
	p  = (const char *)ALIGN_DOWN(s, SO32);
	shift = ALIGN_DOWN_DIFF(s, SO32) * BITS_PER_CHAR;
	x1  = *(const uint32_t *)p;
	if(!HOST_IS_BIGENDIAN)
		x1 >>= shift;
	r1  = has_nul_byte32(x1);
	x1 ^= mask1;
	r1 |= has_nul_byte32(x1);
	r1 <<= shift;
	if(HOST_IS_BIGENDIAN)
		r1 >>= shift;

# if MY_MIPS_IS_64 == 1
	if(!r1 && !IS_ALIGN(p, SOCT))
	{
		p += SO32;
		r1 = *(const uint32_t *)p;
		r1  = has_nul_byte32(x1);
		x1 ^= mask1;
		r1 |= has_nul_byte32(x1);
	}
# endif

	if(!r1)
	{
		check_t mask, x;
		p -= SOCT - SO32;
		mask = (c & 0xFF) * MK_CC(0x01010101);
		do
		{
			p += SOCT;
			x  = *(const check_t *)p;
			r  = has_nul_byte(x);
			x ^= mask;
			r |= has_nul_byte(x);
		} while(!r);
	}
	else
		r = r1;
	return (char *)(uintptr_t)p + nul_byte_index_mips(r);
}

static char const rcsid_scnm[] GCC_ATTR_USED_VAR = "$Id: $";
#endif
