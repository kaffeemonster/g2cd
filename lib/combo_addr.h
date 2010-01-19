/*
 * combo_addr.h
 *
 * combined IPv4 & IPv6 address
 *
 * Copyright (c) 2008-2009 Jan Seiffert
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
# include <stdbool.h>
# include <string.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include "other.h"
# include "hthash.h"


/*
 * since IPv6 is in "heavy deployment", they should not suddenly
 * change the addr len...
 */
# define INET6_ADDRLEN 16

/*
 * Compat foo
 * DANGER, it gets ugly...
 */

# ifndef HAVE_SA_FAMILY_T
typedef unsigned short sa_family_t;
# endif
# ifndef HAVE_IN_PORT_T
typedef uint16_t in_port_t;
# endif
# ifndef HAVE_SOCKLEN_T
typedef uint32_t socklen_t;
# endif

# ifndef HAVE_IPV6
struct in6_addr
{
	union
	{
		uint8_t  u6_addr8[16];
		uint16_t u6_addr16[8];
		uint32_t u6_addr32[4];
	} in6_u;
#  define s6_addr   in6_u.u6_addr8
#  define s6_addr16 in6_u.u6_addr16
#  define s6_addr32 in6_u.u6_addr32
};
#  ifndef AF_INET6
#   define AF_INET6 10
#  endif
struct sockaddr_in6
{
	sa_family_t sin6_family;
	in_port_t sin6_port;       /* Transport layer port # */
	uint32_t sin6_flowinfo;    /* IPv6 flow information */
	struct in6_addr sin6_addr; /* IPv6 address */
	uint32_t sin6_scope_id;    /* IPv6 scope-id */
}

#  define IN6_IS_ADDR_UNSPECIFIED(a) \
	(((const uint32_t *)(a))[0] == 0 && \
	 ((const uint32_t *)(a))[1] == 0 && \
	 ((const uint32_t *)(a))[2] == 0 && \
	 ((const uint32_t *)(a))[3] == 0)
#  define IN6_IS_ADDR_LOOPBACK(a) \
	(((const uint32_t *)(a))[0] == 0 && \
	 ((const uint32_t *)(a))[1] == 0 && \
	 ((const uint32_t *)(a))[2] == 0 && \
	 ((const uint32_t *)(a))[3] == htonl(1))
#  define IN6_IS_ADDR_MULTICAST(a) \
	(((const uint8_t *)(a))[0] == 0xff)
#  define IN6_IS_ADDR_LINKLOCAL(a) \
	((((const uint32_t *)(a))[0] & htonl(0xffc00000)) == \
	  htonl(0xfe800000))
#  define IN6_IS_ADDR_SITELOCAL(a) \
	((((const uint32_t *)(a))[0] & htonl(0xffc00000)) == \
	  htonl (0xfec00000))
#  define IN6_IS_ADDR_V4MAPPED(a) \
	((((const uint32_t *)(a))[0] == 0) && \
	 (((const uint32_t *)(a))[1] == 0) && \
	 (((const uint32_t *)(a))[2] == htonl (0xffff)))
#  define IN6_IS_ADDR_V4COMPAT(a) \
	((((const uint32_t *)(a))[0] == 0) && \
	 (((const uint32_t *)(a))[1] == 0) && \
	 (((const uint32_t *)(a))[2] == 0) && \
	 (ntohl(((const uint32_t *)(a))[3]) > 1))
#  define IN6_ARE_ADDR_EQUAL(a,b) \
	((((const uint32_t *)(a))[0] == ((const uint32_t *)(b))[0]) && \
	 (((const uint32_t *)(a))[1] == ((const uint32_t *)(b))[1]) && \
	 (((const uint32_t *)(a))[2] == ((const uint32_t *)(b))[2]) &&  \
	 (((const uint32_t *)(a))[3] == ((const uint32_t *)(b))[3]))
# endif /* HAVE_IPV6 */

# ifndef HAVE_INET6_ADDRSTRLEN
/*
 * This buffersize is needed, but we don't have it everywere, even on Systems
 * that claim to be IPv6 capable...
 */
#  define INET6_ADDRSTRLEN 46
# endif /* HAVE_INET6_ADDRSTRLEN */

# ifndef s6_addr32
/*
 * All in6_addr member besides a basic char array are nonstandard
 * So we have to see if we have a s6_addr32.
 * There is either no define, or hidden behind a #ifdef _KERNEL
 * which does not sound like i want to set it.
 */
#  ifdef __sun__
#   define s6_addr32 _S6_un._S6_u32
#  elif defined(__FreeBSD__) || defined(BSD)
#   define s6_addr32 __u6_addr.__u6_addr32
#  else
#   error "and how is your s6_addr32 obfuscated?"
#  endif
# endif

# ifndef HAVE_INET_NTOP
const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt);
# endif
# ifndef HAVE_INET_PTON
int inet_pton(int af, const char *src, void *dst)
# endif
char *inet_ntop_c(int af, const void *src, char *dst, socklen_t cnt) GCC_ATTR_VIS("hidden");

/*
 * Comboaddress to represent IPv4 & IPv6
 */
union combo_addr
{
	/*
	 * dirty deads done dirt cheap,
	 * sa_fam should alias with the first member
	 * off all struct: *_family
	 */
	sa_family_t         s_fam;
/*	struct sockaddr     sa; */
	/*
	 * struct sockaddr is normaly unusable. It is a kind
	 * of "virtual" baseclass, which only usefull member is
	 * the sa_family element. Still its the type every socket
	 * api function takes. Evertime you want to do something
	 * usefull: cast your real implementation like sockaddr_in
	 * to sockaddr or the other way round, everytime you get
	 * one (resolv etc.) cast it to the implementation.
	 *
	 * With this member in the union things are neat: work with
	 * the implementations, pass the sa element into the api.
	 * No lengthy casts.
	 *
	 * Unfortunatly they decided that struct sockaddr should
	 * be able to represent every implementation, "physically"/
	 * memorywise, even if not usable (only after a cast).
	 * struct sockaddr is as large as the largest sockaddr_*
	 * which is round about 140 byte for AF_UNIX, which contain
	 * a large char array (path to the socket).
	 * And that SUCKS memorywise!
	 * sockaddr_in6 is not small, but smaller than this.
	 * GCC can not save us, AFAIK there is no attribute which
	 * can make this member virtual/weak/immaterial/no space
	 * allocated, only symbol.
	 * So we leave it out again, back to casts...
	 */
	struct sockaddr_in  in;
	struct sockaddr_in6 in6;
};

static inline struct sockaddr *casa(union combo_addr *in)
{
	return (struct sockaddr *)in;
}

static inline const struct sockaddr *casac(const union combo_addr *in)
{
	return (const struct sockaddr *)in;
}

static inline const char *combo_addr_print(const union combo_addr *src, char *dst, socklen_t cnt)
{
	return inet_ntop(src->s_fam,
		likely(AF_INET == src->s_fam) ? (const void *)&src->in.sin_addr : (const void *)&src->in6.sin6_addr,
		dst, cnt);
}

static inline char *combo_addr_print_c(const union combo_addr *src, char *dst, socklen_t cnt)
{
	return inet_ntop_c(src->s_fam,
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
# define SLASH15 htonl(0xFFFE0000)
# define SLASH16 htonl(0xFFFF0000)
# define SLASH24 htonl(0xFFFFFF00)
# define SLASH32 htonl(0xFFFFFFFF)
# define IP_CMP(a, b, m) (unlikely(htonl(b) == ((a) & (m))))
static inline bool combo_addr_is_public(const union combo_addr *addr)
{
	in_addr_t a;

// TODO: when IPv6 is common, change it
	if(unlikely(AF_INET6 == addr->s_fam))
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
		/* keep test for v4 last */
		if(IN6_IS_ADDR_V4MAPPED(a6) ||
		   IN6_IS_ADDR_V4COMPAT(a6))
			a = a6->s6_addr32[3];
		else
			goto out;
	}
	else
		a = addr->in.sin_addr.s_addr;

	/* according to RFC 3330 & RFC 5735 */
	if(IP_CMP(a, 0xFFFFFFFF, SLASH32)) /* 255.255.255.255/32  Broadcast */
		return false;
	if(IP_CMP(a, 0x00000000, SLASH08)) /* 000.000.000.000/8   "this" net, "this" host */
		return false;
	if(IP_CMP(a, 0xA0000000, SLASH08)) /* 010.000.000.000/8   private */
		return false;
	/* 14.0.0.0/8 X25,X121 Public Data Networks, dead/empty?
	   subject to allocation to RIRs? -> RFC 5735 */
	/* 24.0.0.0/8 IP over cable television systems
	   subject to allocation to RIRs  -> RFC 5735 */
	/* 39.0.0.0/8 Class A Subnet experiment
	   subject to allocation to RIRs  -> RFC 5735 */
	if(IP_CMP(a, 0x7F000000, SLASH08)) /* 127.000.000.000/8   loopback */
		return false;
	/* 128.0.0.0/16 lowest class B net
	   subject to allocation to RIRs  -> RFC 5735 */
	if(IP_CMP(a, 0xA9FE0000, SLASH16)) /* 169.254.000.000/16  APIPA auto addresses*/
		return false;
	if(IP_CMP(a, 0xAC100000, SLASH16)) /* 172.016.000.000/16  private */
		return false;
	/* 191.255.0.0/16 highest class B
	   subject to allocation to RIRs  -> RFC 5735 */
	/* 192.0.0.0/24 lowest class C
	   Future protocol assignments */
	if(IP_CMP(a, 0xC0000200, SLASH24)) /* 192.000.002.000/24  Test-net-1, like example.com */
		return false;
	if(IP_CMP(a, 0xC0586300, SLASH16)) /* 192.088.099.000/24  6to4 relays anycast */
		return false; /* only sinks, not source */
	if(IP_CMP(a, 0xC0A80000, SLASH16)) /* 192.168.000.000/16  private */
		return false;
	if(IP_CMP(a, 0xC6120000, SLASH15)) /* 198.018.000.000/15  Benchmark Network */
		return false;
	if(IP_CMP(a, 0xC6336400, SLASH24)) /* 198.051.100.000/24  Test-net-2, like example.com */
		return false;
	if(IP_CMP(a, 0xCB007100, SLASH24)) /* 203.000.113.000/24  Test-net-3, like example.com */
		return false;
	/* 223.255.255.0/24 highest class C
	   subject to allocation to RIRs  -> RFC 5735 */
	if(IP_CMP(a, 0xE0000000, SLASH04)) /* 224.000.000.000/4   Multicast */
		return false;
	if(IP_CMP(a, 0xF0000000, SLASH04)) /* 240.000.000.000/4   Future use */
		return false;
out:
	return true;
}
# undef SLASH04
# undef SLASH08
# undef SLASH15
# undef SLASH16
# undef SLASH25
# undef SLASH32
# undef IP_CMP

static inline uint32_t combo_addr_hash(const union combo_addr *addr, uint32_t seed)
{
	uint32_t h;

// TODO: when IPv6 is common, change it
	if(likely(addr->s_fam == AF_INET))
		h = hthash_2words(addr->in.sin_addr.s_addr, addr->in.sin_port, seed);
	else
		h = hthash_5words(addr->in6.sin6_addr.s6_addr32[0],
		                  addr->in6.sin6_addr.s6_addr32[1],
		                  addr->in6.sin6_addr.s6_addr32[2],
		                  addr->in6.sin6_addr.s6_addr32[3],
		                  addr->in6.sin6_port, seed);
	return h;
}

static inline uint32_t combo_addr_hash_ip(const union combo_addr *addr, uint32_t seed)
{
	uint32_t h;

// TODO: when IPv6 is common, change it
	if(likely(addr->s_fam == AF_INET))
		h = hthash_1words(addr->in.sin_addr.s_addr, seed);
	else
		h = hthash_4words(addr->in6.sin6_addr.s6_addr32[0],
		                  addr->in6.sin6_addr.s6_addr32[1],
		                  addr->in6.sin6_addr.s6_addr32[2],
		                  addr->in6.sin6_addr.s6_addr32[3],
		                  seed);
	return h;
}

static inline bool combo_addr_eq(const union combo_addr *a, const union combo_addr *b)
{
	bool ret = a->s_fam == a->s_fam;
	if(!ret)
		return ret;
// TODO: when IPv6 is common, change it
	if(likely(AF_INET == a->s_fam)) {
		return a->in.sin_addr.s_addr == b->in.sin_addr.s_addr &&
		       a->in.sin_port == b->in.sin_port;
	} else {
		return !!IN6_ARE_ADDR_EQUAL(&a->in6.sin6_addr, &b->in6.sin6_addr) &&
		       a->in6.sin6_port == b->in6.sin6_port;
	}
}

static inline bool combo_addr_eq_ip(const union combo_addr *a, const union combo_addr *b)
{
	bool ret = a->s_fam == a->s_fam;
	if(!ret)
		return ret;
// TODO: when IPv6 is common, change it
	if(likely(AF_INET == a->s_fam)) {
		return a->in.sin_addr.s_addr == b->in.sin_addr.s_addr;
	} else {
		return !!IN6_ARE_ADDR_EQUAL(&a->in6.sin6_addr, &b->in6.sin6_addr);
	}
}

static inline bool combo_addr_eq_any(const union combo_addr *a)
{
// TODO: when IPv6 is common, change it
	if(likely(AF_INET == a->s_fam)) {
		return INADDR_ANY == a->in.sin_addr.s_addr;
	} else {
		return 0 == a->in6.sin6_addr.s6_addr32[0] && 0 == a->in6.sin6_addr.s6_addr32[1] &&
		       0 == a->in6.sin6_addr.s6_addr32[2] && 0 == a->in6.sin6_addr.s6_addr32[3];
	}
}

static inline unsigned combo_addr_lin(uint32_t *buf, const union combo_addr *a)
{
// TODO: when ipv6 is common, change it
	if(likely(AF_INET == a->s_fam))
	{
		buf[0] = a->in.sin_addr.s_addr;
		buf[1] = a->in.sin_port;
		return 2;
	}
	else
	{
		buf[0] = a->in6.sin6_addr.s6_addr32[0];
		buf[1] = a->in6.sin6_addr.s6_addr32[1];
		buf[2] = a->in6.sin6_addr.s6_addr32[2];
		buf[3] = a->in6.sin6_addr.s6_addr32[3];
		buf[4] = a->in6.sin6_port;
		return 5;
	}
}

static inline unsigned combo_addr_lin_ip(uint32_t *buf, const union combo_addr *a)
{
// TODO: when ipv6 is common, change it
	if(likely(AF_INET == a->s_fam))
	{
		buf[0] = a->in.sin_addr.s_addr;
		return 1;
	}
	else
	{
		buf[0] = a->in6.sin6_addr.s6_addr32[0];
		buf[1] = a->in6.sin6_addr.s6_addr32[1];
		buf[2] = a->in6.sin6_addr.s6_addr32[2];
		buf[3] = a->in6.sin6_addr.s6_addr32[3];
		return 4;
	}
}


#endif /* COMBO_ADDR_H */
