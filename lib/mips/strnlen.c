/*
 * strnlen.c
 * strnlen for non-GNU platforms, mips implementation
 *
 * Copyright (c) 2011 Jan Seiffert
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

size_t strnlen(const char *s, size_t maxlen)
{
	const char *p;
	ssize_t f, k, t;
	uint8x8_t vt1, vt2;
	unsigned r;
	prefetch(s);

	f = ALIGN_DOWN_DIFF(s, SOV8);
	k = SOV8 - f - (ssize_t)maxlen;
	k = k > 0 ? k  : 0;

	asm (
		".set noreorder\n\t"
		"ldc1	%0, (%2)\n\t"
		"xor	%7, %7, %7\n\t"
		"pcmpeqb	%0, %0, %7\n\t"
		"pmovmskb	%0, %0\n\t"
		"mfc1	%1, %0\n\t"
		"addu	%5, %4, %3\n\t"
		"addiu	%5, %5, %9\n\t"
		"srlv	%1, %1, %4\n\t"
		"sllv	%1, %1, %5\n\t"
		"addiu	%5, %3, %9\n\t"
		"bnez	%1, 2f\n\t"
		"srlv	%1, %1, %5\n\t"
		"bnez	%3, 2f\n\t"
		"nop\n\t"
		SZPRFX"addiu	%6, %6, -%8\n"
		"1:\n\t"
		"ldc1	%0, %8(%2)\n\t"
		"pcmpeqb	%0, %0, %7\n\t"
		"pmovmskb	%0, %0\n\t"
		"mfc1	%1, %0\n\t"
		"sltiu	%5, %6, %8+1\n\t"
		"bgtz	%5, 3f\n\t"
		SZPRFX"addiu	%2, %2, %8\n"
		"beqz	%1, 1b\n\t"
		SZPRFX"addiu	%6, %6, -%8\n"
		"3:\n\t"
		SZPRFX"addiu	%3, %6, -%8\n\t"
		"slti	%5, %3, 0\n\t"
		"movz	%3, %5, %5\n\t"
		SZPRFX"negu	%3, %3\n\t"
		"addiu	%3, %3, %9\n\t"
		"sllv	%1, %1, %3\n\t"
		"srlv	%1, %1, %3\n"
		"2:\n\t"
		".set reorder\n\t"
	: /* %0  */ "=f" (vt1),
	  /* %1  */ "=r" (r),
	  /* %2  */ "=d" (p),
	  /* %3  */ "=r" (k),
	  /* %4  */ "=r" (f),
	  /* %5  */ "=r" (t),
	  /* %6  */ "=r" (maxlen),
	  /* %7  */ "=f" (vt2)
	: /* %8  */ "i" (SOV8),
	  /* %9  */ "i" ((sizeof(r) * BITS_PER_CHAR) - SOV8),
	  /* %10 */ "2" (ALIGN_DOWN(s, SOV8)),
	  /* %11 */ "3" (k),
	  /* %12 */ "4" (f),
	  /* %13 */ "6" (maxlen + f)
	);

	if(likely(r))
		r = nul_byte_index_loongson(r);
	else
		r = maxlen;
	return p - s + r;
}
static char const rcsid_snlml[] GCC_ATTR_USED_VAR = "$Id: $";

#elif defined(__mips_dsp)

size_t strnlen(const char *s, size_t maxlen)
{
	const char *p = (const char *)ALIGN_DOWN(s, SOV4);
	ssize_t f, k, t;
	v4i8 vt1;
	unsigned r;

	f = ALIGN_DOWN_DIFF(s, SOV4);
	k = SOV4 - f - (ssize_t)maxlen;
	k = k > 0 ? k  : 0;

	asm (
		".set noreorder\n\t"
		"xor	%5, %5, %5\n\t"
		"cmpgu.eq.qb	%1, %5, %0\n\t"
		"addu	%5, %4, %3\n\t"
# if HOST_IS_BIGENDIAN != 0
		"sllv	%1, %1, %4\n\t"
		"srlv	%1, %1, %5\n\t"
		"bnez	%1, 2f\n\t"
		"sllv	%1, %1, %3\n\t"
# else
		"srlv	%1, %1, %4\n\t"
		"sllv	%1, %1, %5\n\t"
		"bnez	%1, 2f\n\t"
		"srlv	%1, %1, %3\n\t"
# endif
		"bnez	%3, 2f\n\t"
		"xor	%5, %5, %5\n\t"
		SZPRFX"addiu	%6, %6, -%7\n"
		"1:\n\t"
		"lw	%0, %7(%2)\n\t"
		"cmpgu.eq.qb	%1, %5, %0\n\t"
		"sltiu	%5, %6, %7+1\n\t"
		"bgtz	%5, 3f\n\t"
		SZPRFX"addiu	%2, %2, %7\n"
		"beqz	%1, 1b\n\t"
		SZPRFX"addiu	%6, %6, -%7\n"
		"3:\n\t"
		SZPRFX"addiu	%3, %6, -%7\n\t"
		"slti	%5, %3, 0\n\t"
		"movz	%3, %5, %5\n\t"
		SZPRFX"negu	%3, %3\n\t"
		"sll	%3, %3, 3\n\t"
# if HOST_IS_BIGENDIAN != 0
		"srlv	%1, %1, %3\n\t"
		"sllv	%1, %1, %3\n"
# else
		"sllv	%1, %1, %3\n\t"
		"srlv	%1, %1, %3\n"
# endif
		"2:\n\t"
		".set reorder\n\t"
	: /* %0  */ "=r" (vt1),
	  /* %1  */ "=r" (r),
	  /* %2  */ "=d" (p),
	  /* %3  */ "=r" (k),
	  /* %4  */ "=r" (f),
	  /* %5  */ "=r" (t),
	  /* %6  */ "=r" (maxlen)
	: /* %7  */ "i" (SOV4),
	  /* %8  */ "2" (p),
	  /* %9  */ "3" (k * BITS_PER_CHAR),
	  /* %10 */ "4" (f * BITS_PER_CHAR),
	  /* %11 */ "6" (maxlen + f),
	  /* %12 */ "0" (*(const v4i8 *)p)
	);

	if(likely(r))
		r = nul_byte_index_dsp(r);
	else
		r = maxlen;
	return p - s + r;
}
static char const rcsid_snlmd[] GCC_ATTR_USED_VAR = "$Id: $";

#else

size_t strnlen(const char *s, size_t maxlen)
{
	const char *p;
	check_t r;
	uint32_t r1;
	ssize_t f, k;
	prefetch(s);

	/*
	 * MIPS has, like most RISC archs, proplems
	 * with constants. Long imm. (64 Bit) are very
	 * painfull on MIPS. So do the first short tests
	 * in 32 Bit for short strings
	 */
	f = ALIGN_DOWN_DIFF(s, SO32);
	k = SO32 - f - (ssize_t)maxlen;
	k = k > 0 ? k  : 0;

	p = (const char *)ALIGN_DOWN(s, SO32);
	r1 = *(const uint32_t *)p;
	if(!HOST_IS_BIGENDIAN)
		r1 >>= f * BITS_PER_CHAR;
	r1 = has_nul_byte32(r1);
	if(!HOST_IS_BIGENDIAN) {
		r1 <<= (k + f) * BITS_PER_CHAR;
		r1 >>= (k + f) * BITS_PER_CHAR;
	} else {
		r1 >>=  k      * BITS_PER_CHAR;
		r1 <<= (k + f) * BITS_PER_CHAR;
	}
	if(r1)
		return nul_byte_index_mips(r1);
	else if(k)
		return maxlen;

	maxlen -= SO32 - f;
	r = 0;
	if(unlikely(!maxlen))
		return p + SO32 - s;
# if MY_MIPS_IS_64 == 1
	if(!IS_ALIGN(p, SOCT))
	{
		p += SO32;
		r1 = *(const uint32_t *)p;
		r1 = has_nul_byte32(r1);
		if(r1)
			r = r1;
		else
			maxlen -= SO32;
		if(HOST_IS_BIGENDIAN)
			r <<= SO32 * BITS_PER_CHAR;
	}
# endif

	if(!r)
	{
		if(unlikely(!maxlen))
			return p + SO32 - s;
		p -= SOCT - SO32;
		do
		{
			p += SOCT;
			r = has_nul_byte(*(const check_t *)p);
			if(maxlen <= SOCT)
				break;
			maxlen -= SOCT;
		} while(!r);
	}

	k = SOCT - (ssize_t)maxlen;
	k = k > 0 ? k : 0;
	if(!HOST_IS_BIGENDIAN) {
		r <<= k * BITS_PER_CHAR;
		r >>= k * BITS_PER_CHAR;
	} else {
		r >>= k * BITS_PER_CHAR;
		r <<= k * BITS_PER_CHAR;
	}
	if(likely(r))
		r = nul_byte_index_mips(r);
	else
		r = maxlen;
	return p - s + r;
}

static char const rcsid_snlm[] GCC_ATTR_USED_VAR = "$Id: $";
#endif
