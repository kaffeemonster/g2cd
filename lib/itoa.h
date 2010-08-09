/*
 * itoa.h
 * number print helper
 *
 * Copyright (c) 2008-2010 Jan Seiffert
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
#ifndef ITOA_H
# define ITOA_H

# include "my_bitops.h"
# include "other.h"

# define HEXUC_STRING "0123456789ABCDEFGHIJKL"
# define HEXLC_STRING "0123456789abcdefghijkl"

# define SIGNED_KERNEL(buff, wptr, n) \
	if(n < 0) \
		*buff++ = '-'; \
	n = n < 0 ? -n : n; \
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
		n = n < 0 ? -n : n; \
		wptr = buff; \
		do { *wptr++ = (n % 10) + '0'; n /= 10; } while(--rem && n); \
	} else \
		return NULL

# define UNSIGNED_KERNEL_N(buff, wptr, n, rem) \
	wptr = buff; \
	do { *wptr++ = (n % 10) + '0'; n /= 10; } while(--rem && n)

# define HEXU_KERNEL(buff, wptr, n) \
	do { \
		static const char hexchar[] = HEXUC_STRING; \
		wptr = buff; \
		do { *wptr++ = hexchar[n % 16]; n /= 16; } while(n); \
	} while(0)

# define HEXL_KERNEL(buff, wptr, n) \
	do { \
		static const char hexchar[] = HEXLC_STRING; \
		wptr = buff; \
		do { *wptr++ = hexchar[n % 16]; n /= 16; } while(n); \
	} while(0)

# define HEXU_KERNEL_N(buff, wptr, n, rem) \
	do { \
		static const char hexchar[] = HEXUC_STRING; \
		wptr = buff; \
		do { *wptr++ = hexchar[n % 16]; n /= 16; } while(--rem && n); \
	} while(0)

# define HEXL_KERNEL_N(buff, wptr, n, rem) \
	do { \
		static const char hexchar[] = HEXLC_STRING; \
		wptr = buff; \
		do { *wptr++ = hexchar[n % 16]; n /= 16; } while(--rem && n); \
	} while(0)

# define MAKE_FUNC_X(prfx, type, kernel, add) \
	static inline char *prfx##to##add(char *buff, const type num) \
	{ \
		char *wptr; \
		type n = num; \
		kernel(buff, wptr, n); \
		strreverse(buff, wptr - 1); \
		return wptr; \
	}

# define MAKE_FUNC(prfx, type, kernel) MAKE_FUNC_X(prfx, type, kernel, a)

# define MAKE_FUNC_N_X(prfx, type, kernel, add) \
	static inline char *prfx##nto##add(char *buff, size_t rem, const type num) \
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

# define MAKE_FUNC_N(prfx, type, kernel) MAKE_FUNC_N_X(prfx, type, kernel, a)

# define MAKE_FUNC_FIX_X(prfx, type, kernel, fill, fixprfx, add) \
	static inline char *prfx##to##add##_##fixprfx##fix(char *buff, const type num, unsigned digits) \
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

# define MAKE_FUNC_FIX(prfx, type, kernel, fill, fixprfx) MAKE_FUNC_FIX_X(prfx, type, kernel, fill, fixprfx, a)

# define MAKE_SFUNC_0FIX(prfx, type) MAKE_FUNC_FIX(prfx, type, SIGNED_KERNEL, '0', 0)
# define MAKE_UFUNC_0FIX(prfx, type) MAKE_FUNC_FIX(prfx, type, UNSIGNED_KERNEL, '0', 0)
# define MAKE_SFUNC_SFIX(prfx, type) MAKE_FUNC_FIX(prfx, type, SIGNED_KERNEL, ' ', s)
# define MAKE_UFUNC_SFIX(prfx, type) MAKE_FUNC_FIX(prfx, type, UNSIGNED_KERNEL, ' ', s)
# define MAKE_SFUNC_N(prfx, type) MAKE_FUNC_N(prfx, type, SIGNED_KERNEL_N)
# define MAKE_UFUNC_N(prfx, type) MAKE_FUNC_N(prfx, type, UNSIGNED_KERNEL_N)
# define MAKE_SFUNC(prfx, type) MAKE_FUNC(prfx, type, SIGNED_KERNEL)
# define MAKE_UFUNC(prfx, type) MAKE_FUNC(prfx, type, UNSIGNED_KERNEL)
# define MAKE_LXFUNC_0FIX(prfx, type) MAKE_FUNC_FIX_X(prfx, type, HEXL_KERNEL, '0', 0, xa)
# define MAKE_UXFUNC_0FIX(prfx, type) MAKE_FUNC_FIX_X(prfx, type, HEXU_KERNEL, '0', 0, Xa)
# define MAKE_LXFUNC_SFIX(prfx, type) MAKE_FUNC_FIX_X(prfx, type, HEXL_KERNEL, ' ', s, xa)
# define MAKE_UXFUNC_SFIX(prfx, type) MAKE_FUNC_FIX_X(prfx, type, HEXU_KERNEL, ' ', s, Xa)
# define MAKE_LXFUNC_N(prfx, type) MAKE_FUNC_N_X(prfx, type, HEXL_KERNEL_N, xa)
# define MAKE_UXFUNC_N(prfx, type) MAKE_FUNC_N_X(prfx, type, HEXU_KERNEL_N, Xa)
# define MAKE_LXFUNC(prfx, type) MAKE_FUNC_X(prfx, type, HEXL_KERNEL, xa)
# define MAKE_UXFUNC(prfx, type) MAKE_FUNC_X(prfx, type, HEXU_KERNEL, Xa)

#define MAKE_SFUNC_SET(prfx, type) \
	MAKE_SFUNC(       prfx, type) \
	MAKE_SFUNC_N(     prfx, type) \
	MAKE_SFUNC_SFIX(  prfx, type) \
	MAKE_SFUNC_0FIX(  prfx, type)

#define MAKE_UFUNC_SET(prfx, type) \
	MAKE_UFUNC(       prfx, type) \
	MAKE_UFUNC_N(     prfx, type) \
	MAKE_UFUNC_SFIX(  prfx, type) \
	MAKE_UFUNC_0FIX(  prfx, type) \
	MAKE_LXFUNC(      prfx, type) \
	MAKE_UXFUNC(      prfx, type) \
	MAKE_LXFUNC_N(    prfx, type) \
	MAKE_UXFUNC_N(    prfx, type) \
	MAKE_LXFUNC_SFIX( prfx, type) \
	MAKE_UXFUNC_SFIX( prfx, type) \
	MAKE_LXFUNC_0FIX( prfx, type) \
	MAKE_UXFUNC_0FIX( prfx, type)

MAKE_SFUNC_SET(   c,   signed char)
MAKE_UFUNC_SET(  uc, unsigned char)
MAKE_SFUNC_SET(   s,   signed short)
MAKE_UFUNC_SET(  us, unsigned short)
MAKE_SFUNC_SET(   i,   signed int)
MAKE_UFUNC_SET(   u, unsigned int)
MAKE_SFUNC_SET(   l,   signed long)
MAKE_UFUNC_SET(  ul, unsigned long)
MAKE_SFUNC_SET(  ll,   signed long long)
MAKE_UFUNC_SET( ull, unsigned long long)
MAKE_SFUNC_SET(   z,          ssize_t)
MAKE_UFUNC_SET(  uz,          size_t)


static inline char *addrtoa(char *buff, const void *ptr)
{
	static const char hexchar[] = HEXUC_STRING;
	char *wptr = buff + (sizeof(ptr) * 2);
	uintptr_t p = (uintptr_t)ptr;
	unsigned i;

	for(i = 0; i < (sizeof(ptr) * 2); i++)
		*wptr-- = hexchar[(p >> (i  * 4)) & 0x0F];

	return buff + (sizeof(ptr) * 2);
}

static inline char *ptoa(char *buff, const void *ptr)
{
	static const char hexchar[] = HEXLC_STRING;
	char *wptr;
	uintptr_t p = (uintptr_t)ptr;
	unsigned i;

	buff[0] = '0';
	buff[1] = 'x';
	wptr = &buff[1] + (sizeof(ptr) * 2);
	for(i = 0; i < (sizeof(ptr) * 2); i++)
		*wptr-- = hexchar[(p >> (i  * 4)) & 0x0F];

	return buff + (sizeof(ptr) * 2) + 2;
}

char *put_dec_trunc(char *buf, unsigned q) GCC_ATTR_VIS("hidden");

static inline char *ustoa_trunc(char *buf, unsigned q)
{
	char *ret_val = put_dec_trunc(buf, q); /* if this is really a short, this is enough */
	strreverse(buf, ret_val - 1);
	return ret_val;
}

#undef HEXUC_STRING
#undef HEXLC_STRING
#undef SIGNED_KERNEL
#undef SIGNED_KERNEL_N
#undef UNSIGNED_KERNEL
#undef UNSIGNED_KERNEL_N
#undef HEXU_KERNEL
#undef HEXU_KERNEL_N
#undef HEXL_KERNEL
#undef HEXL_KERNEL_N
#undef MAKE_FUNC
#undef MAKE_FUNC_X
#undef MAKE_FUNC_N
#undef MAKE_FUNC_N_X
#undef MAKE_FUNC_FIX
#undef MAKE_FUNC_FIX_X
#undef MAKE_SFUNC_0FIX
#undef MAKE_UFUNC_0FIX
#undef MAKE_SFUNC_SFIX
#undef MAKE_UFUNC_SFIX
#undef MAKE_SFUNC_N
#undef MAKE_UFUNC_N
#undef MAKE_SFUNC
#undef MAKE_UFUNC
#undef MAKE_LXFUNC_0FIX
#undef MAKE_UXFUNC_0FIX
#undef MAKE_LXFUNC_SFIX
#undef MAKE_UXFUNC_SFIX
#undef MAKE_LXFUNC_N
#undef MAKE_UXFUNC_N
#undef MAKE_LXFUNC
#undef MAKE_UXFUNC

#undef MAKE_SFUNC_SET
#undef MAKE_UFUNC_SET

#endif /* ITOA_H */
/* EOF */
