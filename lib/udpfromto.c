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
 * IPv6 and changes etc. (C) 2009-2011 Jan Seiffert
 *
 * Version: $Id: udpfromto.c,v 1.4.4.1 2006/03/15 15:37:58 nbk Exp $
 */

#ifdef __APPLE__
/* Define before netinet/in.h for IPV6_PKTINFO */
# define __APPLE_USE_RFC_3542
/* there is also a RFC_2292 whihc makes PKTINFO
 * compatible with the older RFC */
#endif
#include "../config.h"
#include <stdlib.h>
#include <stddef.h>
/* System includes */
#include <sys/types.h>
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#ifdef DRD_ME
# include <valgrind/drd.h>
#endif
/* own */
#include "udpfromto.h"
#include "combo_addr.h"
#include "my_epoll.h"

/*
 * First we will have to redefine everything and the kitchen sink.
 * This is highly non portable, so to get this code compiling
 * n(did i say working???), we have to turn some things to our favour.
 * Some fallback and magic in here so it may also work, hopefully...
 */

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

#if defined(HAVE_IP6_PKTINFO)
# define STRUCT_SIZE (sizeof(struct in6_pktinfo))
#elif defined(HAVE_IP_PKTINFO)
# define STRUCT_SIZE (sizeof(struct in_pktinfo))
#elif HAVE_DECL_IP_RECVDSTADDR == 1
# ifdef HAVE_IPV6
#  define STRUCT_SIZE (sizeof(struct in6_addr))
# else
#  define STRUCT_SIZE (sizeof(struct in_addr))
# endif
#else
# define STRUCT_SIZE (256)
#endif

#ifdef WIN32
# define set_s_errno(x) (WSASetLastError(x))
# define s_errno WSAGetLastError()
/* broken mingw header... */
# ifdef WSA_CMSG_FIRSTHDR
#  define CMSG_FIRSTHDR(x) WSA_CMSG_FIRSTHDR(x)
# endif
# ifdef WSA_CMSG_NXTHDR
#  define CMSG_NXTHDR(x, y) WSA_CMSG_NXTHDR(x, y)
# endif
# ifdef WSA_CMSG_DATA
#  define CMSG_DATA(x) WSA_CMSG_DATA(x)
# endif
# ifdef WSA_CMSG_LEN
#  define CMSG_LEN(x) WSA_CMSG_LEN(x)
# endif
# ifdef WSA_CMSG_SPACE
#  define CMSG_SPACE(x) WSA_CMSG_SPACE(x)
# endif
# ifdef WSA_CMSG_FIRSTHDR
#  define CMSG_FIRSTHDR(x) WSA_CMSG_FIRSTHDR(x)
# endif
#else
# define set_s_errno(x) (errno = (x))
# define s_errno errno
typedef struct msghdr my_msghdr;
typedef struct cmsghdr my_cmsghdr;
#endif

/*
 * Generations of unix have piled up a variety of missing funcs.
 * Try to fill in with the most propably fitting code.
 * This is guesswork, and has a high chance to break something...
 */
#ifndef CMSG_FIRSTHDR
# define CMSG_FIRSTHDR(mhdr) \
	((size_t)(mhdr)->msg_controllen >= sizeof(my_cmsghdr) ? \
	 (my_cmsghdr *)(mhdr)->msg_control : \
	 (my_cmsghdr *)0)
#endif
#if !defined(CMSG_LEN) || !defined(CMSG_SPACE) || !defined(CMSG_DATA) || !defined(CMSG_NXTHDR)
# define __CMSG_ALIGN(p) (((unsigned)(p) + sizeof(int) - 1) & (~(sizeof(int) - 1)))
# ifndef CMSG_LEN
#  define CMSG_LEN(len)   (__CMSG_ALIGN(sizeof(my_cmsghdr)) + (len))
# endif
# ifndef CMSG_SPACE
#  define CMSG_SPACE(len) (__CMSG_ALIGN(sizeof(my_cmsghdr)) + __CMSG_ALIGN(len))
# endif
# ifndef CMSG_DATA
#  define CMSG_DATA(cmsg) ((unsigned char *)(cmsg) + __CMSG_ALIGN(sizeof(my_cmsghdr)))
# endif
# ifndef CMSG_NXTHDR
#  define CMSG_NXTHDR(mhdr, cmsg) __cmsg_nxthdr(mhdr, cmsg)
static my_cmsghdr *__cmsg_nxthdr(my_msghdr *mhdr, my_cmsghdr *cmsg)
{
	if(!cmsg)
		return CMSG_FIRSTHDR(mhdr);

	cmsg = (my_cmsghdr *)((char *)cmsg + __CMSG_ALIGN(cmsg->cmsg_len));
	if((char *)cmsg + sizeof(my_cmsghdr) >  (char *)mhdr->msg_control + mhdr->msg_controllen)
		return NULL;
	return cmsg;
}
# endif
#endif

#ifdef HAVE_IP6_PKTINFO
static int v6pktinfo;
#endif

#if defined(HAVE_IP_PKTINFO) || defined(HAVE_IP6_PKTINFO) || HAVE_DECL_IP_RECVDSTADDR == 1
# define MAX_RECVMMSG 128
struct msg_thread_space
{
#  ifdef HAVE_RECVMMSG
	struct mmsghdr msghv[MAX_RECVMMSG];
#  else
	my_msghdr msghv[MAX_RECVMMSG];
#  endif
	char cbuf[MAX_RECVMMSG][CMSG_SPACE(STRUCT_SIZE)] GCC_ATTR_ALIGNED(sizeof(size_t));
};

# ifndef HAVE___THREAD
static pthread_key_t key2msg_space;
static void recvmmsg_init(void) GCC_ATTR_CONSTRUCT;
static void recvmmsg_deinit(void) GCC_ATTR_DESTRUCT;

static __init void recvmmsg_init(void)
{
	size_t i;
	if(pthread_key_create(&key2msg_space, free)) {
		fprintf(stderr, __FILE__ ": couln't create TLS key for recvmmsg buffer");
		exit(EXIT_FAILURE);
	}
}

static void recvmmsg_deinit(void)
{
	struct msg_thread_space *mts;

	if((mts = pthread_getspecific(key2msg_space))) {
		free(mts);
		pthread_setspecific(key2msg_space, NULL);
	}
	pthread_key_delete(key2msg_space);
}
# else
static __thread struct msg_thread_space *msg_thread_space;
# endif

static noinline struct msg_thread_space *mts_get_internal(void)
{
	struct msg_thread_space *mts;

	mts = malloc(sizeof(*mts));
	if(!mts)
		return NULL;
# ifndef HAVE___THREAD
	pthread_setspecific(key2msg_space, mts);
# else
	msg_thread_space = mts;
# endif
	return mts;
}

static struct msg_thread_space *mts_get(void)
{
	struct msg_thread_space *mts;
# ifndef HAVE___THREAD
	mts = pthread_getspecific(key2msg_space);
# else
	mts = msg_thread_space;
# endif
	if(unlikely(!mts))
		return mts_get_internal();

	return mts;
}
#endif

/*
 * OK
 *
 * Now to the init funcs, to hide runtime foo
 */
static int init_v4(int s, int fam GCC_ATTR_UNUSED_PARAM)
{
	int err = -1, opt = 1;

#ifdef HAVE_IP_PKTINFO
	/* Set the IP_PKTINFO option (Linux). */
	err = setsockopt(s, SOL_IP, IP_PKTINFO, (void *)&opt, sizeof(opt));
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
# ifdef DRD_ME
	DRD_IGNORE_VAR(v6pktinfo);
# endif
	v6pktinfo = IPV6_RECVPKTINFO;
	err = setsockopt(s, SOL_IPV6, IPV6_RECVPKTINFO, &opt, sizeof(opt));
#  ifdef IPV6_2292PKTINFO
	if(-1 == err && ENOPROTOOPT == s_errno) {
		if(-1 != (err = setsockopt(s, SOL_IPV6, IPV6_2292PKTINFO, &opt, sizeof(opt))))
			v6pktinfo = IPV6_2292PKTINFO;
	}
#  endif
# else
	v6pktinfo = IPV6_PKTINFO;
	err = setsockopt(s, SOL_IPV6, IPV6_PKTINFO, (void *)&opt, sizeof(opt));
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
	set_s_errno(ENOSYS);

	if(AF_INET == fam)
		return init_v4(s, fam);
	else
		return init_v6(s, fam);
}


/*
 *
 * And finally, the funcs
 *
 *
 */
ssize_t recvmfromto_pre(int s, struct mfromto *info, size_t len, int flags)
{
	struct msg_thread_space *mts;
	ssize_t err;
	unsigned i;

	len = likely(len <= MAX_RECVMMSG) ? len : MAX_RECVMMSG;
	/*
	 * IP_PKTINFO / IP_RECVDSTADDR don't provide sin_port so we have to
	 * retrieve it using getsockname(). Even when we can not receive the
	 * sender, we have to provide something.
	 * This also "primes" the buffer.
	 */
	if(unlikely((err = getsockname(s, info[0].to, &info[0].to_len)) < 0))
		return err;

// TODO: test new recvmmsg
#if defined(HAVE_IP_PKTINFO) || defined(HAVE_IP6_PKTINFO) || HAVE_DECL_IP_RECVDSTADDR == 1
	mts = mts_get();
	if(!mts)
		return -1;
# ifdef HAVE_RECVMMSG
	memset(mts->msghv, 0, sizeof(mts->msghv[0]) * len);
	for(i = 1; i < len; i++) {
		memcpy(info[i].to, info[0].to, sizeof(*info[0].to));
		info[i].to_len = info[0].to_len;
	}
// TODO: move part of this at post end
	for(i = 0; i < len; i++)
	{
		mts->msghv[i].msg_hdr.msg_control    = mts->cbuf[i];
		mts->msghv[i].msg_hdr.msg_controllen = sizeof(mts->cbuf[0]);
		mts->msghv[i].msg_hdr.msg_name       =  info[i].from;
		mts->msghv[i].msg_hdr.msg_namelen    =  info[i].from_len;
		mts->msghv[i].msg_hdr.msg_iov        = &info[i].iov;
		mts->msghv[i].msg_hdr.msg_iovlen     = 1;
		mts->msghv[i].msg_hdr.msg_flags      = flags;
	}

	err = recvmmsg(s, mts->msghv, len, 0, NULL);
	if(-1 == err) {
		if(EAGAIN == s_errno || EWOULDBLOCK == s_errno)
			err = 0;
		i = 0;
	} else
		i = (unsigned)err;
# else
	for(i = 0; i < len; i++)
	{
		memset(&mts->msghv[i], 0, sizeof(mts->msghv[0]));
		mts->msghv[i].msg_control    = mts->cbuf[i];
		mts->msghv[i].msg_controllen = sizeof(mts->cbuf[0]);
		mts->msghv[i].msg_name       =  info[i].from;
		mts->msghv[i].msg_namelen    =  info[i].from_len;
		mts->msghv[i].msg_iov        = &info[i].iov;
		mts->msghv[i].msg_iovlen     = 1;
		mts->msghv[i].msg_flags      = flags;
		if(i > 0) {
			memcpy(info[i].to, info[0].to, sizeof(*info[0].to));
			info[i].to_len = info[0].to_len;
		}

		/* Receive one packet. */
		err = my_epoll_recvmsg(s, &mts->msghv[i], 0);
		if(err > 0) {
			info[i].iov.iov_len = err;
		} else {
			if(EAGAIN == s_errno || EWOULDBLOCK == s_errno)
				err = 0;
			break;
		}
	}
# endif
#else
	for(i = 0; i < len; i++)
	{
		if(i > 0) {
			memcpy(info[i].to, info[0].to, sizeof(*info[0].to));
			info[i].to_len = info[0].to_len;
		}
		/* fallback: call recvfrom */
		err = recvfrom(s, info[i].iov.iov_base, info[i].iov.iov_len, flags, info[i].from, &info[i].fromlen);
		if(err > 0) {
			info[i].iov.iov_len = err;
		} else {
			if(EAGAIN == s_errno || EWOULDBLOCK == s_errno)
				err = 0;
			break;
		}
	}
#endif

	return err >= 0 ? i : -((ssize_t)(i + 1));
}

void recvmfromto_post(struct mfromto *info, size_t len)
{
#if defined(HAVE_IP_PKTINFO) || defined(HAVE_IP6_PKTINFO) || HAVE_DECL_IP_RECVDSTADDR == 1
	struct msg_thread_space *mts;
	my_msghdr *msgh;
	unsigned i;

	mts = mts_get();
	for(i = 0; i < len; i++)
	{
		my_cmsghdr *cmsg;

# ifdef HAVE_RECVMMSG
		info[i].iov.iov_len = mts->msghv[i].msg_len;
		msgh = &mts->msghv[i].msg_hdr;
# else
		msgh = &mts->msghv[i];
# endif
		info[i].from_len = msgh->msg_namelen;

		/* Process auxiliary received data in msgh */
		for(cmsg = CMSG_FIRSTHDR(msgh); cmsg; cmsg = CMSG_NXTHDR(msgh, cmsg))
		{
		/* IPv4 */
# ifdef HAVE_IP_PKTINFO
	// TODO: change when IPv6 is big
			if(likely(SOL_IP      == cmsg->cmsg_level &&
						  IP_PKTINFO == cmsg->cmsg_type))
			{
				struct sockaddr_in *toi = (struct sockaddr_in *)info[i].to;
				struct in_pktinfo *ip = (struct in_pktinfo *)CMSG_DATA(cmsg);

				toi->sin_family = AF_INET;
				toi->sin_addr = ip->ipi_addr;
#ifdef HAVE_SA_LEN
				toi->sin_len =
#endif
				info[i].to_len = sizeof(*toi);
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
				struct sockaddr_in *toi = (struct sockaddr_in *)info[i].to;
				struct in_addr *ip = (struct in_addr *)CMSG_DATA(cmsg);

				toi->sin_family = AF_INET;
				toi->sin_addr = *ip;
#ifdef HAVE_SA_LEN
				toi->sin_len =
#endif
				info[i].to_len = sizeof(*toi);
				break;
			}
# endif
		/* IPv6 */
# ifdef HAVE_IP6_PKTINFO
			if(SOL_IPV6  == cmsg->cmsg_level &&
				v6pktinfo == cmsg->cmsg_type)
			{
				struct sockaddr_in6 *toi6 = (struct sockaddr_in6 *)info[i].to;
				struct in6_pktinfo *ipv6 = (struct in6_pktinfo *)CMSG_DATA(cmsg);

				toi6->sin6_family = AF_INET6;
				memcpy(&toi6->sin6_addr, &ipv6->ipi6_addr, sizeof(toi6->sin6_addr));
#ifdef HAVE_SA_LEN
				toi6->sin_len =
#endif
				info[i].to_len = sizeof(*toi6);
				break;
			}
# elif HAVE_DECL_IP_RECVDSTADDR == 1
			/* deprecated */
			if(IPPROTO_IPV6     == cmsg->cmsg_level &&
				IPV6_RECVDSTADDR == cmsg->cmsg_type)
			{
				struct sockaddr_in6 *toi6 = (struct sockaddr_in6 *)info[i].to;
				struct in6_addr *ipv6 = (struct in6_addr *)CMSG_DATA(cmsg);

				toi6->sin6_family = AF_INET6;
				memcpy(&toi6->sin6_addr, ipv6, INET6_ADDRLEN);
#ifdef HAVE_SA_LEN
				toi6->sin_len =
#endif
				info[i].to_len = sizeof(*toi6);
				break;
			}
# endif
	// TODO: if all fails, one can use IP_RECVIF
			/*
			 * this should also work on IPv6 with solaris, but needs
			 * a scan otver the interfaces to get an ip to the index...
			 */
		}
	}
#endif
}

ssize_t recvmfromto(int s, struct mfromto *info, size_t len, int flags)
{
	ssize_t ret, err = 0;

	ret = err = recvmfromto_pre(s, info, len, flags);
	if(-1 == err)
		return ret;
	if(err < 0)
		err = -(err + 1);
	recvmfromto_post(info, err);
	return ret;
}

ssize_t sendtofrom(int s, void *buf, size_t len, int flags,
                   struct sockaddr *to, socklen_t tolen,
                   struct sockaddr *from, socklen_t fromlen)
{
#if !defined(WIN32) && (defined(HAVE_IP_PKTINFO) || defined (HAVE_IP6_PKTINFO) || HAVE_DECL_IP_SENDSRCADDR == 1)
	my_msghdr msgh;
	my_cmsghdr *cmsg;
	my_iovec iov;
	char cmsgbuf[CMSG_SPACE(STRUCT_SIZE)] GCC_ATTR_ALIGNED(sizeof(size_t));

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

	/* CMSG_SPACE instead of CMSG_LEN here? */
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
#  ifdef HAVE_IP_PKTINFO_DST
		memcpy(&pi_ptr->ipi_spec_dst, &from_sin->sin_addr, sizeof(pi_ptr->ipi_spec_dst));
#  else
		memcpy(&pi_ptr->ipi_addr, &from_sin->sin_addr, sizeof(pi_ptr->ipi_addr));
#  endif
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
	return my_epoll_sendto(s, buf, len, flags, to, tolen);
#endif	/* defined(HAVE_IP_PKTINFO) || defined (HAVE_IP6_PKTINFO) || HAVE_DECL_IP_SENDSRCADDR == 1 */
}

/*@unused@*/
static const char rcsid_uft[] GCC_ATTR_USED_VAR = "$Id: udpfromto.c,v 1.4.4.1 2006/03/15 15:37:58 nbk Exp $";
/* EOF */
