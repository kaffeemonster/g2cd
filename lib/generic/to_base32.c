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

#ifndef HAVE_DO_40BIT
static unsigned char *do_40bit(unsigned char *dst, uint64_t d1)
{
	uint64_t d2;
# if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ >= 8
	uint64_t m1;

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
	d1  += 0x6161616161616161ULL;
	m1   = has_greater(d1, 0x7A);
	m1 >>= 7;
	m1   = 0x49 * m1;
	d1  -= m1;
	/* write out */
	put_unaligned_be64(d1, dst);
# else
	uint32_t a1, a2;
	uint32_t b1, b2;
	uint32_t m1, m2;

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
	a1  += 0x61616161UL;          a2  += 0x61616161UL;
	m1   = has_greater(a1, 0x7A); m2   = has_greater(a2, 0x7A);
	m1 >>= 7;                     m2 >>= 7;
	m1   = 0x49 * m1;             m2   = 0x49 * m2;
	a1  -= m1;                    a2  -= m2;
	/* write out */
	put_unaligned_be32(a1, dst);
	put_unaligned_be32(a2, dst+4);
# endif
	return dst + 8;
}
#endif

static inline uint32_t rol32(uint32_t word, unsigned int shift)
{
	return (word << shift) | (word >> (32 - shift));
}

F_NAME(unsigned char *, to_base32, _generic)(unsigned char *dst, const unsigned char *src, unsigned len)
{
#ifndef ONLY_REMAINDER
	while(len >= sizeof(uint64_t))
	{
		uint64_t d = get_unaligned_be64(src);
		src += 5;
		len -= 5;
		dst = do_40bit(dst, d);
	}
#endif
	while(len >= 5)
	{
		uint64_t d = ((uint64_t)get_unaligned_be32(src) << 32) |
		             ((uint64_t)src[4] << 24);
		src += 5;
		len -= 5;
		dst = do_40bit(dst, d);
	}
	/* less than 32 bit left */
	if(len)
	{
		static const unsigned char base32c[] = "abcdefghijklmnopqrstuvwxyz234567=";
		unsigned b32chars = B32_LEN(len);
		uint32_t d;
		unsigned i;

		/* collect the bytes */
		for(i = len, d = 0; i; i--)
			d = (d << 8) | *src++;

		/* bring to start position */
		d = rol32(d, (sizeof(d) - len) * BITS_PER_CHAR + 5);
		i = 0;
		/* write out */
		do
		{
			*dst++ = base32c[d & 0x1F];
			d = rol32(d, 5);
		} while(++i < b32chars);
	}
	return dst;
}
#undef F_NAME

static char const rcsid_tb32g[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
