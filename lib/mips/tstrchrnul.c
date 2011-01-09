/*
 * tstrchrnul.c
 * tstrchrnul, mips implementation
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
tchar_t *tstrchrnul(const tchar_t *s, tchar_t c)
{
	const char *p;
	uint8x8_t mask, vt1, vt2, vt3;
	unsigned r;
	prefetch(s);

	asm (
		".set noreorder\n\t"
		"ldc1	%0, (%5)\n\t"
		"mtc1	%8, %3\n\t"
		"xor	%2, %2, %2\n\t"
		"pshufh	%3, %3, %2\n\t"
		"pcmpeqh	%1, %0, %2\n\t"
		"pcmpeqh	%0, %0, %3\n\t"
		"or	%0, %1, %0\n\t"
		"pmovmskb	%0, %0\n\t"
		"mfc1	%4, %0\n\t"
		"srlv	%4, %4, %6\n\t"
		"bnez	%4, 2f\n\t"
		"sllv	%4, %4, %6\n\t"
		"1:\n\t"
		"ldc1	%0, %7(%5)\n\t"
		"pcmpeqh	%1, %0, %2\n\t"
		"pcmpeqh	%0, %0, %3\n\t"
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
	  /* %8 */ "r" (c),
	  /* %9 */ "5" (ALIGN_DOWN(s, SOV8))
	);
	return ((tchar_t *)(uintptr_t)p) + nul_word_index_loongson(r);
}

static char const rcsid_tscnml[] GCC_ATTR_USED_VAR = "$Id: $";
#elif defined(__mips_dsp)

tchar_t *tstrchrnul(const tchar_t *s, tchar_t c)
{
	const char *p = (const char *)ALIGN_DOWN(s, SOV4);
	v4i8 vt1;
	unsigned r;

	asm (
		".set noreorder\n\t"
		"cmp.eq.ph	%4, %0\n\t"
		"pick.ph	%1, %7, %4\n\t"
		"cmp.eq.ph	%5, %0\n\t"
		"pick.ph	%1, %7, %1\n\t"
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
		"lw	%0, %6(%2)\n\t"
		"cmp.eq.ph	%4, %0\n\t"
		"pick.ph	%1, %7, %4\n\t"
		"cmp.eq.ph	%5, %0\n\t"
		"pick.ph	%1, %7, %1\n\t"
		"beqz	%1, 1b\n\t"
		SZPRFX"addiu	%2, %2, %6\n\t"
		"2:\n\t"
		".set reorder\n\t"
	: /* %0 */ "=&r" (vt1),
	  /* %1 */ "=&r" (r),
	  /* %2 */ "=&d" (p)
	: /* %3 */ "r" (ALIGN_DOWN_DIFF(s, SOV4) * BITS_PER_CHAR),
	  /* %4 */ "r" (0),
	  /* %5 */ "r" (((uint32_t)c & 0xffff) * 0x00010001),
	  /* %6 */ "i" (SOV4),
	  /* %7 */ "r" (0x00010001),
	  /* %8 */ "2" (p),
	  /* %9 */ "0" (*(const v4i8 *)p)
	);
	return ((tchar_t *)(uintptr_t)p) + nul_word_index_dsp(r);
}

static char const rcsid_tscnmd[] GCC_ATTR_USED_VAR = "$Id: $";
#else

tchar_t *tstrchrnul(const tchar_t *s, tchar_t c)
{
	const char *p;
	check_t r;
	uint32_t r1, mask1, x1;
	unsigned shift;

	if(unlikely(!c))
		return ((tchar_t *)(uintptr_t)s) + tstrlen(s);
	prefetch(s);

	/*
	 * MIPS has, like most RISC archs, proplems
	 * with constants. Long imm. (64 Bit) are very
	 * painfull on MIPS. So do the first short tests
	 * in 32 Bit for short strings
	 */
	mask1 = (((uint32_t)c) & 0xFFFF) * 0x00010001;
	p  = (const char *)ALIGN_DOWN(s, SO32);
	shift = ALIGN_DOWN_DIFF(s, SO32) * BITS_PER_CHAR;
	x1  = *(const uint32_t *)p;
	if(!HOST_IS_BIGENDIAN)
		x1 >>= shift;
	r1  = has_nul_word(x1);
	x1 ^= mask1;
	r1 |= has_nul_word(x1);
	r1 <<= shift;
	if(HOST_IS_BIGENDIAN)
		r1 >>= shift;

# if MY_MIPS_IS_64 == 1
	if(!r1 && !IS_ALIGN(p, SOCT))
	{
		p += SO32;
		r1 = *(const uint32_t *)p;
		r1  = has_nul_word32(x1);
		x1 ^= mask1;
		r1 |= has_nul_word32(x1);
	}
# endif

	if(!r1)
	{
		check_t mask, x;
		p -= SOCT - SO32;
		mask = (((check_t)c) & 0xFFFF) * MK_CC(0x00010001);
		do
		{
			p += SOCT;
			x  = *(const check_t *)p;
			r  = has_nul_word(x);
			x ^= mask;
			r |= has_nul_word(x);
		} while(!r);
	}
	else
		r = r1;
	return ((tchar_t *)(uintptr_t)p) + nul_word_index_mips(r);
}

static char const rcsid_tscnm[] GCC_ATTR_USED_VAR = "$Id: $";
#endif
/* EOF */
