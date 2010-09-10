/*
 * inet_pton.c
 *
 * read IPv4 or IPv6 addresses
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
 * inet_pton - turn a astring into an internet address
 * af: address family
 * src: to string
 * dst: the buffer to store address
 *
 * return value: 1 on success, 0 if the address could not be parsed,
 *              -1 on error
 *
 * there are systems out there (cygwin, old solaris), which
 * do know IPv6, but don't know inet_pton. Since inet_pton
 * also fits nicely in a threaded enviroment (buffer supplied,
 * no static thing), simply always provide our own one.
 */

#include "../config.h"
#include <errno.h>
#include "other.h"
#include "combo_addr.h"

static int read_octet(const char *src, const char **end)
{
	const char *src_start;
	int c, num = 0;

	for(c = *src++, src_start = src; c && 256 > num; c = *src++)
	{
		c -= '0';
		if(c >= 0 && c <= 9)
			num = (num * 10) + c;
		else
			break;
		if(src - src_start > 1 && !num) {
			src = src_start;
			break;
		}
	}
	if(end)
		*end = src - 1;
	return num;
}

static int read_ipv4(const char *src, struct in_addr *dst)
{
	uint8_t tbuf[sizeof(*dst)];
	int octets;

	for(octets = 0; octets < 4; octets++)
	{
		const char *end;
		int num = read_octet(src, &end);
		if(255 < num || end == src)
			return 0;
		src = end;
		tbuf[octets] = num;
		if(octets < 3 && '.' != *src++)
			return 0;
	}
	if('\0' != *src)
		return 0;
	memcpy(dst, tbuf, sizeof(*dst));
	return 1;
}

static int read_xhextet(const char *src, const char **end)
{
	const char *src_start;
	int c, num = 0;

	for(c = *src++, src_start = src; c && num <= 0xFFFF; c = *src++)
	{
		int d = c - '0', x = c - 'a';
		if((d >= 0 && d <= 9) || (x >= 0 && x <= 5))
		{
			num <<= 4;
			if(d >= 0 && d <= 9)
				num += d;
			else
				num += x + 10;
		}
		else
			break;
		if(src - src_start > 1 && !num) {
			src = src_start;
			break;
		}
	}
	if(end)
		*end = src - 1;
	return num;
}

static int read_ipv6(const char *src, struct in6_addr *dst)
{
	uint16_t tbuf[INET6_ADDRLEN/2];
	int hextets, zero_point = -1;

// TODO: fuck, reading IPv6 addresses is a nightmare...
	/* strip starting :: */
	if(':' == *src)
	{
		if(':' != *++src)
			return 0;
		zero_point = 0;
		src++;
	}
	for(hextets = 0; hextets < INET6_ADDRLEN/2; hextets++)
	{
		const char *end;
		int num = read_xhextet(src, &end);
		if(num > 0xFFFF || (end == src && *end != '\0'))
			return 0;
		tbuf[hextets] = htons(num);
		if(':' == *end)
		{
			if(':' == *++end)
			{
				if(-1 < zero_point)
					return 0;
				end++;
				zero_point = hextets + 1;
			} else if ('\0' == *end)
				return 0;
		}
		else if('.' == *end) /* upsie, a IPv4 dot? */
		{
			if(0 > zero_point)
				return 0;
// TODO: Does this work with BEHAVE/96 addresses like with MAPPED?
			if((tbuf + (INET6_ADDRLEN/2)) - (tbuf + hextets) >= 4)
			{
				int ret_val = read_ipv4(src, (struct in_addr *)&tbuf[hextets]);
				if(1 > ret_val)
					return ret_val;
				hextets += 2;
				goto move_around;
			}
			return 0;
		}
		else if('\0' == *end) {
			src = end;
			hextets++;
			break;
		} else
			return 0;
		src = end;
	}
	if('\0' != *src)
		return 0;
move_around:
	if(-1 < zero_point)
	{
		int n = hextets - zero_point;
		uint16_t *sp, *ep;
		if(!n)
			return 0;
		sp = &tbuf[hextets-1];
		ep = &tbuf[(INET6_ADDRLEN/2)-1];
		while(n--)
			*ep-- = *sp--;
		sp = &tbuf[zero_point-1];
		while(ep > sp)
			*ep-- = 0;
	}

	memcpy(dst, tbuf, sizeof(tbuf));
	return 1;
}

int inet_pton(int af, const char *src, void *dst)
{
// TODO: change when IPv6 is famous
	if(likely(AF_INET == af))
		return read_ipv4(src, dst);
	else if(AF_INET6 == af)
		return read_ipv6(src, dst);
	errno = EAFNOSUPPORT;
	return -1;
}

static char const rcsid_iptn[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
