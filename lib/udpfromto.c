/*
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 *  Helper functions to get/set addresses of UDP packets
 *  based on recvfromto by Miquel van Smoorenburg
 *
 * recvfromto	Like recvfrom, but also stores the destination
 *		IP address. Useful on multihomed hosts.
 *
 *		Should work on Linux and BSD.
 *
 *		Copyright (C) 2002 Miquel van Smoorenburg.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU Lesser General Public
 *		License as published by the Free Software Foundation; either
 *		version 2 of the License, or (at your option) any later version.
 *
 * sendfromto	added 18/08/2003, Jan Berkel <jan@sitadelle.com>
 *		Works on Linux and FreeBSD (5.x)
 *
 * IPv6 and changes etc. (C) 2009 Jan Seiffert
 *
 * Version: $Id: udpfromto.c,v 1.4.4.1 2006/03/15 15:37:58 nbk Exp $
 */

#include "../config.h"
/* System includes */
#include <sys/types.h>
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
/* own */
#include "udpfromto.h"
#include "combo_addr.h"

#if HAVE_DECL_IP_RECVDSTADDR == 1
# ifndef IPV6_RECVDSTADDR
#  define IPV6_RECVDSTADDR IP_RECVDSTADDR
# endif
#endif
#if HAVE_DECL_IP_SENDSRCADDR == 1
# ifndef IPV6_SENDSRCADDR
#  define IPV6_SENDSRCADDR IP_SENDSRCADDR
# endif
#endif

#ifndef SOL_IP
# define SOL_IP IPPROTO_IP
#endif

#ifndef SOL_IPV6
# define SOL_IPV6 IPPROTO_IPV6
#endif

#if !defined(CMSG_LEN) || !defined(CMSG_SPACE)
# define __CMSG_ALIGN(p) (((unsigned)(p) + sizeof(int)) & ~sizeof(int))
# ifndef CMSG_LEN
#  define CMSG_LEN(len)   (__CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))
# endif
# ifndef CMSG_SPACE
#  define CMSG_SPACE(len) (__CMSG_ALIGN(sizeof(struct cmsghdr)) + __CMSG_ALIGN(len))
# endif
#endif

#ifdef HAVE_IP6_PKTINFO
static int v6pktinfo;
#endif

static int init_v4(int s, int fam GCC_ATTR_UNUSED_PARAM)
{
	int err = -1, opt = 1;

#ifdef HAVE_IP_PKTINFO
	/* Set the IP_PKTINFO option (Linux). */
	err = setsockopt(s, SOL_IP, IP_PKTINFO, &opt, sizeof(opt));
#elif HAVE_DECL_IP_RECVDSTADDR == 1
	/*
	 * Set the IP_RECVDSTADDR option (BSD).
	 * Note: IP_RECVDSTADDR == IP_SENDSRCADDR
	 */
	err = setsockopt(s, IPPROTO_IP, IP_RECVDSTADDR, &opt, sizeof(opt));
#elif HAVE_DECL_IP_RECVIF == 1
	err = setsockopt(s, IPPROTO_IP, IP_RECVIF, &opt, sizeof(opt));
#else
	opt = opt;
	s = s;
	fam = fam;
#endif
	return err;
}

static int init_v6(int s, int fam GCC_ATTR_UNUSED_PARAM)
{
	int err = -1, opt = 1;

#ifdef HAVE_IP6_PKTINFO
	/* Set the IP6_PKTINFO option (Linux).
	 *
	 * looks like they changed the name/ABI/API 3 times?
	 * see down below. in_pktinfo was linux local, but the
	 * v6 version became an RFC, but with another name
	 * (and value?).
	 */
# ifdef IPV6_RECVPKTINFO
	v6pktinfo = IPV6_RECVPKTINFO;
	err = setsockopt(s, SOL_IPV6, IPV6_RECVPKTINFO, &opt, sizeof(opt));
#  ifdef IPV6_2292PKTINFO
	if(-1 == err && ENOPROTOOPT == errno) {
		if(-1 != (err = setsockopt(s, SOL_IPV6, IPV6_2292PKTINFO, &opt, sizeof(opt))))
			v6pktinfo = IPV6_2292PKTINFO;
	}
#  endif
# else
	v6pktinfo = IPV6_PKTINFO;
	err = setsockopt(s, SOL_IPV6, IPV6_PKTINFO, &opt, sizeof(opt));
# endif
#elif HAVE_DECL_IP_RECVDSTADDR == 1
	/*
	 * Set the IP_RECVDSTADDR option (BSD).
	 * Note: IP_RECVDSTADDR == IP_SENDSRCADDR
	 *
	 * they say it is deprecated, since in6_pktinfo is now 
	 * an RFC. So KAME nuked it...
	 * They don't have IP_PKTINFO vor v4, but IPV6_PKTINFO.
	 * Give me a break...
	 */
	err = setsockopt(s, IPPROTO_IPV6, IPV6_RECVDSTADDR, &opt, sizeof(opt));
#elif HAVE_DECL_IP_RECVIF == 1
// TODO: does this also work with IPv6?
	/* since in6_pktinfo is rfc ... */
#else
	opt = opt;
	s = s;
	fam = fam;
#endif
	return err;
}

int udpfromto_init(int s, int fam)
{
	errno = ENOSYS;

	if(AF_INET == fam)
		return init_v4(s, fam);
	else
		return init_v6(s, fam);
}

ssize_t recvfromto(int s, void *buf, size_t len, int flags,
                   struct sockaddr *from, socklen_t *fromlen,
                   struct sockaddr *to, socklen_t *tolen)
{
#if defined(HAVE_IP_PKTINFO) || defined(HAVE_IP6_PKTINFO) || HAVE_DECL_IP_RECVDSTADDR == 1
	struct msghdr msgh;
	struct iovec iov;
	struct cmsghdr *cmsg;
	char cbuf[256] GCC_ATTR_ALIGNED(sizeof(size_t));
#endif
	ssize_t err;

	/*
	 * IP_PKTINFO / IP_RECVDSTADDR don't provide sin_port so we have to
	 * retrieve it using getsockname(). Even when we can not receive the
	 * sender, we have to provide something.
	 * This also "primes" the buffer.
	 */
	if(to && (err = getsockname(s, to, tolen)) < 0)
		return err;

#if defined(HAVE_IP_PKTINFO) || defined(HAVE_IP6_PKTINFO) || HAVE_DECL_IP_RECVDSTADDR == 1
	/* Set up iov and msgh structures. */
	memset(&msgh, 0, sizeof(msgh));
	iov.iov_base        = buf;
	iov.iov_len         = len;
	msgh.msg_control    = cbuf;
	msgh.msg_controllen = sizeof(cbuf);
	msgh.msg_name       = from;
	msgh.msg_namelen    = fromlen ? *fromlen : 0;
	msgh.msg_iov        = &iov;
	msgh.msg_iovlen     = 1;
	msgh.msg_flags      = 0;

	/* Receive one packet. */
	err = recvmsg(s, &msgh, flags);
	if(err < 0)
		return err;

	if(fromlen)
		*fromlen = msgh.msg_namelen;

	if(!to)
		return err;

	/* Process auxiliary received data in msgh */
	for(cmsg = CMSG_FIRSTHDR(&msgh); cmsg; cmsg = CMSG_NXTHDR(&msgh, cmsg))
	{
	/* IPv4 */
# ifdef HAVE_IP_PKTINFO
// TODO: change when IPv6 is big
		if(likely(SOL_IP      == cmsg->cmsg_level &&
		           IP_PKTINFO == cmsg->cmsg_type))
		{
			struct sockaddr_in *toi = (struct sockaddr_in *)to;
			struct in_pktinfo *ip = (struct in_pktinfo *)CMSG_DATA(cmsg);

			toi->sin_family = AF_INET;
			toi->sin_addr = ip->ipi_addr;
			*tolen = sizeof(*toi);
			break;
		}
# elif HAVE_DECL_IP_RECVDSTADDR == 1
// TODO: May not work with Solaris?
		/*
		 * (old? 2.6 (kernel? System?)) Solaris does not seem to use
		 * the control msg to return the to-address, but msg_accrights?
		 */
// TODO: does RECVDSTADDR return a port?
		/* Or only on old Solaris? */
		if(IPPROTO_IP     == cmsg->cmsg_level &&
		   IP_RECVDSTADDR == cmsg->cmsg_type)
		{
			struct sockaddr_in *toi = (struct sockaddr_in *)to;
			struct in_addr *ip = (struct in_addr *)CMSG_DATA(cmsg);

			toi->sin_family = AF_INET;
			toi->sin_addr = *ip;
			*tolen = sizeof(*toi);
			break;
		}
# endif
	/* IPv6 */
# ifdef HAVE_IP6_PKTINFO
		if(SOL_IPV6  == cmsg->cmsg_level &&
		   v6pktinfo == cmsg->cmsg_type)
		{
			struct sockaddr_in6 *toi6 = (struct sockaddr_in6 *)to;
			struct in6_pktinfo *ipv6 = (struct in6_pktinfo *)CMSG_DATA(cmsg);

			toi6->sin6_family = AF_INET6;
			memcpy(&toi6->sin6_addr, &ipv6->ipi6_addr, sizeof(toi6->sin6_addr));
			*tolen = sizeof(*toi6);
			break;
		}
# elif HAVE_DECL_IP_RECVDSTADDR == 1
		/* deprecated */
		if(IPPROTO_IPV6     == cmsg->cmsg_level &&
		   IPV6_RECVDSTADDR == cmsg->cmsg_type)
		{
			struct sockaddr_in6 *toi6 = (struct sockaddr_in6 *)to;
			struct in6_addr *ipv6 = (struct in6_addr *)CMSG_DATA(cmsg);

			toi6->sin6_family = AF_INET6;
			memcpy(&toi6->sin6_addr, ipv6, INET6_ADDRLEN);
			*tolen = sizeof(*toi6);
			break;
		}
# endif
// TODO: if all fails, one can use IP_RECVIF
		/*
		 * this should also work on IPv6 with solaris, but needs
		 * a scan otver the interfaces to get an ip to the index...
		 */
	}
	return err;
#else
	/* fallback: call recvfrom */
	return recvfrom(s, buf, len, flags, from, fromlen);
#endif /* defined(HAVE_IP_PKTINFO) || HAVE_DECL_IP_RECVDSTADDR == 1 */
}

ssize_t sendtofrom(int s, void *buf, size_t len, int flags,
                   struct sockaddr *to, socklen_t tolen,
                   struct sockaddr *from, socklen_t fromlen)
{
#if defined(HAVE_IP_PKTINFO) || defined (HAVE_IP6_PKTINFO) || HAVE_DECL_IP_SENDSRCADDR == 1
	struct msghdr msgh;
	struct cmsghdr *cmsg;
	struct iovec iov;
# if defined(HAVE_IP_PKTINFO) || defined(HAVE_IP6_PKTINFO)
	/* struct in6_pktinfo is larger, so wins */
	char cmsgbuf[CMSG_SPACE(sizeof(struct in6_pktinfo))] GCC_ATTR_ALIGNED(sizeof(size_t));
# elif HAVE_DECL_IP_SENDSRCADDR == 1
	char cmsgbuf[CMSG_SPACE(sizeof(struct in6_addr))] GCC_ATTR_ALIGNED(sizeof(size_t));
# endif

	fromlen = fromlen;

	/* Set up iov and msgh structures. */
	memset(&msgh, 0, sizeof(msgh));
	iov.iov_base = buf;
	iov.iov_len = len;
	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = cmsgbuf;
	msgh.msg_controllen = sizeof(cmsgbuf);
	msgh.msg_name = to;
	msgh.msg_namelen = tolen;

	cmsg = CMSG_FIRSTHDR(&msgh);

/* IPv4 */
# ifdef HAVE_IP_PKTINFO
	if(AF_INET == from->sa_family)
	{
		struct sockaddr_in *from_sin = (struct sockaddr_in *)from;
		struct in_pktinfo *pi_ptr;

		cmsg->cmsg_level = SOL_IP;
		cmsg->cmsg_type  = IP_PKTINFO;
		cmsg->cmsg_len   = CMSG_LEN(sizeof(*pi_ptr));
		pi_ptr           = (struct in_pktinfo *)CMSG_DATA(cmsg);
		memset(pi_ptr, 0, sizeof(*pi_ptr));
		memcpy(&pi_ptr->ipi_spec_dst, &from_sin->sin_addr, sizeof(pi_ptr->ipi_spec_dst));
	}
# elif HAVE_DECL_IP_SENDSRCADDR == 1
	if(AF_INET == from->sa_family)
	{
		struct sockaddr_in *from_sin = (struct sockaddr_in *)from;

		cmsg->cmsg_level = IPPROTO_IP;
		cmsg->cmsg_type  = IP_SENDSRCADDR;
		cmsg->cmsg_len   = CMSG_LEN(sizeof(from_sin->sin_addr));
		memcpy(CMSG_DATA(cmsg), &from_sin->sin_addr, sizeof(from_sin->sin_addr));
	}
# endif
/* IPv6 */
# ifdef HAVE_IP6_PKTINFO
	else
	{
		struct sockaddr_in6 *from_sin6 = (struct sockaddr_in6 *)from;
		struct in6_pktinfo *pi6_ptr;

		cmsg->cmsg_level = SOL_IPV6;
// TODO: also set to v6pktinfo?
		cmsg->cmsg_type  = IPV6_PKTINFO;
		cmsg->cmsg_len   = CMSG_LEN(sizeof(*pi6_ptr));
		pi6_ptr          = (struct in6_pktinfo *)CMSG_DATA(cmsg);
		memset(pi6_ptr, 0, sizeof(*pi6_ptr));
// TODO: contains an ifindex, we memset 0 it
		/* interface 0 may not have the right source addr ... */
		memcpy(&pi6_ptr->ipi6_addr, &from_sin6->sin6_addr, sizeof(pi6_ptr->ipi6_addr));
	}
# elif HAVE_DECL_IP_SENDSRCADDR == 1
	else
	{
		struct sockaddr_in6 *from_sin6 = (struct sockaddr_in6 *)from;

		cmsg->cmsg_level = IPPROTO_IPV6;
		cmsg->cmsg_type  = IPV6_SENDSRCADDR;
		cmsg->cmsg_len   = CMSG_LEN(sizeof(from_sin6->sin6_addr));
		memcpy(CMSG_DATA(cmsg), &from_sin6->sin6_addr, sizeof(from_sin6->sin6_addr));
	}
# endif

	return sendmsg(s, &msgh, flags);
#else
	/* fallback: call sendto() */
	from = from;
	fromlen = fromlen;
	return sendto(s, buf, len, flags, to, tolen);
#endif	/* defined(HAVE_IP_PKTINFO) || defined (HAVE_IP6_PKTINFO) || HAVE_DECL_IP_SENDSRCADDR == 1 */
}

/*@unused@*/
static const char rcsid_uft[] GCC_ATTR_USED_VAR = "$Id: udpfromto.c,v 1.4.4.1 2006/03/15 15:37:58 nbk Exp $";
/* EOF */
