/*
 * combo_addr.h
 *
 * combined IPv4 & IPv6 address
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

#ifndef LIB_COMBO_ADDR_H
# define LIB_COMBO_ADDR_H
# include <stdbool.h>
# include <string.h>
# ifdef WIN32
#  ifndef _WIN32_WINNT
#   define _WIN32_WINNT 0x0501
#  endif
#  ifdef HAVE_WS2VISTA
#   include <Wsk.h>
#  else
#   include <winsock2.h>
#   include <ws2tcpip.h>
#  endif
#  define EAFNOSUPPORT 7955
# else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
# endif
# include "other.h"

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
#  ifndef HAVE_SA_LEN
typedef unsigned short sa_family_t;
#  else
typedef unsigned char sa_family_t;
#  endif
# endif
# ifndef HAVE_IN_PORT_T
typedef uint16_t in_port_t;
# endif
# ifndef HAVE_IN_ADDR_T
typedef uint32_t in_addr_t;
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
#  ifdef HAVE_SA_LEN
	unsigned char sin6_len;
#  endif
	sa_family_t sin6_family;
	in_port_t sin6_port;       /* Transport layer port # */
	uint32_t sin6_flowinfo;    /* IPv6 flow information */
	struct in6_addr sin6_addr; /* IPv6 address */
	uint32_t sin6_scope_id;    /* IPv6 scope-id */
}

/* general match */
#  define IN6_ARE_ADDR_EQUAL(a,b) \
	((((const uint32_t *)(a))[0] == ((const uint32_t *)(b))[0]) && \
	 (((const uint32_t *)(a))[1] == ((const uint32_t *)(b))[1]) && \
	 (((const uint32_t *)(a))[2] == ((const uint32_t *)(b))[2]) &&  \
	 (((const uint32_t *)(a))[3] == ((const uint32_t *)(b))[3]))
/* special addr match */
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
# endif /* HAVE_IPV6 */

/*
 * other Special Addr matcher
 */

/* RFC4193
 * instead to test for fc::/7 it's faster to test for both
 * possible vals, fc::/8 && fd::/8
 */
# define IN6_IS_ADDR_UNIQUELOCAL_A(a) \
	(((const uint8_t *)(a))[0] == 0xfc)
# define IN6_IS_ADDR_UNIQUELOCAL_B(a) \
	(((const uint8_t *)(a))[0] == 0xfd)
# define IN6_IS_ADDR_UNIQUELOCAL(a) \
	(IN6_IS_ADDR_UNIQUELOCAL_A(a) || \
	 IN6_IS_ADDR_UNIQUELOCAL_B(a))
/* RFC6666
 * Discard-Only Addr block, this way you can anounce
 * a remote end to discard paket, say in case of DDoS.
 * We want to honor it.
 */
# define IN6_IS_ADDR_DISCARD(a) \
	 (((const uint32_t *)(a))[0] == htonl(0x01000000) && \
	  ((const uint32_t *)(a))[1] == 0)
/* RFC2928
 * IETF Protocol Assignments 2001::/23, like 192.0.0.0/24
 * We generaly do not want to talk to these very special
 * Networks/Services:
 * -> 2001::/32	TEREDO RFC4380 -> Ok, this one we throw under the bus
 * -> 2001:1::1/128	Port Control Protocol Anycast RFC7723
 * -> 2001:2::/48	Benchmark-Network RFC5180
 * -> 2001:3::/32	AMT RFC7450 Multicast-Unicast-Tunnel relays anycast assingment
 * -> 2001:4:112::/48	AS112-v6 RFC7535
 * -> 2001:5::/32	EID Space for LISP (Managed by RIPE NCC)
 * -> 2001:10::/28	ORCHID RFC4843 Deprecated
 * -> 2001:20::/28	ORCHIDv2 RFC7343
 *
 * ORCHID(v2) is some wonky non routable, but still valid IPv6
 * addr. Like a hash to uniquely identify a host.
 * Applications can pass it into the socket API, as identifier
 * to magically address and reach the other end. But both
 * ends first have to agree on some kind of mapping/translation.
 * So when we get such an address passed plain (like as an
 * KHL), and that way escaped the translation of the socket
 * api, we can do nothing with it.
 * These Addr are not reachable on the pure IP level without
 * translation (the RFC saying "... they are expected to be routable
 * at an overlay level. Consequently, while they are considered
 * non-routable addresses from the IPv6-layer perspective, all
 * existing IPv6 applications are expected to be able to use them
 * in a manner compatible with current IPv6 addresses.").
 * My Ass: The transaltion may only be valid/known on the orig. host.
 */
#  define IN6_IS_ADDR_IETFBLOCK(a) \
	((((const uint32_t *)(a))[0] & htonl(0xfffffe00)) == \
	  htonl(0x20010000))
/* RFC3849
 * official documentation prefix
 */
# define IN6_IS_ADDR_DOCU(a) \
	(((const uint32_t *)(a))[0] == htonl(0x20010DB8))
/* RFC7534
 * AS112 delegation, local addr DNS sink hole
 */
# define IN6_IS_ADDR_AS112(a) \
	((((const uint32_t *)(a))[0] == htonl(0x2620004f)) && \
	 ((((const uint32_t *)(a))[0] & htonl(0xffff0000)) == \
	  htonl(0x80000000)))
/* RFC6052
 * well known prefix for BEHAVE draft/IPv4-IPv6 Translat.
 * There can/will be provider specific prefixes for
 * BEHAVE translation, which would need similar
 * treatment...
 */
# define IN6_IS_ADDR_V4BEHAVE(a) \
	((((const uint32_t *)(a))[0] == 0) && \
	 (((const uint32_t *)(a))[1] == 0) && \
	 (((const uint32_t *)(a))[2] == htonl(0x0064FF9B)))

# if HAVE_DECL_INET6_ADDRSTRLEN != 1
/*
 * This buffersize is needed, but we don't have it everywere, even on
 * Systems that claim to be IPv6 capable...
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
int inet_pton(int af, const char *src, void *dst);
# endif
char *inet_ntop_c(int af, const void *src, char *dst, socklen_t cnt) GCC_ATTR_VIS("hidden");
char *inet_ntop_c_rev(int af, const void *src, char *dst) GCC_ATTR_VIS("hidden");

/*
 * Comboaddress to represent IPv4 & IPv6
 *
 * some "new" RFC (2553) defines struct sockaddr_storage
 * for this, but as it is "new", not all systems have it.
 * Esp. some systems have given it the size to store all
 * possible sockaddr_, like AF_UNIX, which is heavy on the
 * memusage...
 * So roll our own.
 */
union combo_addr
{
	/*
	 * dirty deads done dirt cheap,
	 * sa_fam should alias with the first member
	 * off all struct: *_family
	 */
	struct
	{
#ifdef HAVE_SA_LEN
		/*
		 * except on some systems, notably BSD.
		 * They decided it would be better to have a length in
		 * front of all that, and it must be properly set and
		 * so on.
		 * A len in front is generally a good idea i support,
		 * but in this case, idiotic:
		 * The socklen_t arg to all those socket APIs is for
		 * what again?
		 * NB: should have better been the second member, fam
		 * beeing the first to distinct the class...
		 */
		unsigned char       len;
#endif
		sa_family_t         fam;
	} s;
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
	 * At least on glibc/linux. Other systems define just the
	 * bare minimum?
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

struct combo_addr_arr
{
	union combo_addr *a;
	unsigned num;
};

static inline struct sockaddr *casa(union combo_addr *in)
{
	return (struct sockaddr *)in;
}

static inline const struct sockaddr *casac(const union combo_addr *in)
{
	return (const struct sockaddr *)in;
}

static inline size_t casalen(const union combo_addr *in)
{
	return AF_INET == in->s.fam ? sizeof(in->in) : sizeof(in->in6);
}

static inline void casalen_ib(union combo_addr *in GCC_ATTR_UNUSED_PARAM)
{
#ifdef HAVE_SA_LEN
	in->s.len = sizeof(in);
#endif
}

static inline void casalen_ii(union combo_addr *in GCC_ATTR_UNUSED_PARAM)
{
#ifdef HAVE_SA_LEN
	in->s.len = casalen(in);
#endif
}

static inline in_port_t combo_addr_port(const union combo_addr *addr)
{
// TODO: when IPv6 is common, change it
	return likely(AF_INET == addr->s.fam) ? addr->in.sin_port : addr->in6.sin6_port;
}

static inline void combo_addr_set_port(union combo_addr *addr, in_port_t port)
{
// TODO: when IPv6 is common, change it
	if(likely(AF_INET == addr->s.fam))
		addr->in.sin_port = port;
	else
		addr->in6.sin6_port = port;
}


static inline const char *combo_addr_print(const union combo_addr *src, char *dst, socklen_t cnt)
{
	return inet_ntop(src->s.fam,
		likely(AF_INET == src->s.fam) ? (const void *)&src->in.sin_addr : (const void *)&src->in6.sin6_addr,
		dst, cnt);
}

static inline char *combo_addr_print_c(const union combo_addr *src, char *dst, socklen_t cnt)
{
	return inet_ntop_c(src->s.fam,
		likely(AF_INET == src->s.fam) ? (const void *)&src->in.sin_addr : (const void *)&src->in6.sin6_addr,
		dst, cnt);
}

static inline char *combo_addr_print_c_rev(const union combo_addr *src, char *dst)
{
	return inet_ntop_c_rev(src->s.fam,
		likely(AF_INET == src->s.fam) ? (const void *)&src->in.sin_addr : (const void *)&src->in6.sin6_addr,
		dst);
}


static inline int combo_addr_read(const char *src, union combo_addr *dst)
{
	int ret_val;

	ret_val = inet_pton(AF_INET, src, &dst->in.sin_addr);
	if(0 < ret_val) {
		dst->s.fam = AF_INET;
		casalen_ii(dst);
	}
	else if(0 == ret_val)
	{
		ret_val = inet_pton(AF_INET6, src, &dst->in6.sin6_addr);
		if(0 < ret_val) {
			dst->s.fam = AF_INET6;
			casalen_ii(dst);
		}
	}
	return ret_val;
}

bool combo_addr_read_wport(char *str, union combo_addr *addr) GCC_ATTR_VIS("hidden");
bool combo_addr_is_forbidden(const union combo_addr *addr) GCC_ATTR_VIS("hidden");
bool combo_addr_is_public(const union combo_addr *addr) GCC_ATTR_VIS("hidden");
uint32_t combo_addr_hash(const union combo_addr *addr, uint32_t seed) GCC_ATTR_VIS("hidden");
uint32_t combo_addr_hash_extra(const union combo_addr *addr, uint32_t extra, uint32_t seed) GCC_ATTR_VIS("hidden");
uint32_t combo_addr_hash_ip(const union combo_addr *addr, uint32_t seed) GCC_ATTR_VIS("hidden");

static inline bool combo_addr_is_v6(const union combo_addr *addr)
{
	return addr->s.fam == AF_INET6 &&
	       !(IN6_IS_ADDR_V4MAPPED(&addr->in6.sin6_addr) ||
	         IN6_IS_ADDR_V4COMPAT(&addr->in6.sin6_addr));
}

static inline bool combo_addr_eq(const union combo_addr *a, const union combo_addr *b)
{
	bool ret = a->s.fam == b->s.fam;
	if(!ret)
		return ret;
// TODO: when IPv6 is common, change it
	if(likely(AF_INET == a->s.fam)) {
		return a->in.sin_addr.s_addr == b->in.sin_addr.s_addr &&
		       a->in.sin_port == b->in.sin_port;
	} else {
		return !!IN6_ARE_ADDR_EQUAL(&a->in6.sin6_addr, &b->in6.sin6_addr) &&
		       a->in6.sin6_port == b->in6.sin6_port;
	}
}

static inline bool combo_addr_eq_ip(const union combo_addr *a, const union combo_addr *b)
{
	bool ret = a->s.fam == b->s.fam;
	if(!ret)
		return ret;
// TODO: when IPv6 is common, change it
	if(likely(AF_INET == a->s.fam)) {
		return a->in.sin_addr.s_addr == b->in.sin_addr.s_addr;
	} else {
		return !!IN6_ARE_ADDR_EQUAL(&a->in6.sin6_addr, &b->in6.sin6_addr);
	}
}

static inline bool combo_addr_eq_any(const union combo_addr *a)
{
// TODO: when IPv6 is common, change it
	if(likely(AF_INET == a->s.fam)) {
		return INADDR_ANY == a->in.sin_addr.s_addr;
	} else {
		return 0 == a->in6.sin6_addr.s6_addr32[0] && 0 == a->in6.sin6_addr.s6_addr32[1] &&
		       0 == a->in6.sin6_addr.s6_addr32[2] && 0 == a->in6.sin6_addr.s6_addr32[3];
	}
}

static inline int combo_addr_cmp(const union combo_addr *a, const union combo_addr *b)
{
	int ret;
	if((ret = (int)a->s.fam - (int)b->s.fam))
		return ret;
	if(likely(AF_INET == a->s.fam))
	{
		if((ret = (long)a->in.sin_addr.s_addr - (long)b->in.sin_addr.s_addr))
			return ret;
		return (int)a->in.sin_port - (int)b->in.sin_port;
	}
	else
	{
		if((ret = (long)a->in6.sin6_addr.s6_addr32[0] - (long)b->in6.sin6_addr.s6_addr32[0]))
			return ret;
		if((ret = (long)a->in6.sin6_addr.s6_addr32[1] - (long)b->in6.sin6_addr.s6_addr32[1]))
			return ret;
		if((ret = (long)a->in6.sin6_addr.s6_addr32[2] - (long)b->in6.sin6_addr.s6_addr32[2]))
			return ret;
		if((ret = (long)a->in6.sin6_addr.s6_addr32[3] - (long)b->in6.sin6_addr.s6_addr32[3]))
			return ret;
		return (int)a->in6.sin6_port - (int)b->in6.sin6_port;
	}
}

static inline unsigned combo_addr_lin(uint32_t *buf, const union combo_addr *a)
{
// TODO: when ipv6 is common, change it
	if(likely(AF_INET == a->s.fam))
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
	if(likely(AF_INET == a->s.fam))
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
