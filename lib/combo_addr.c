/*
 * combo_addr.c
 *
 * combined IPv4 & IPv6 address, large funcs
 *
 * Copyright (c) 2008-2016 Jan Seiffert
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

#ifdef HAVE_CONFIG_H
# include "../config.h"
#endif
#include "other.h"
#include <string.h>
#include <stdlib.h>
#include "combo_addr.h"
#include "hthash.h"

bool combo_addr_read_wport(char *str, union combo_addr *addr)
{
	char *t;
	int ret_val;
	unsigned port = 0;

	/* opening IPv6 bracket? */
	if('[' == *str)
	{
		str++;
		/* do we have the closing bracket? */
		t = strrchr(str, ']');
		if(!t)
			return false;
		*t++ = '\0';
		if(':' == *t) { /* and here comes the port */
			port = atoi(t+1);
			if(port >= 1 << 16)
				return false;
		}

		ret_val = inet_pton(AF_INET6, str, &addr->in6.sin6_addr);
		if(0 < ret_val) {
			addr->s.fam = AF_INET6;
			casalen_ii(addr);
		} else
			return false;
// TODO: try IPv4?
	}
	else
	{
		/* ipv4 with port or ipv6 without? */
		t = strrchr(str, ':');
		if(t)
		{
			char *x = t;
			/* ok, we have one ':', do we have another? */
			do {
				x--;
			} while(x >= str && ':' != *x);
			if(x >= str)
			{
				/* another ':', we guess it's ipv6  without port */
				ret_val = inet_pton(AF_INET6, str, &addr->in6.sin6_addr);
				if(0 < ret_val) {
					addr->s.fam = AF_INET6;
					casalen_ii(addr);
				} else
					return false;
// TODO: try IPv4?
			}
			else
			{
				/* nope, this is prop. the port */
				*t++ = '\0';
				port = atoi(t);
				if(port >= 1 << 16)
					return false;
				ret_val = inet_pton(AF_INET, str, &addr->in.sin_addr);
				if(0 < ret_val) {
					addr->s.fam = AF_INET;
					casalen_ii(addr);
				} else
					return false;
// TODO: try IPv6?
			}
		}
		else
		{
			ret_val = inet_pton(AF_INET, str, &addr->in.sin_addr);
			if(0 < ret_val) {
				addr->s.fam = AF_INET;
				casalen_ii(addr);
			} else
				return false;
// TODO: try IPv6?
		}
	}
	combo_addr_set_port(addr, htons(port));
	return true;
}

uint32_t combo_addr_hash(const union combo_addr *addr, uint32_t seed)
{
	uint32_t h;

// TODO: when IPv6 is common, change it
	if(likely(addr->s.fam == AF_INET))
		h = hthash_2words(addr->in.sin_addr.s_addr, addr->in.sin_port, seed);
	else
		h = hthash_5words(addr->in6.sin6_addr.s6_addr32[0],
		                  addr->in6.sin6_addr.s6_addr32[1],
		                  addr->in6.sin6_addr.s6_addr32[2],
		                  addr->in6.sin6_addr.s6_addr32[3],
		                  addr->in6.sin6_port, seed);
	return h;
}

uint32_t combo_addr_hash_extra(const union combo_addr *addr, uint32_t extra, uint32_t seed)
{
	uint32_t h;

// TODO: when IPv6 is common, change it
	if(likely(addr->s.fam == AF_INET))
		h = hthash_3words(addr->in.sin_addr.s_addr, addr->in.sin_port, extra, seed);
	else
		h = hthash_6words(addr->in6.sin6_addr.s6_addr32[0],
		                  addr->in6.sin6_addr.s6_addr32[1],
		                  addr->in6.sin6_addr.s6_addr32[2],
		                  addr->in6.sin6_addr.s6_addr32[3],
		                  addr->in6.sin6_port, extra, seed);
	return h;
}

uint32_t combo_addr_hash_ip(const union combo_addr *addr, uint32_t seed)
{
	uint32_t h;

// TODO: when IPv6 is common, change it
	if(likely(addr->s.fam == AF_INET))
		h = hthash_1words(addr->in.sin_addr.s_addr, seed);
	else
		h = hthash_4words(addr->in6.sin6_addr.s6_addr32[0],
		                  addr->in6.sin6_addr.s6_addr32[1],
		                  addr->in6.sin6_addr.s6_addr32[2],
		                  addr->in6.sin6_addr.s6_addr32[3],
		                  seed);
	return h;
}

# define SLASH04 htonl(0xF0000000)
# define SLASH08 htonl(0xFF000000)
# define SLASH10 htonl(0xFFC00000)
# define SLASH15 htonl(0xFFFE0000)
# define SLASH16 htonl(0xFFFF0000)
# define SLASH24 htonl(0xFFFFFF00)
# define SLASH28 htonl(0xFFFFFFF0)
# define SLASH32 htonl(0xFFFFFFFF)
# define IP_CMP(a, b, m) (unlikely(htonl(b) == ((a) & (m))))

/*
 *
 * http://www.iana.org/assignments/iana-ipv4-special-registry/iana-ipv4-special-registry.xhtml
 * http://www.iana.org/assignments/iana-ipv6-special-registry/iana-ipv6-special-registry.xhtml
 *
 */
bool combo_addr_is_public(const union combo_addr *addr)
{
	in_addr_t a;

// TODO: when IPv6 is common, change it
	if(unlikely(AF_INET6 == addr->s.fam))
	{
		const struct in6_addr *a6 = &addr->in6.sin6_addr;
		if(unlikely(IN6_IS_ADDR_UNSPECIFIED(a6)))
			return false;
		if(unlikely(IN6_IS_ADDR_LOOPBACK(a6)))
			return false;
		if(unlikely(IN6_IS_ADDR_MULTICAST(a6)))
			return false;
		if(unlikely(IN6_IS_ADDR_LINKLOCAL(a6)))
			return false;
		if(unlikely(IN6_IS_ADDR_SITELOCAL(a6)))
			return false;
		if(unlikely(IN6_IS_ADDR_UNIQUELOCAL_A(a6)))
			return false;
		if(unlikely(IN6_IS_ADDR_UNIQUELOCAL_B(a6)))
			return false;
		if(unlikely(IN6_IS_ADDR_IETFBLOCK(a6)))
			return false;
		if(unlikely(IN6_IS_ADDR_DISCARD(a6)))
			return false;
		if(unlikely(IN6_IS_ADDR_DOCU(a6)))
			return false;
		if(unlikely(IN6_IS_ADDR_AS112(a6)))
			return false;
		/* the compat range (::0/96) is deprecated,
		 * do not talk to it till it gets reassigned */
		if(IN6_IS_ADDR_V4COMPAT(a6))
			return false;
		/* keep test for v4 last */
		if(IN6_IS_ADDR_V4MAPPED(a6) ||
		   IN6_IS_ADDR_V4BEHAVE(a6))
			a = a6->s6_addr32[3];
		else
			goto out;
	}
	else
		a = addr->in.sin_addr.s_addr;

	/* according to RFC 3330 & RFC 5735 */
	if(IP_CMP(a, 0xFFFFFFFF, SLASH32)) /* 255.255.255.255/32  Broadcast RFC919 */
		return false;
	if(IP_CMP(a, 0xC0000000, SLASH24)) /* 192.000.000.000/24  lowest class C Future protocol assignments RFC6890 */
		return false;
	/*
	 * 192.000.000.000/24 is not for general consumption. Until now only special IETF allocations in this range.
	 * -> 192.000.000.000/29 IPv4 Service Continuity Prefix RFC7335
	 * -> 192.000.000.008/32 IPv4 dummy address RFC7600
	 * -> 192.000.000.009/32 Port Control Protocol Anycast RFC7723
	 * -> 192.000.000.170/32
	 *    192.000.000.171/32 NAT64/DNS64 Discovery RFC7050
	 * 192.0.0.0/29 is reserved for the p2p tunnel link between
	 * B4 & AFTR (CPE and Carrier NAT).
	 * These should not show up in the internet.
	 * Only question is:
	 * - ICMP messages may "come" from there
	 * - A machine beeing the B4 and running a servent may output
	 *   this address, even for a short period, till it finds it
	 *   outside address
	 * Still, we can not answer to it, this address is "not
	 * reachable". Since its use is optional, some provider
	 * may set up their own "dead end", undocumented...
	 */
//TODO: add DS-Lite well known addresses
	if(IP_CMP(a, 0xC0000200, SLASH24)) /* 192.000.002.000/24  Test-net-1 RFC5737, like example.com */
		return false;
	if(IP_CMP(a, 0xC01FC400, SLASH24)) /* 192.031.196.000/24  AS112-v4 allocation RFC7535, local addr DNS sink hole */
		return false;
	if(IP_CMP(a, 0xC034C100, SLASH24)) /* 192.052.193.000/24  AMT RFC7450, Multicast-Unicast-tunnel relay anycast allocation */
		return false; /* only sinks, not source */
	if(IP_CMP(a, 0xC0586300, SLASH24)) /* 192.088.099.000/24  6to4 relays anycast RFC7526 */
		return false; /* only sinks, not source */
	if(IP_CMP(a, 0xC0AF3000, SLASH24)) /* 192.178.048.000/24  AS112 allocation RFC7534, local addr DNS sink hole */
		return false;
	/* APNIC provided documentation prefixes */
	if(IP_CMP(a, 0xC6336400, SLASH24)) /* 198.051.100.000/24  Test-net-2 RFC5737, like example.com */
		return false;
	if(IP_CMP(a, 0xCB007100, SLASH24)) /* 203.000.113.000/24  Test-net-3 RFC5737, like example.com */
		return false;
	/* 223.255.255.0/24 highest class C
	   subject to allocation to RIRs  -> RFC 5735 */
	/* 128.0.0.0/16 lowest class B net
	   subject to allocation to RIRs  -> RFC 5735 */
	if(IP_CMP(a, 0xA9FE0000, SLASH16)) /* 169.254.000.000/16  APIPA Link Local auto addresses RFC3927 */
		return false;
	if(IP_CMP(a, 0xAC100000, SLASH16)) /* 172.016.000.000/16  private RFC1918 */
		return false;
	if(IP_CMP(a, 0xC0A80000, SLASH16)) /* 192.168.000.000/16  private RFC1918 */
		return false;
	if(IP_CMP(a, 0xC6120000, SLASH15)) /* 198.018.000.000/15  Benchmark Network RFC2544 */
		return false;
	/* 191.255.0.0/16 highest class B
	   subject to allocation to RIRs  -> RFC 5735 */
	if(IP_CMP(a, 0x64400000, SLASH10)) /* 100.064.000.000/10   Shared Addr Space RFC6598, priv addr in routing networks, ex. between CGN and CPE, like RFC1918 */
		return false;
	if(unlikely(IP_CMP(a, 0x00000000, SLASH08))) /* 000.000.000.000/8   "this" net, "this" host RFC1122 */
		return false;
	if(IP_CMP(a, 0x0A000000, SLASH08)) /* 010.000.000.000/8   private RFC1918 */
		return false;
	/* 14.0.0.0/8 X25,X121 Public Data Networks, dead/empty?
	   subject to allocation to RIRs? -> RFC 5735 */
	/* 24.0.0.0/8 IP over cable television systems
	   subject to allocation to RIRs  -> RFC 5735 */
	/* 39.0.0.0/8 Class A Subnet experiment
	   subject to allocation to RIRs  -> RFC 5735 */
	if(IP_CMP(a, 0x7F000000, SLASH08)) /* 127.000.000.000/8   loopback RFC1122 */
		return false;
	if(IP_CMP(a, 0xE0000000, SLASH04)) /* 224.000.000.000/4   Multicast */
		return false;
	if(IP_CMP(a, 0xF0000000, SLASH04)) /* 240.000.000.000/4   Future use/Reserved RFC1112 */
		return false;
out:
	return true;
}

bool combo_addr_is_forbidden(const union combo_addr *addr)
{
	in_addr_t a;

// TODO: when IPv6 is common, change it
	if(unlikely(AF_INET6 == addr->s.fam))
	{
		const struct in6_addr *a6 = &addr->in6.sin6_addr;
		if(unlikely(IN6_IS_ADDR_UNSPECIFIED(a6)))
			return true;
		if(unlikely(IN6_IS_ADDR_LOOPBACK(a6)))
			return true;
		if(unlikely(IN6_IS_ADDR_MULTICAST(a6)))
			return true;
		if(unlikely(IN6_IS_ADDR_IETFBLOCK(a6)))
			return true;
		if(unlikely(IN6_IS_ADDR_DISCARD(a6)))
			return true;
		if(unlikely(IN6_IS_ADDR_DOCU(a6)))
			return true;
		if(unlikely(IN6_IS_ADDR_AS112(a6)))
			return true;
		/* the compat range (::0/96) is deprecated,
		 * do not talk to it till it gets reassigned */
		if(IN6_IS_ADDR_V4COMPAT(a6))
			return true;
		/* keep test for v4 last */
		if(IN6_IS_ADDR_V4MAPPED(a6) ||
		   IN6_IS_ADDR_V4BEHAVE(a6))
			a = a6->s6_addr32[3];
		else
			goto out;
	}
	else
		a = addr->in.sin_addr.s_addr;

	/* according to RFC 3330 & RFC 5735 */
	if(IP_CMP(a, 0xFFFFFFFF, SLASH32)) /* 255.255.255.255/32  Broadcast RFC919 */
		return true;
	if(IP_CMP(a, 0xC0000000, SLASH24)) /* 192.000.000.000/24  lowest class C Future protocol assignments RFC6890 */
		return true;
	/*
	 * 192.000.000.000/24 is not for general consumption. Until now only special allocations in this range.
	 * -> 192.000.000.000/29 IPv4 Service Continuity Prefix RFC7335
	 * -> 192.000.000.008/32 IPv4 dummy address RFC7600
	 * -> 192.000.000.009/32 Port Control Protocol Anycast RFC7723
	 * -> 192.000.000.170/32
	 *    192.000.000.171/32 NAT64/DNS64 Discovery RFC7050
	 */
	if(IP_CMP(a, 0xC0000200, SLASH24)) /* 192.000.002.000/24  Test-net-1 RFC5737, like example.com */
		return true;
	if(IP_CMP(a, 0xC01FC400, SLASH24)) /* 192.031.196.000/24  AS112-v4 allocation RFC7535, local addr DNS sink hole */
		return true;
	if(IP_CMP(a, 0xC034C100, SLASH24)) /* 192.052.193.000/24  AMT RFC7450, Multicast-Unicast-tunnel relay anycast allocation */
		return true; /* only sinks, not source */
	if(IP_CMP(a, 0xC0586300, SLASH24)) /* 192.088.099.000/24  6to4 relays anycast RFC7526 */
		return true; /* only sinks, not source */
	if(IP_CMP(a, 0xC0AF3000, SLASH24)) /* 192.178.048.000/24  AS112 allocation RFC7534, local addr DNS sink hole */
		return true;
	/* APNIC provided documentation prefixes */
	if(IP_CMP(a, 0xC6336400, SLASH24)) /* 198.051.100.000/24  Test-net-2 RFC5737, like example.com */
		return true;
	if(IP_CMP(a, 0xCB007100, SLASH24)) /* 203.000.113.000/24  Test-net-3 RFC5737, like example.com */
		return true;
	/* 223.255.255.0/24 highest class C
	   subject to allocation to RIRs  -> RFC 5735 */
	/* 128.0.0.0/16 lowest class B net
	   subject to allocation to RIRs  -> RFC 5735 */
	if(IP_CMP(a, 0xC6120000, SLASH15)) /* 198.018.000.000/15  Benchmark Network RFC2544 */
		return true;
	/* 191.255.0.0/16 highest class B
	   subject to allocation to RIRs  -> RFC 5735 */
	if(unlikely(IP_CMP(a, 0x00000000, SLASH08))) /* 000.000.000.000/8   "this" net, "this" host RFC1122 */
		return true;
	/* 14.0.0.0/8 X25,X121 Public Data Networks, dead/empty?
	   subject to allocation to RIRs? -> RFC 5735 */
	/* 24.0.0.0/8 IP over cable television systems
	   subject to allocation to RIRs  -> RFC 5735 */
	/* 39.0.0.0/8 Class A Subnet experiment
	   subject to allocation to RIRs  -> RFC 5735 */
	if(IP_CMP(a, 0x7F000000, SLASH08)) /* 127.000.000.000/8   loopback RFC1122 */
		return true;
	if(IP_CMP(a, 0xE0000000, SLASH04)) /* 224.000.000.000/4   Multicast */
		return true;
	if(IP_CMP(a, 0xF0000000, SLASH04)) /* 240.000.000.000/4   Future use/Reserved RFC1112 */
		return true;
out:
	return false;
}

static char const rcsid_ca[] GCC_ATTR_USED_VAR = "$Id:$";
