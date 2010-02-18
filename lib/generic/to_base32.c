/*
 * to_base32.c
 * convert binary string to base32, generic impl.
 *
 * Copyright (c) 2010 Jan Seiffert
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
 * $Id: $
 */

#ifndef ARCH_NAME_SUFFIX
# define F_NAME(z, x, y) z x
#else
# define F_NAME(z, x, y) static z x##y
#endif

#define TCINST (SOST / sizeof(tchar_t))

F_NAME(tchar_t *, to_base32, _generic)(tchar_t *dst, const unsigned char *src, unsigned len)
{
	static const unsigned char base32c[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567=";
	unsigned b32chars = B32_LEN(len), i = 0, ch = 0;
	int shift = 11;

#ifndef ONLY_REMAINDER
#endif
	do
	{
		*dst++ = base32c[((src[i] * 256 + src[i + 1]) >> shift) & 0x1F];
		shift  -= 5;
		if(shift <= 0) {
			shift += BITS_PER_CHAR;
			i++;
		}
	} while(++ch < b32chars - 1);
	*dst++ = base32c[(src[len - 1] << (b32chars * 5 % BITS_PER_CHAR)) & 0x1F];
	return dst;
}
#undef F_NAME

static char const rcsid_tb32g[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
