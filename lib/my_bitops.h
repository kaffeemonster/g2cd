/*
 * my_bitops.h
 * header-file for some global usefull bitbanging functions
 *
 * Copyright (c) 2004-2015 Jan Seiffert
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
 * $Id: my_bitops.h,v 1.7 2005/11/05 10:17:17 redbully Exp redbully $
 */

#ifndef LIB_MY_BITOPS_H
# define LIB_MY_BITOPS_H

# include <sys/types.h>
# ifndef IN_STRWHATEVER
#  include <string.h>
# endif
# include "other.h"

# define LIB_MY_BITOPS_EXTRN(x) x GCC_ATTR_VIS("hidden")
# define LIB_MY_BITOPS_EXTRN_I(x) GCC_ATTR_DLLEXPORT x
# ifdef ADLER32_C
LIB_MY_BITOPS_EXTRN_I(uint32_t adler32(uint32_t adler, const uint8_t *buf, unsigned len));
# endif
LIB_MY_BITOPS_EXTRN(size_t popcountst(size_t n) GCC_ATTR_CONST GCC_ATTR_FASTCALL);
LIB_MY_BITOPS_EXTRN(size_t flsst(size_t find) GCC_ATTR_CONST GCC_ATTR_FASTCALL);
LIB_MY_BITOPS_EXTRN(size_t introsort_u32(uint32_t a[], size_t n));
LIB_MY_BITOPS_EXTRN(ssize_t bitfield_encode(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len));
LIB_MY_BITOPS_EXTRN(ssize_t bitfield_decode(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len));
LIB_MY_BITOPS_EXTRN(ssize_t bitfield_and(uint8_t *res, size_t t_len, const uint8_t *data, size_t s_len));
LIB_MY_BITOPS_EXTRN(int bitfield_lookup(const uint32_t *vals, size_t v_len, const uint8_t *data, size_t s_len));

LIB_MY_BITOPS_EXTRN(char *cpy_rest(char *dst, const char *src, unsigned i) GCC_ATTR_FASTCALL);
LIB_MY_BITOPS_EXTRN(char *cpy_rest_o(char *dst, const char *src, unsigned i) GCC_ATTR_FASTCALL);
LIB_MY_BITOPS_EXTRN(char *cpy_rest0(char *dst, const char *src, unsigned i) GCC_ATTR_FASTCALL);
LIB_MY_BITOPS_EXTRN(void *memxorcpy(void *dst, const void *src1, const void *src2, size_t len));
static inline void *memxor(void *dst, const void *src, size_t len)
{
	return memxorcpy(dst, dst, src, len);
}
LIB_MY_BITOPS_EXTRN(void *memand(void *dst, const void *src, size_t len));
LIB_MY_BITOPS_EXTRN(void *memneg(void *dst, const void *src, size_t len));
LIB_MY_BITOPS_EXTRN(size_t mempopcnt(const void *s, size_t len));
LIB_MY_BITOPS_EXTRN(void *mem_searchrn(void *src, size_t len));
LIB_MY_BITOPS_EXTRN(size_t mem_spn_ff(const void *src, size_t len));
LIB_MY_BITOPS_EXTRN(size_t str_spn_space(const char *str));
LIB_MY_BITOPS_EXTRN(char *str_skip_space(char *str));

#ifdef __GLIBC_PREREQ
# if __GLIBC_PREREQ(2, 14) && (defined(__i386__) || defined(__x86_64__))
#  define MEMSPECIAL_DONT_DO_IT
# else
#  undef MEMSPECIAL_DONT_DO_IT
# endif
#else
# undef MEMSPECIAL_DONT_DO_IT
#endif

# undef mempcpy
# if !defined(HAVE_MEMPCPY) && !defined(MEMPCPY_DEFINED)
LIB_MY_BITOPS_EXTRN_I(void *mempcpy(void *restrict dst, const void *restrict src, size_t len));
# endif
LIB_MY_BITOPS_EXTRN(void *my_mempcpy(void *restrict dst, const void *restrict src, size_t len));
#  undef memcpy
LIB_MY_BITOPS_EXTRN(void *my_memcpy(void *restrict dst, const void *restrict src, size_t len));

# ifdef MEMSPECIAL_DONT_DO_IT
#  define my_memmove(a, b, c) memmove(a, b, c)
# else
#  undef memmove
LIB_MY_BITOPS_EXTRN(void *my_memmove(void *dst, const void *src, size_t len));
LIB_MY_BITOPS_EXTRN(void *my_memcpy_fwd(void *dst, const void *src, size_t len));
LIB_MY_BITOPS_EXTRN(void *my_memcpy_rev(void *dst, const void *src, size_t len));
# endif

# undef memchr
LIB_MY_BITOPS_EXTRN(void *my_memchr(const void *s, int c, size_t n));
LIB_MY_BITOPS_EXTRN(int strncasecmp_a(const char *s1, const char *s2, size_t n));
LIB_MY_BITOPS_EXTRN_I(char *strpcpy(char *restrict dst, const char *restrict src));
LIB_MY_BITOPS_EXTRN_I(char *strlpcpy(char *restrict dst, const char *restrict src, size_t maxlen));
# ifndef HAVE_STRNLEN
LIB_MY_BITOPS_EXTRN_I(size_t strnlen(const char *s, size_t maxlen) GCC_ATTR_PURE);
#  define STRNLEN_DEFINED
# endif
# ifndef HAVE_STRLEN
LIB_MY_BITOPS_EXTRN_I(size_t strlen(const char *s) GCC_ATTR_PURE);
#  define STRLEN_DEFINED
# endif
# ifndef HAVE_STRCHRNUL
LIB_MY_BITOPS_EXTRN_I(char *strchrnul(const char *s, int c) GCC_ATTR_PURE);
#  define STRCHRNUL_DEFINED
# endif
# ifndef HAVE_STRRCHR
LIB_MY_BITOPS_EXTRN_I(char *strrchr(const char *s, int c) GCC_ATTR_PURE);
#  define STRRCHR_DEFINED
# endif

# define strlitcmp(x, y)	(memcmp((x), (y), str_size(y)))
# define strlitcpy(x, y)	(memcpy((x), (y), str_size(y)))
# define strplitcpy(x, y)	(mempcpy((x), (y), str_size(y)))

LIB_MY_BITOPS_EXTRN(unsigned char *to_base16(unsigned char *dst, const unsigned char *src, unsigned len));
#define B32_LEN(x) (((x) * BITS_PER_CHAR + 4) / 5)
LIB_MY_BITOPS_EXTRN(unsigned char *to_base32(unsigned char *dst, const unsigned char *src,  unsigned len));

static inline void strreverse(char *begin, char *end)
{
	char tchar;

	while(end > begin)
		tchar = *end, *end-- = *begin, *begin++ = tchar;
}

LIB_MY_BITOPS_EXTRN(void strreverse_l(char *begin, char *end));

static inline char *strcpyreverse(char *dst, const char *begin, const char *end)
{
	while(end >= begin)
		*dst++ = *end--;
	return dst;
}

/* round up to a power of two */
static inline size_t roundup_power_of_2(size_t c)
{
	c--;
	c |= c >> 1;
	c |= c >> 2;
	c |= c >> 4;
	c |= c >> 8;
	c |= c >> 16;
	if((sizeof(c)*BITS_PER_CHAR) > 32)
		c |= (c >> 16) >> 16;
	return c + 1;
}

LIB_MY_BITOPS_EXTRN(size_t decode_html_entities_utf8(char *dest, const char *src, size_t len));

# define CFB(x) (1 << ((x)%32))
# define CFF_DEFAULT (1 << 0)
# define CFF_AVX_TST (1 << 1)
# define CFF_AVX512_TST (1 << 2)
struct test_cpu_feature
{
	void (*func)(void);
	int flags;
# if defined(__i386__) || defined(__x86_64__)
	uint32_t features[9];
# endif
};

LIB_MY_BITOPS_EXTRN(void *test_cpu_feature(const struct test_cpu_feature *, size_t));
LIB_MY_BITOPS_EXTRN(void patch_instruction(void *where, const struct test_cpu_feature *t, size_t l));
LIB_MY_BITOPS_EXTRN(void emit_emms(void));
LIB_MY_BITOPS_EXTRN(unsigned get_cpus_online(void));
LIB_MY_BITOPS_EXTRN(void cpu_detect_finish(void));

# endif
