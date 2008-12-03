/*
 * itoa.h
 * number print helper
 *
 * Copyright (c) 2008 Jan Seiffert
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
#ifndef ITOA_H
# define ITOA_H

# include "my_bitops.h"
# include "other.h"

# define HEXC_STRING "0123456789ABCDEFGHIJKL"

# define SIGNED_KERNEL(buff, wptr, n) \
	if(n < 0) \
		*buff++ = '-'; \
	wptr = buff; \
	do { *wptr++ = (n % 10) + '0'; n /= 10; } while(n)

# define UNSIGNED_KERNEL(buff, wptr, n) \
	wptr = buff; \
	do { *wptr++ = (n % 10) + '0'; n /= 10; } while(n)

# define SIGNED_KERNEL_N(buff, wptr, n, rem) \
	if(n < 0) { \
		*buff++ = '-'; \
		rem--; \
	} \
	if(likely(rem)) { \
		wptr = buff; \
		do { *wptr++ = (n % 10) + '0'; n /= 10; } while(--rem && n); \
	} else \
		return NULL

# define UNSIGNED_KERNEL_N(buff, wptr, n, rem) \
	wptr = buff; \
	do { *wptr++ = (n % 10) + '0'; n /= 10; } while(--rem && n)

# define HEX_KERNEL(buff, wptr, n) \
	do { \
		static const char hexchar[] = HEXC_STRING; \
		wptr = buff; \
		do { *wptr++ = hexchar[n % 16]; n /= 16; } while(n); \
	} while(0)

# define MAKE_FUNC(prfx, type, kernel) \
	static inline char *prfx##toa(char *buff, const type num) \
	{ \
		char *wptr; \
		type n = num; \
		kernel(buff, wptr, n); \
		strreverse(buff, wptr - 1); \
		return wptr; \
	}

# define MAKE_FUNC_N(prfx, type, kernel) \
	static inline char *prfx##ntoa(char *buff, size_t rem, const type num) \
	{ \
		char *wptr = NULL; \
		type n = num; \
		if(likely(rem)) \
		{ \
			kernel(buff, wptr, n, rem); \
			if(unlikely(n && !rem)) \
				return NULL; \
			strreverse(buff, wptr - 1); \
		} \
		return wptr; \
	}

# define MAKE_FUNC_FIX(prfx, type, kernel, fill, fixprfx) \
	static inline char *prfx##toa_##fixprfx##fix(char *buff, const type num, unsigned digits) \
	{ \
		char *wptr0, *wptr1; \
		type n = num; \
		for(wptr0 = buff; digits--;) \
			*wptr0++ = fill; \
		kernel(buff, wptr1, n); \
		wptr0 = wptr0 >= wptr1 ? wptr0 : wptr1; \
		strreverse(buff, wptr0 - 1); \
		return wptr0; \
	} \

# define MAKE_SFUNC_0FIX(prfx, type) MAKE_FUNC_FIX(prfx, type, SIGNED_KERNEL, '0', 0)
# define MAKE_UFUNC_0FIX(prfx, type) MAKE_FUNC_FIX(prfx, type, UNSIGNED_KERNEL, '0', 0)
# define MAKE_SFUNC_SFIX(prfx, type) MAKE_FUNC_FIX(prfx, type, SIGNED_KERNEL, ' ', s)
# define MAKE_UFUNC_SFIX(prfx, type) MAKE_FUNC_FIX(prfx, type, UNSIGNED_KERNEL, ' ', s)
# define MAKE_SFUNC_N(prfx, type) MAKE_FUNC_N(prfx, type, SIGNED_KERNEL_N)
# define MAKE_UFUNC_N(prfx, type) MAKE_FUNC_N(prfx, type, UNSIGNED_KERNEL_N)
# define MAKE_SFUNC(prfx, type) MAKE_FUNC(prfx, type, SIGNED_KERNEL)
# define MAKE_UFUNC(prfx, type) MAKE_FUNC(prfx, type, UNSIGNED_KERNEL)

MAKE_SFUNC(       c,   signed char)
MAKE_UFUNC(      uc, unsigned char)
MAKE_UFUNC_N(    uc, unsigned char)
MAKE_SFUNC(       s,   signed short)
MAKE_UFUNC(      us, unsigned short)
MAKE_SFUNC(       i,   signed int)
MAKE_SFUNC_SFIX(  i,   signed int)
MAKE_SFUNC_N(     i,   signed int)
MAKE_UFUNC(       u, unsigned int)
MAKE_UFUNC_SFIX(  u, unsigned int)
MAKE_UFUNC_N(     u, unsigned int)
MAKE_SFUNC(       l,   signed long)
MAKE_SFUNC_SFIX(  l,   signed long)
MAKE_SFUNC_N(     l,   signed long)
MAKE_UFUNC(      ul, unsigned long)
MAKE_UFUNC_SFIX( ul, unsigned long)
MAKE_UFUNC_N(    ul, unsigned long)
MAKE_SFUNC(      ll,   signed long long)
MAKE_UFUNC(     ull, unsigned long long)


static inline char *ptoa(char *buff, const void *ptr)
{
	static const char hexchar[] = HEXC_STRING;
	char *wptr = buff + (sizeof(ptr) * 2);
	uintptr_t p = (uintptr_t)ptr;
	unsigned i;

	for(i = 0; i < sizeof(ptr); i++) {
		*wptr-- = hexchar[(p >> (   i  * 4)) & 0x0F];
		*wptr-- = hexchar[(p >> ((1+i) * 4)) & 0x0F];
	}

	return buff + (sizeof(ptr) * 2);
}

static inline char *ustoxa(char *buff, const unsigned short num)
{
	static const char hexchar[] = "0123456789abcdefghjkl";
	char *wptr;
	unsigned n = num;

	wptr = buff;
	do {
		*wptr++ = hexchar[n % 16];
		n /= 16;
	} while(n);
	strreverse(buff, wptr - 1);
	return wptr;
}

static inline char *utoXa_0fix(char *buff, const unsigned num, unsigned fix)
{
	char *wptr0, *wptr1;
	unsigned n = num;
	unsigned i;

	for(wptr0 = buff, i = fix; i--;)
		*wptr0++ = '0';

	HEX_KERNEL(buff, wptr1, n);
	wptr0 = wptr0 >= wptr1 ? wptr0 : wptr1;
	strreverse(buff, wptr0 - 1);

	return wptr0;
}

static inline char *ultoXa_0fix(char *buff, const unsigned long num, unsigned fix)
{
	char *wptr0, *wptr1;
	unsigned long n = num;
	unsigned i;

	for(wptr0 = buff, i = fix; i--;)
		*wptr0++ = '0';

	HEX_KERNEL(buff, wptr1, n);
	wptr0 = wptr0 >= wptr1 ? wptr0 : wptr1;
	strreverse(buff, wptr0 - 1);

	return wptr0;
}

#undef HEXC_STRING
#undef SIGNED_KERNEL
#undef UNSIGNED_KERNEL
#undef UNSIGNED_KERNEL_N
#undef HEX_KERNEL
#undef MAKE_FUNC
#undef MAKE_FUNC_N
#undef MAKE_FUNC_FIX
#undef MAKE_SFUNC_0FIX
#undef MAKE_UFUNC_0FIX
#undef MAKE_SFUNC_SFIX
#undef MAKE_UFUNC_SFIX
#undef MAKE_SFUNC_N
#undef MAKE_UFUNC_N
#undef MAKE_SFUNC
#undef MAKE_UFUNC


#endif /* ITOA_H */
/* EOF */
