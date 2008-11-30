/*
 * combo_addr.h
 *
 * combined IPv4 & IPv6 address
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
 * $Id:$
 */

#ifndef LIB_COMBO_ADDR_H
# define LIB_COMBO_ADDR_H
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include "other.h"

/*
 * Comboaddress to represent IPv4 & IPv6
 */
union combo_addr
{
	/* dirty deads done dirt cheap,
	 * sa_fam should alias with the first member
	 * off all struct: *_family
	 */
	sa_family_t         s_fam;
	struct sockaddr     sa;
	struct sockaddr_in  in;
	struct sockaddr_in6 in6;
};

static inline const char *combo_addr_print(const union combo_addr *src, char *dst, socklen_t cnt)
{
	return inet_ntop(src->s_fam,
		AF_INET == src->s_fam ? (const void *)&src->in.sin_addr : (const void *)&src->in6.sin6_addr,
		dst, cnt);
}

static inline int combo_addr_read(const char *src, union combo_addr *dst)
{
	int ret_val;

	ret_val = inet_pton(AF_INET, src, &dst->in.sin_addr);
	if(0 < ret_val)
		dst->s_fam = AF_INET;
	else if(0 == ret_val)
	{
		ret_val = inet_pton(AF_INET6, src, &dst->in6.sin6_addr);
		if(0 < ret_val)
			dst->s_fam = AF_INET6;
	}
	return ret_val;
}

static inline in_port_t combo_addr_port(const union combo_addr *addr)
{
	return AF_INET == addr->s_fam ? addr->in.sin_port : addr->in6.sin6_port;
}

static inline void combo_addr_set_port(union combo_addr *addr, in_port_t port)
{
	if(AF_INET == addr->s_fam)
		addr->in.sin_port = port;
	else
		addr->in6.sin6_port = port;
}

# define SLASH04 0xF0000000
# define SLASH08 0xFF000000
# define SLASH16 0xFFFF0000
# define SLASH32 0xFFFFFFFF
static inline int combo_addr_is_public(const union combo_addr *addr)
{
	in_addr_t a;

	if(AF_INET6 == addr->s_fam)
	{
		const struct in6_addr *a6 = &addr->in6.sin6_addr;
		if(IN6_IS_ADDR_UNSPECIFIED(a6))
			return 0;
		if(IN6_IS_ADDR_LOOPBACK(a6))
			return 0;
		if(IN6_IS_ADDR_MULTICAST(a6))
			return 0;
		if(IN6_IS_ADDR_LINKLOCAL(a6))
			return 0;
		if(IN6_IS_ADDR_SITELOCAL(a6))
			return 0;
		/* keep test for v4 last */
		if(IN6_IS_ADDR_V4MAPPED(a6) ||
		   IN6_IS_ADDR_V4COMPAT(a6))
			a = a6->s6_addr32[3];
		else
			goto out;
	}
	else
		a = addr->in.sin_addr.s_addr;

	if(0xFFFFFFFF == (a & SLASH32)) /* 255.255.255.255/32  Broadcast */
		return 0;
	if(0x00000000 == (a & SLASH08)) /* 000.000.000.000/8 */
		return 0;
	if(0xA0000000 == (a & SLASH08)) /* 010.000.000.000/8 */
		return 0;
	if(0x7F000000 == (a & SLASH08)) /* 127.000.000.000/8 */
		return 0;
	if(0xA9FE0000 == (a & SLASH16)) /* 169.254.000.000/16  APIPA auto addresses*/
		return 0;
	if(0xAC100000 == (a & SLASH16)) /* 172.016.000.000/16 */
		return 0;
	if(0xC0A80000 == (a & SLASH16)) /* 192.168.000.000/16 */
		return 0;
	if(0xE0000000 == (a & SLASH04)) /* 224.000.000.000/4   Multicast */
		return 0;
out:
	return 1;
}
# undef SLASH04
# undef SLASH08
# undef SLASH16
# undef SLASH32


#endif /* COMBO_ADDR_H */
