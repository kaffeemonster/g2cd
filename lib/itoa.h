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

#define HEXC_STRING "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!\"§$%&/()=?+#*\\{}[]^<>|,;.:-_ \t"

#define SIGNED_KERNEL(buff, wptr, n) \
	if(n < 0) \
		*buff++ = '-'; \
	wptr = buff; \
	do { *wptr++ = (n % 10) + '0'; n /= 10; } while(n)

#define UNSIGNED_KERNEL(buff, wptr, n) \
	wptr = buff; \
	do { *wptr++ = (n % 10) + '0'; n /= 10; } while(n)

#define UNSIGNED_KERNEL_N(buff, wptr, n, rem) \
	wptr = buff; \
	do { *wptr++ = (n % 10) + '0'; n /= 10; } while(--rem && n)

#define HEX_KERNEL(buff, wptr, n) \
	do { \
		static const char hexchar[] = HEXC_STRING; \
		wptr = buff; \
		do { *wptr++ = hexchar[n % 16]; n /= 16; } while(n); \
	} while(0)


static inline char *stoa(char *buff, const short num)
{
	char *wptr;
	short n = num;

	SIGNED_KERNEL(buff, wptr, n);
	strreverse(buff, wptr - 1);
	return wptr;
}

static inline char *ustoa(char *buff, const unsigned short num)
{
	char *wptr;
	unsigned short n = num;

	UNSIGNED_KERNEL(buff, wptr, n);
	strreverse(buff, wptr - 1);
	return wptr;
}

static inline char *itoa(char *buff, const int num)
{
	char *wptr;
	int n = num;

	SIGNED_KERNEL(buff, wptr, n);
	strreverse(buff, wptr - 1);
	return wptr;
}

static inline char *itoa_sfix(char *buff, const int num, unsigned fix)
{
	char *wptr0, *wptr1;
	int n = num;
	unsigned i;

	for(wptr0 = buff, i = fix; i--;)
		*wptr0++ = ' ';

	SIGNED_KERNEL(buff, wptr1, n);
	wptr0 = wptr0 >= wptr1 ? wptr0 : wptr1;
	strreverse(buff, wptr0 - 1);
	return wptr0;
}

static inline char *utoa(char *buff, const unsigned num)
{
	char *wptr;
	unsigned n = num;

	UNSIGNED_KERNEL(buff, wptr, n);
	strreverse(buff, wptr - 1);
	return wptr;
}

static inline char *untoa(char *buff, size_t rem, const unsigned num)
{
	char *wptr = NULL;
	unsigned n = num;

	if(rem)
	{
		UNSIGNED_KERNEL_N(buff, wptr, n, rem);
		if(n && !rem)
			return NULL;
		strreverse(buff, wptr - 1);
	}
	return wptr;
}

static inline char *ltoa(char *buff, const long num)
{
	char *wptr;
	long n = num;

	SIGNED_KERNEL(buff, wptr, n);
	strreverse(buff, wptr - 1);
	return wptr;
}

static inline char *ultoa(char *buff, const unsigned long num)
{
	char *wptr;
	unsigned long n = num;

	UNSIGNED_KERNEL(buff, wptr, n);
	strreverse(buff, wptr - 1);
	return wptr;
}

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

#endif /* ITOA_H */
/* EOF */
