/*
 * my_mips.h
 * magic mips fudging
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

#ifndef LIB_MIPS_MY_MIPS_H

# ifdef __GNUC__
typedef signed char v4i8 __attribute__((vector_size(4)));
# else
typedef uint32_t v4i8;
# endif
typedef long long a64;

# define SOV4 (sizeof(v4i8))


# if defined(__mips_loongson_vector_rev)
#  include <loongson.h>
#  define SOV8 (sizeof(uint8x8_t))
# else
#  define SOV8 8
# endif

static inline unsigned nul_byte_index_loongson(unsigned r)
{
	if(r & 0x01)
		return 0;
	if(r & 0x02)
		return 1;
	if(r & 0x04)
		return 2;
	if(r & 0x08)
		return 3;
	if(r & 0x10)
		return 4;
	if(r & 0x20)
		return 5;
	if(r & 0x40)
		return 6;
	return 7;
}

static inline unsigned nul_word_index_loongson(unsigned r)
{
	if(r & 0x01)
		return 0;
	if(r & 0x04)
		return 1;
	if(r & 0x10)
		return 2;
	return 3;
}

static inline unsigned nul_byte_index_dsp(unsigned r)
{
# if __mips < 32
	if(HOST_IS_BIGENDIAN)
	{
		if(r & 0x01000000)
			return 0;
		if(r & 0x00010000)
			return 1;
		if(r & 0x00000100)
			return 2;
		return 3;
	}
	else
	{
		if(r & 0x00000001)
			return 0;
		if(r & 0x00000100)
			return 1;
		if(r & 0x00010000)
			return 2;
		return 3;
	}
# else
#  if _GNUC_PREREQ (4,0)
	if(HOST_IS_BIGENDIAN)
		return (unsigned)__builtin_clz(r) / BITS_PER_CHAR;
	else
		return (unsigned)__builtin_ctz(r) / BITS_PER_CHAR;
#  else
	unsigned t;
	if(HOST_IS_BIGENDIAN)
		r = (unsigned)(-((int)r)) & r;
	asm ("clz %0, %1" : "=r" (t) : "r" (r));
	if(HOST_IS_BIGENDIAN)
		t = 31 - t;
	return t / BITS_PER_CHAR;
#  endif
# endif
}

static inline unsigned nul_word_index_dsp(unsigned r)
{
# if __mips < 32
	if(HOST_IS_BIGENDIAN) {
		if(r & 0x00010000)
			return 0;
		return 1;
	} else {
		if(r & 0x00000001)
			return 0;
		return 1;
	}
# else
#  if _GNUC_PREREQ (4,0)
	if(HOST_IS_BIGENDIAN)
		return (unsigned)__builtin_clz(r) / (BITS_PER_CHAR * 2);
	else
		return (unsigned)__builtin_ctz(r) / (BITS_PER_CHAR * 2);
#  else
	unsigned t;
	if(HOST_IS_BIGENDIAN)
		r = (unsigned)(-((int)r)) & r;
	asm ("clz %0, %1" : "=r" (t) : "r" (r));
	if(HOST_IS_BIGENDIAN)
		t = 31 - t;
	return t / (BITS_PER_CHAR * 2);
#  endif
# endif
}

# if __mips == 64 || defined(__mips64)
typedef unsigned long long check_t;
# undef has_nul_byte
# undef has_nul_word
# define has_nul_byte(x) has_nul_byte64(x)
# define has_nul_word(x) has_nul_word64(x)
# else
typedef unsigned check_t;
# undef has_nul_byte
# undef has_nul_word
# define has_nul_byte(x) has_nul_byte32(x)
# define has_nul_word(x) has_nul_word32(x)
# endif

# define SOCT (sizeof(check_t))
# define SOCTM1 (SOCT-1)
# define CHECK_T_BITS (SOCT * BITS_PER_CHAR)
# define MK_CC(x)	((check_t)x | ((check_t)x << CHECK_T_BITS/2))

static inline unsigned nul_byte_index_mips(check_t r)
{
# if __mips == 64 || defined(__mips64)
#  if __mips < 32
	if(HOST_IS_BIGENDIAN)
		return nul_byte_index_b64(r);
	else
		return nul_byte_index_l64(r);
#  else
#   if _GNUC_PREREQ (4,0)
	if(HOST_IS_BIGENDIAN)
		return (unsigned)__builtin_clzll(r) / BITS_PER_CHAR;
	else
		return (unsigned)__builtin_ctzll(r) / BITS_PER_CHAR;
#   else
	unsigned t;
	if(HOST_IS_BIGENDIAN)
		r = (check_t)(-((long long)r)) & r;
	asm ("dclz %0, %1" : "=r" (t) : "r" (r));
	if(HOST_IS_BIGENDIAN)
		t = SOCTM1 - t;
	return t / BITS_PER_CHAR;
#   endif
#  endif
# else
#  if __mips < 32
	if(HOST_IS_BIGENDIAN)
		return nul_byte_index_b32(r);
	else
		return nul_byte_index_l32(r);
#  else
#   if _GNUC_PREREQ (4,0)
	if(HOST_IS_BIGENDIAN)
		return (unsigned)__builtin_clz(r) / BITS_PER_CHAR;
	else
		return (unsigned)__builtin_ctz(r) / BITS_PER_CHAR;
#   else
	unsigned t;
	if(HOST_IS_BIGENDIAN)
		r = (check_t)(-((int)r)) & r;
	asm ("clz %0, %1" : "=r" (t) : "r" (r));
	if(HOST_IS_BIGENDIAN)
		t = SOCTM1 - t;
	return t / BITS_PER_CHAR;
#   endif
#  endif
# endif
}

static inline unsigned nul_word_index_mips(check_t r)
{
# if __mips == 64 || defined(__mips64)
#  if __mips < 32
	if(HOST_IS_BIGENDIAN)
		return nul_word_index_b64(r);
	else
		return nul_word_index_l64(r);
#  else
#   if _GNUC_PREREQ (4,0)
	if(HOST_IS_BIGENDIAN)
		return (unsigned)__builtin_clzll(r) / (BITS_PER_CHAR * 2);
	else
		return (unsigned)__builtin_ctzll(r) / (BITS_PER_CHAR * 2);
#   else
	unsigned t;
	if(HOST_IS_BIGENDIAN)
		r = (check_t)(-((long long)r)) & r;
	asm ("dclz %0, %1" : "=r" (t) : "r" (r));
	if(HOST_IS_BIGENDIAN)
		t = SOCTM1 - t;
	return t / (BITS_PER_CHAR * 2);
#   endif
#  endif
# else
#  if __mips < 32
	if(HOST_IS_BIGENDIAN)
		return nul_word_index_b32(r);
	else
		return nul_word_index_l32(r);
#  else
#   if _GNUC_PREREQ (4,0)
	if(HOST_IS_BIGENDIAN)
		return (unsigned)__builtin_clz(r) / (BITS_PER_CHAR * 2);
	else
		return (unsigned)__builtin_ctz(r) / (BITS_PER_CHAR * 2);
#   else
	unsigned t;
	if(HOST_IS_BIGENDIAN)
		r = (check_t)(-((int)r)) & r;
	asm ("clz %0, %1" : "=r" (t) : "r" (r));
	if(HOST_IS_BIGENDIAN)
		t = SOCTM1 - t;
	return t / (BITS_PER_CHAR * 2);
#   endif
#  endif
# endif
}

# if _MIPS_SZPTR < 64
#  define SZPRFX
# else
#  define SZPRFX "d"
# endif

#endif
