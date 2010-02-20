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

/*
 * Thanks go out to my brother for the idea how to group
 * those 5bit quantities.
 */

#ifndef ARCH_NAME_SUFFIX
# define F_NAME(z, x, y) z x
#else
# define F_NAME(z, x, y) static noinline z x##y
#endif

static tchar_t *do_40bit(tchar_t *dst, uint64_t d1)
{
	uint64_t d2;
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ >= 8
	uint64_t m1, t1, t2;

# if HOST_IS_BIGENDIAN == 0
	d1 = __swab64(d1);
# endif
	d2   = d1;                    /* copy */
	d2 >>= 12;                    /* shift copy */
	d1  &= 0xFFFFFFFF00000000ULL; /* eliminate */
	d2  &= 0x00000000FFFFFFFFULL;
	d1  |= d2;                    /* join */
	d2   = d1;                    /* copy */
	d2 >>= 6;                     /* shift copy */
	d1  &= 0xFFFF0000FFFF0000ULL; /* eliminate */
	d2  &= 0x0000FFFF0000FFFFULL;
	d1  |= d2;                    /* join */
	d2   = d1;                    /* copy */
	d2 >>= 3;                     /* shift copy */
	d1  &= 0xFF00FF00FF00FF00ULL; /* eliminate */
	d2  &= 0x00FF00FF00FF00FFULL;
	d1  |= d2;                    /* join */
	d1 >>= 3;                     /* bring it down */
	d1  &= 0x1F1F1F1F1F1F1F1FULL; /* eliminate */

	/* convert */
	d1  += 0x4141414141414141ULL;
	m1   = has_greater(d1, 0x5A);
	m1 >>= 7;
	m1   = 0x29 * m1;
	d1  -= m1;
	/* write out */
	t1     =  d1 & 0x00FF00FF00FF00FFULL;
	t2     = (d1 & 0xFF00FF00FF00FF00ULL) >> 8;
	dst[7] = (t1 & 0x0000ffff);
	dst[6] = (t2 & 0x0000ffff);
	t1 >>= 16; t2 >>= 16;
	dst[5] = (t1 & 0x0000ffff);
	dst[4] = (t2 & 0x0000ffff);
	t1 >>= 16; t2 >>= 16;
	dst[3] = (t1 & 0x0000ffff);
	dst[2] = (t2 & 0x0000ffff);
	t1 >>= 16; t2 >>= 16;
	dst[1] = (t1 & 0x0000ffff);
	dst[0] = (t2 & 0x0000ffff);
#else
	uint32_t a1, a2;
	uint32_t b1, b2;
	uint32_t m1, m2, t1, t2;

# if HOST_IS_BIGENDIAN == 0
	d1 = __swab64(d1);
# endif
	d2 = d1;                                   /* copy */
	d2 >>= 12;                                 /* shift copy */
	a1   = (d1 & 0xFFFFFFFF00000000ULL) >> 32; /* split it */
	a2   =  d2 & 0x00000000FFFFFFFFULL;
	b1   = a1;           b2   = a2;            /* copy */
	b1 >>= 6;            b2 >>= 6;             /* shift copy */
	b1  &= 0x0000FFFFUL; a1  &= 0xFFFF0000UL;  /* eliminate */
	b2  &= 0x0000FFFFUL; a2  &= 0xFFFF0000UL;
	a1  |= b1;           a2  |= b2;            /* join */
	b1   = a1;           b2   = a2;            /* copy */
	b1 >>= 3;            b2 >>= 3;             /* shift copy */
	b1  &= 0x00FF00FFUL; a1  &= 0xFF00FF00UL;  /* eliminate */
	b2  &= 0x00FF00FFUL; a2  &= 0xFF00FF00UL;
	a1  |= b1;           a2  |= b2;            /* join */
	a1 >>= 3;            a2 >>= 3;             /* bring bits down */
	a1  &= 0x1F1F1F1FUL; a2  &= 0x1F1F1F1FUL;  /* eliminate */

	/* convert */
	a1  += 0x41414141UL;          a2  += 0x41414141UL;
	m1   = has_greater(a1, 0x5A); m2   = has_greater(a2, 0x5A);
	m1 >>= 7;                     m2 >>= 7;
	m1   = 0x29 * m1;             m2   = 0x29 * m2;
	a1  -= m1;                    a2  -= m2;
	/* write out */
	t1     =  a2 & 0x00FF00FFUL;
	t2     = (a2 & 0xFF00FF00UL) >> 8;
	dst[7] = (t1 & 0x0000ffff);
	dst[6] = (t2 & 0x0000ffff);
	t1 >>= 16; t2 >>= 16;
	dst[5] = (t1 & 0x0000ffff);
	dst[4] = (t2 & 0x0000ffff);
	t1     =  a1 & 0x00FF00FFUL;
	t2     = (a1 & 0xFF00FF00UL) >> 8;
	dst[3] = (t1 & 0x0000ffff);
	dst[2] = (t2 & 0x0000ffff);
	t1 >>= 16; t2 >>= 16;
	dst[1] = (t1 & 0x0000ffff);
	dst[0] = (t2 & 0x0000ffff);
#endif
	return dst + 8;
}

F_NAME(tchar_t *, to_base32, _generic)(tchar_t *dst, const unsigned char *src, unsigned len)
{
#ifndef ONLY_REMAINDER
	while(len >= sizeof(uint64_t))
	{
		uint64_t d = get_unaligned((const uint64_t *)src);
		src += 5;
		len -= 5;
		dst = do_40bit(dst, d);
	}
#endif
	while(len >= 5)
	{
#if HOST_IS_BIGENDIAN == 0
		uint64_t d =  ((uint64_t)get_unaligned((const uint32_t *)src)) |
		             (((uint64_t)src[4]) << 32);
#else
		uint64_t d = ((uint64_t)(get_unaligned((const uint32_t *)src)) << 32) |
		             ((uint64_t)(src[4]) << 24);
#endif
		src += 5;
		len -= 5;
		dst = do_40bit(dst, d);
	}
	/* less than 32 bit left */
	if(len)
	{
		static const unsigned char base32c[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567=";
		unsigned b32chars = B32_LEN(len);
		uint32_t d;
		unsigned i;

		/* collect the bytes */
		for(i = len, d = 0; i; i--)
			d = (d << 8) | *src++;

		/* bring up */
		d <<= (sizeof(d) - len) * BITS_PER_CHAR;
		i = 0;
		/* write out */
		do
		{
			*dst++ = base32c[(d >> (32 - 5)) & 0x1F];
			d <<= 5;
		} while(++i < b32chars);
	}
	return dst;
}
#undef F_NAME

static char const rcsid_tb32g[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
