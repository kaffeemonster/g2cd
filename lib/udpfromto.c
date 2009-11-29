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
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
/* own */
#include "udpfromto.h"
#include "combo_addr.h"

#ifdef HAVE_IP_RECVDSTADDR
# ifndef IPV6_RECVDSTADDR
#  define IPV6_RECVDSTADDR IP_RECVDSTADDR
# endif
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

int udpfromto_init(int s, int fam)
{
	int err = -1, opt = 1;
	errno = ENOSYS;

#ifdef HAVE_IP_PKTINFO
	/* Set the IP_PKTINFO option (Linux). */
	if(AF_INET == fam)
		err = setsockopt(s, SOL_IP, IP_PKTINFO, &opt, sizeof(opt));
	else
		err = setsockopt(s, SOL_IPV6, IPV6_RECVPKTINFO, &opt, sizeof(opt));
#elif defined(HAVE_IP_RECVDSTADDR)
	/*
	 * Set the IP_RECVDSTADDR option (BSD).
	 * Note: IP_RECVDSTADDR == IP_SENDSRCADDR
	 */
	if(AF_INET == fam)
		err = setsockopt(s, IPPROTO_IP, IP_RECVDSTADDR, &opt, sizeof(opt));
	else
		err = setsockopt(s, IPPROTO_IPV6, IPV6_RECVDSTADDR, &opt, sizeof(opt));
// TODO: IPv6? Does someone has a bsd at hand?
#elif defined(HAVE_IP_RECVIF)
	if(AF_INET == fam)
		err = setsockopt(s, IPPROTO_IP, IP_RECVIF, &opt, sizeof(opt));
#else
	opt = opt;
	s = s;
	fam = fam;
#endif
	return err;
}

ssize_t recvfromto(int s, void *buf, size_t len, int flags,
                   struct sockaddr *from, socklen_t *fromlen,
                   struct sockaddr *to, socklen_t *tolen)
{
#if defined(HAVE_IP_PKTINFO) || defined(HAVE_IP_RECVDSTADDR)
	struct msghdr msgh;
	struct iovec iov;
	struct cmsghdr *cmsg;
	char cbuf[256];
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

#if defined(HAVE_IP_PKTINFO) || defined(HAVE_IP_RECVDSTADDR)
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
# ifdef HAVE_IP_PKTINFO
		if(SOL_IP      == cmsg->cmsg_level &&
		    IP_PKTINFO == cmsg->cmsg_type)
		{
			struct sockaddr_in *toi = (struct sockaddr_in *)to;
			struct in_pktinfo *ip = (struct in_pktinfo *)CMSG_DATA(cmsg);

			toi->sin_family = AF_INET;
			toi->sin_addr = ip->ipi_addr;
			*tolen = sizeof(*toi);
			break;
		}
		if(SOL_IPV6         == cmsg->cmsg_level &&
		   IPV6_RECVPKTINFO == cmsg->cmsg_type)
		{
			struct sockaddr_in6 *toi6 = (struct sockaddr_in6 *)to;
			struct in6_pktinfo *ipv6 = (struct in6_pktinfo *)CMSG_DATA(cmsg);

			toi6->sin6_family = AF_INET6;
			memcpy(&toi6->sin6_addr, &ipv6->ipi6_addr, sizeof(toi6->sin6_addr));
			*tolen = sizeof(*toi6);
			break;
		}
# elif defined HAVE_IP_RECVDSTADDR
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
		if(IPPROTO_IPV6     == cmsg->cmsg_level &&
		   IPV6_RECVDSTADDR == cmsg->cmsg_type)
		{
// TODO: IPv6?? How does BSD do it
			struct sockaddr_in6 *toi6 = (struct sockaddr_in6 *)to;
			struct in6_addr *ipv6 = (struct in6_addr *)CMSG_DATA(cmsg);

			toi6->sin6_family = AF_INET6;
			memcpy(&toi6->sin6_addr, ipv6, INET6_ADDRLEN);
			*tolen = sizeof(*toi6);
			break;
		}
# endif
	}
	return err;
#else
	/* fallback: call recvfrom */
	return recvfrom(s, buf, len, flags, from, fromlen);
#endif /* defined(HAVE_IP_PKTINFO) || defined(HAVE_IP_RECVDSTADDR) */
}

ssize_t sendtofrom(int s, void *buf, size_t len, int flags,
                   struct sockaddr *to, socklen_t tolen,
                   struct sockaddr *from, socklen_t fromlen)
{
#if defined(HAVE_IP_PKTINFO) || defined(HAVE_IP_SENDSRCADDR)
	struct msghdr msgh;
	struct cmsghdr *cmsg;
	struct iovec iov;
# ifdef HAVE_IP_PKTINFO
	char cmsgbuf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
# elif defined(HAVE_IP_SENDSRCADDR)
	char cmsgbuf[CMSG_SPACE(sizeof(struct in6_addr))];
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
	else
	{
		struct sockaddr_in6 *from_sin6 = (struct sockaddr_in6 *)from;
		struct in6_pktinfo *pi6_ptr;

		cmsg->cmsg_level = SOL_IPV6;
		cmsg->cmsg_type  = IPV6_PKTINFO;
		cmsg->cmsg_len   = CMSG_LEN(sizeof(*pi6_ptr));
		pi6_ptr          = (struct in6_pktinfo *)CMSG_DATA(cmsg);
		memset(pi6_ptr, 0, sizeof(*pi6_ptr));
// TODO: contains an ifindex, we memset 0 it
		/* interface 0 may not have the right source addr ... */
		memcpy(&pi6_ptr->ipi6_addr, &from_sin6->sin6_addr, sizeof(pi6_ptr->ipi6_addr));
	}
# elif defined(HAVE_IP_SENDSRCADDR)
	if(AF_INET == from->sa_family)
	{
		struct sockaddr_in *from_sin = (struct sockaddr_in *)from;

		cmsg->cmsg_level = IPPROTO_IP;
		cmsg->cmsg_type  = IP_SENDSRCADDR;
		cmsg->cmsg_len   = CMSG_LEN(sizeof(from_sin->sin_addr));
		memcpy(CMSG_DATA(cmsg), &from_sin->sin_addr, sizeof(from_sin->sin_addr));
	}
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
#endif	/* defined(HAVE_IP_PKTINFO) || defined (HAVE_IP_SENDSRCADDR) */
}

/*@unused@*/
static const char rcsid_uft[] GCC_ATTR_USED_VAR = "$Id: udpfromto.c,v 1.4.4.1 2006/03/15 15:37:58 nbk Exp $";
/* EOF */
