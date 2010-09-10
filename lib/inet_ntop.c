/*
 * inet_ntop.c
 *
 * print IPv4 and IPv6 addresses
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
 * $Id:$
 */

/*
 * inet_ntop - turn an internet address into a string
 * af: address family
 * src: the address
 * dst: the buffer to print to
 * cnt: size of dst
 *
 * return value: a pointer to dst or NULL on error
 *
 * there are systems out there (cygwin, old solaris), which
 * do know IPv6, but don't know inet_ntop. Since inet_ntop
 * also fits nicely in a threaded enviroment (buffer supplied,
 * no static thing), simply always provide our own one.
 */

/*
 * inet_ntop_c - turn an internet address into a string, return the end
 * af: address family
 * src: the address
 * dst: to buffet to print to
 * cnt: size of dst
 *
 * return value: a pointer behind the last character or NULL on error
 *
 * nice to concat a string
 */

#include "../config.h"
#include <errno.h>
#include "other.h"
#include "combo_addr.h"

#ifndef str_size
# define str_size(x)       (sizeof(x) - 1)
#endif

static char *put_dec_8bit(char *buf, unsigned q)
{
	unsigned r;

	r      = (q * 0xcd) >> 11;
	*buf++ = (q - 10 * r) + '0';

	if(r)
	{
		q      = (r * 0xd) >> 7;
		*buf++ = (r - 10 * q)  + '0';

		if(q)
			*buf++ = q + '0';
	}

	return buf;
}

static char *strcpyreverse_l(char *dst, const char *begin, const char *end)
{
	if(sizeof(size_t) > 4)
	{
		for(; unlikely(end - 7 >= begin); end -= 8, dst += 8)
		{
			uint64_t tll1 = get_unaligned((const uint64_t *)(end - 7));
			tll1 = __swab64(tll1);
			put_unaligned(tll1, (uint64_t *)dst);
		}
	}
	for(; end - 3 >= begin; end -= 4, dst += 4)
	{
		uint32_t tl1 = get_unaligned((const uint32_t *)(end - 3));
		tl1 = __swab32(tl1);
		put_unaligned(tl1, (uint32_t *)dst);
	}
	if(end - 1 >= begin)
	{
		uint16_t t1 = get_unaligned((const uint16_t *)(end - 1));
		t1 = __swab16(t1);
		put_unaligned(t1, (uint16_t *)dst);
		end -= 2;
		dst += 2;
	}

	while(end >= begin)
		*dst++ = *end--;
	return dst;
}

static char *print_ipv4_c_rev(const struct in_addr *src, char *dst)
{
	union {
		uint32_t a;
		unsigned char c[4];
	} u;
	int i;

	u.a = src->s_addr;
	for(i = 4; i--;) {
		dst = put_dec_8bit(dst, u.c[i]);
		*dst++ = '.';
	}
	return dst - 1;
}

static char *print_ipv4_c(const struct in_addr *src, char *dst, socklen_t cnt)
{
	char tbuf[sizeof("000.000.000.000") + 1];
	char *wptr;

	if(unlikely(sizeof("1.1.1.1") > (size_t)cnt)) {
		errno = ENOSPC;
		return NULL;
	}

	tbuf[0] = '\0';
	wptr = print_ipv4_c_rev(src, tbuf + 1);
	if((socklen_t)(wptr - tbuf) > cnt) {
		errno = ENOSPC;
		return NULL;
	}
	return strcpyreverse_l(dst, tbuf, wptr - 1) - 1;
}

static const char *print_ipv4(const struct in_addr *src, char *dst, socklen_t cnt)
{
	const char *wptr = print_ipv4_c(src, dst, cnt);
	if(wptr)
		return dst;
	return NULL;
}

static char *print_ipv6_c_rev(const struct in6_addr *src, char *dst)
{
	struct { int start, len; } cur = {-1, -1}, best = {0, 0};
	int i;

	if(unlikely(IN6_IS_ADDR_V4MAPPED(src))) {
		dst = print_ipv4_c_rev((const struct in_addr *)&src->s6_addr32[3], dst);
		return strplitcpy(dst, ":ffff::");
	}

	/* find run of zeros */
	for(i = 0; i < INET6_ADDRLEN/2; i++)
	{
		uint16_t a_word = get_unaligned_be16(&src->s6_addr[i * 2]);
		if(!a_word) {
			if(-1 == cur.start)
				cur.start = i, cur.len = 1;
			else
				cur.len++;
		} else {
			if(-1 != cur.start) {
				if(-1 == best.start || cur.len > best.len)
					best = cur;
				cur.start = -1;
			}
		}
	}
	/* make shure we have the best run after the loop */
	if(-1 != cur.start) {
		if(-1 == best.start || cur.len > best.len)
			best = cur;
	}
	/* is the run long enough? */
	if(-1 != best.start && best.len < 2)
		best.start = -1;

	/* print it */
	if(-1 != best.start && INET6_ADDRLEN/2 == (best.start + best.len))
		*dst++ = ':';
	for(i = INET6_ADDRLEN/2; i--;)
	{
		uint16_t a_word;
		if(-1 != best.start &&
		   i >= best.start &&
		   i < (best.start + best.len)) {
			if(i == best.start)
				*dst++ = ':';
			continue;
		}
		a_word = get_unaligned_be16(&src->s6_addr[i * 2]);
		do {
			*dst++ = "0123456789abcdefghijkl"[a_word % 16];
			a_word /= 16;
		} while(a_word);
		if(i)
			*dst++ = ':';
	}
	return dst;
}

static char *print_ipv6_c(const struct in6_addr *src, char *dst, socklen_t cnt)
{
	char *wptr;
	char tbuf[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];

	if(unlikely((str_size("::1")) > (size_t)cnt)) {
		errno = ENOSPC;
		return NULL;
	}

// TODO: handle BEHAVE prefix same as MAPPED
	if(unlikely(IN6_IS_ADDR_V4MAPPED(src))) {
		if(unlikely((str_size("::ffff:1.1.1.1")) > (size_t)cnt)) {
			errno = ENOSPC;
			return NULL;
		}
		return print_ipv4_c((const struct in_addr *)&src->s6_addr32[3],
		                    strplitcpy(dst, "::ffff:"), cnt - str_size("::ffff:"));
	}

	tbuf[0] = '\0';
	wptr = print_ipv6_c_rev(src, tbuf + 1);
	if((socklen_t)(wptr - tbuf) > cnt) {
		errno = ENOSPC;
		return NULL;
	}
	return strcpyreverse_l(dst, tbuf, wptr - 1) - 1;
}

static const char *print_ipv6(const struct in6_addr *src, char *dst, socklen_t cnt)
{
	const char *wptr = print_ipv6_c(src, dst, cnt);
	if(wptr)
		return dst;
	return NULL;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{
// TODO: change when IPv6 is famous
	if(likely(AF_INET == af))
		return print_ipv4(src, dst, cnt);
	else if(AF_INET6 == af)
		return print_ipv6(src, dst, cnt);
	errno = EAFNOSUPPORT;
	return NULL;
}

char *inet_ntop_c(int af, const void *src, char *dst, socklen_t cnt)
{
// TODO: change when IPv6 is famous
	if(likely(AF_INET == af))
		return print_ipv4_c(src, dst, cnt);
	else if(AF_INET6 == af)
		return print_ipv6_c(src, dst, cnt);
	errno = EAFNOSUPPORT;
	return NULL;
}

char *inet_ntop_c_rev(int af, const void *src, char *dst)
{
// TODO: change when IPv6 is famous
	if(likely(AF_INET == af))
		return print_ipv4_c_rev(src, dst);
	else if(AF_INET6 == af)
		return print_ipv6_c_rev(src, dst);
	errno = EAFNOSUPPORT;
	return NULL;
}

static char const rcsid_intp[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
