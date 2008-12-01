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
# include "hthash.h"

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
		likely(AF_INET == src->s_fam) ? (const void *)&src->in.sin_addr : (const void *)&src->in6.sin6_addr,
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
// TODO: when IPv6 is common, change it
	return likely(AF_INET == addr->s_fam) ? addr->in.sin_port : addr->in6.sin6_port;
}

static inline void combo_addr_set_port(union combo_addr *addr, in_port_t port)
{
// TODO: when IPv6 is common, change it
	if(likely(AF_INET == addr->s_fam))
		addr->in.sin_port = port;
	else
		addr->in6.sin6_port = port;
}

# define SLASH04 htonl(0xF0000000)
# define SLASH08 htonl(0xFF000000)
# define SLASH16 htonl(0xFFFF0000)
# define SLASH32 htonl(0xFFFFFFFF)
# define IP_CMP(a, b, m) (htonl(b) == ((a) & (m)))
static inline int combo_addr_is_public(const union combo_addr *addr)
{
	in_addr_t a;

// TODO: when IPv6 is common, change it
	if(unlikely(AF_INET6 == addr->s_fam))
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

	if(IP_CMP(a, 0xFFFFFFFF, SLASH32)) /* 255.255.255.255/32  Broadcast */
		return 0;
	if(IP_CMP(a, 0x00000000, SLASH08)) /* 000.000.000.000/8 */
		return 0;
	if(IP_CMP(a, 0xA0000000, SLASH08)) /* 010.000.000.000/8 */
		return 0;
	if(IP_CMP(a, 0x7F000000, SLASH08)) /* 127.000.000.000/8 */
		return 0;
	if(IP_CMP(a, 0xA9FE0000, SLASH16)) /* 169.254.000.000/16  APIPA auto addresses*/
		return 0;
	if(IP_CMP(a, 0xAC100000, SLASH16)) /* 172.016.000.000/16 */
		return 0;
	if(IP_CMP(a, 0xC0A80000, SLASH16)) /* 192.168.000.000/16 */
		return 0;
	if(IP_CMP(a, 0xE0000000, SLASH04)) /* 224.000.000.000/4   Multicast */
		return 0;
out:
	return 1;
}
# undef SLASH04
# undef SLASH08
# undef SLASH16
# undef SLASH32
# undef IP_CMP

static inline uint32_t combo_addr_hash(const union combo_addr *addr, uint32_t seed)
{
	uint32_t h;

// TODO: when IPv6 is common, change it
	if(likely(addr->s_fam == AF_INET))
		h = hthash_2words(addr->in.sin_addr.s_addr, addr->in.sin_port, seed);
	else
		h = hthash_3words(addr->in6.sin6_addr.s6_addr32[0], addr->in6.sin6_addr.s6_addr32[3],
		                  addr->in6.sin6_port, seed);
	return h;
}



#endif /* COMBO_ADDR_H */
