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
# if MY_MIPS_IS_64 == 1
	if(!IS_ALIGN(p, SOCT))
	{
		p += SO32;
		r1 = *(const uint32_t *)p;
		r1 = has_nul_byte32(r1);
		if(r1)
			r = r1;
		if(HOST_IS_BIGENDIAN)
			r <<= SO32 * BITS_PER_CHAR;
	}
#endif

	if(!r)
	{
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
