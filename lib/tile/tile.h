/*
 * tile.h
 * special tile instructions
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

#ifndef TILE_TILE_H
# define TILE_TILE_H

# define SOUL (sizeof(unsigned long))
# define SOULM1 (SOUL - 1)
# include "../other.h"

extern void no_such_instruction(void);

static inline unsigned long v1ddotpua(unsigned long d, unsigned long a, unsigned long b)
{
# ifdef __tilegx__
	d = __insn_v1ddotpua(d, a, b);
# else
	no_such_instruction();
# endif
	return d;
}

static inline unsigned long v4shl(unsigned long a, unsigned long b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v4shl(a, b);
# else
	no_such_instruction();
# endif
	return r;
}

static inline unsigned long v4shru(unsigned long a, unsigned long b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v4shru(a, b);
# else
	no_such_instruction();
# endif
	return r;
}

static inline unsigned long v2sub(unsigned long a, unsigned long b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v2sub(a, b);
# else
	r = __insn_subh(a, b);
# endif
	return r;
}

static inline unsigned long v4sub(unsigned long a, unsigned long b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v4sub(a, b);
# else
	no_such_instruction();
# endif
	return r;
}

static inline unsigned long v4add(unsigned long a, unsigned long b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v4add(a, b);
# else
	no_such_instruction();
# endif
	return r;
}

static inline unsigned long v1sadau(unsigned long d, unsigned long a, unsigned long b)
{
# ifdef __tilegx__
	d = __insn_v1sadau(d, a, b);
# else
	d = __insn_sadab_u(d, a, b);
# endif
	return d;
}

static inline unsigned long v1cmpeqi(unsigned long a, const signed char b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v1cmpeqi(a, b);
# else
	r = __insn_seqib(a, b);
# endif
	return r;
}
# undef has_nul_byte
# define has_nul_byte(x) v1cmpeqi(x, 0)

static inline unsigned long v1cmpeq(unsigned long a, unsigned long b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v1cmpeq(a, b);
# else
	r = __insn_seqb(a, b);
# endif
	return r;
}
# undef has_eq_byte
# define has_eq_byte(x,y) v1cmpeq(x, y)

static inline unsigned long v1cmpne(unsigned long a, unsigned long b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v1cmpne(a, b);
# else
	r = __insn_sneb(a, b);
# endif
	return r;
}

static inline unsigned long v1cmpltui(unsigned long a, const signed char b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v1cmpltui(a, b);
# else
	r = __insn_sltib_u(a, b);
# endif
	return r;
}
# undef has_greater
static inline unsigned long has_greater(unsigned long x, const signed char b)
{
	x  = v1cmpltui(x, b + 1);
	x ^= MK_C(0x01010101);
	return x << 7;
}
# undef has_between
static inline unsigned long tile_has_between(unsigned long a, const signed char x, const signed char y)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v1cmpltui(a, x+1);
	a = __insn_v1cmpltui(a, y);
# else
	r = __insn_sltib_u(a, x+1);
	a = __insn_sltib_u(a, y);
# endif
	return a ^ r;
}
# define has_between(x,n,m) (tile_has_between(x,n,m)<<7)

static inline unsigned long v2cmpeqi(unsigned long a, const signed char b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v2cmpeqi(a, b);
# else
	r = __insn_seqih(a, b);
# endif
	return r;
}
# undef has_nul_word
# define has_nul_word(x) v2cmpeqi(x, 0)

static inline unsigned long v2cmpeq(unsigned long a, unsigned long b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v2cmpeq(a, b);
# else
	r = __insn_seqh(a, b);
# endif
	return r;
}
# undef has_eq_word
# define has_eq_word(x,y) v2cmpeq(x, y)

static inline unsigned long v2cmpltu(unsigned long a, unsigned long b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v2cmpltu(a, b);
# else
	r = __insn_slth_u(a, b);
# endif
	return r;
}

static inline unsigned long v2mz(unsigned long a, unsigned long b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v2mz(a, b);
# else
	r = __insn_mzh(a, b);
# endif
	return r;
}

static inline unsigned long v1int_h(unsigned long a, unsigned long b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v1int_h(a, b);
# else
	r = __insn_inthb(a, b);
# endif
	return r;
}

static inline unsigned long v1int_l(unsigned long a, unsigned long b)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_v1int_l(a, b);
# else
	r = __insn_intlb(a, b);
# endif
	return r;
}

static inline unsigned long tile_shufflebytes(unsigned long d, unsigned long a, unsigned long b)
{
# ifdef __tilegx__
	d = __insn_shufflebytes(d, a, b);
# else
	no_such_instruction();
# endif
	return d;
}

static inline unsigned long tile_ldna(const void *x)
{
	unsigned long r;
# ifdef __tilegx__
	r = __insn_ldna(x);
# elif defined(__tilepro__)
	r = __insn_lw_na(x);
# else
	r = *(const unsigned long *)(ALIGN_DOWN(x, SOUL));
# endif
	return r;
}

static inline unsigned long tile_align(unsigned long d, unsigned long a, const void *x)
{
# ifdef __tilegx__
	d = __insn_dblalign(d, a, x);
# elif defined(__tilepro__)
	d = __insn_dword_align(d, a, x);
# else
	unsigned shift1, shift2;
	shift1  = (unsigned)ALIGN_DOWN_DIFF(x, SOUL);
	shift2  = SOUL - shift1;
	shift1 *= BITS_PER_CHAR;
	shift2 *= BITS_PER_CHAR;
	if(HOST_IS_BIGENDIAN)
		d = d << shift1 | a >> shift2;
	else
		d = d >> shift1 | a << shift2;
# endif
	return d;
}

#endif
