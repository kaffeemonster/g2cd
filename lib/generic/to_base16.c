/*
 * to_base16.c
 * convert binary string to hex, generic impl.
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

#define CINST (SOST)

F_NAME(unsigned char *, to_base16, _generic)(unsigned char *dst, const unsigned char *src, unsigned len)
{
	static const unsigned char base16c[] = "0123456789abcdef";
	unsigned i;

#ifndef ONLY_REMAINDER
	static const unsigned long vals[] =
	{
		MK_C(0x0f0f0f0fUL),
		MK_C(0x30303030UL),
	};

	for(; len >= SOST; len -= SOST, src += SOST)
	{
		size_t in_l, in_h, m_l, m_h;
		size_t t1, t2;

		in_l = get_unaligned((const size_t *)src);

		in_h  = (in_l & (~vals[0])) >> 4;
		in_l &= vals[0];
		in_h += vals[1];
		in_l += vals[1];
		m_h   = has_greater(in_h, 0x39);
		m_l   = has_greater(in_l, 0x39);
		m_h >>= 7;
		m_l >>= 7;
		in_h += 0x27 * m_h;
		in_l += 0x27 * m_l;
		if(!HOST_IS_BIGENDIAN)
		{
			t1 = in_h;
			t2 = in_l;
			for(i = 0; i < CINST; i++, dst += 2)
			{
				dst[0] = (t1 & 0x000000ff);
				dst[1] = (t2 & 0x000000ff);
				t1 >>= BITS_PER_CHAR; t2 >>= BITS_PER_CHAR;
			}
		}
		else
		{
			t1 = in_h;
			t2 = in_l;
			for(i = CINST; i; i--, dst += 2)
			{
				dst[(i * 2) - 2] = (t1 & 0x000000ff);
				dst[(i * 2) - 1] = (t2 & 0x000000ff);
				t1 >>= BITS_PER_CHAR; t2 >>= BITS_PER_CHAR;
			}
		}
	}
#endif

	for(i = 0; i < len; i++) {
		*dst++ = base16c[src[i] / 16];
		*dst++ = base16c[src[i] % 16];
	}
	return dst;
}
#undef F_NAME

static char const rcsid_tb16g[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
