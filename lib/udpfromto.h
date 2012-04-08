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
 * IPv6 and changes etc. Copyright (C) 2009-2012 Jan Seiffert
 */
#ifndef UDPFROMTO_H
# define UDPFROMTO_H

# include "combo_addr.h"
# include "other.h"

int udpfromto_init(some_fd s, int fam) GCC_ATTR_VIS("hidden");
ssize_t sendtofrom(some_fd s, void *buf, size_t len, int flags,
                   struct sockaddr *from, socklen_t fromlen,
                   struct sockaddr *to, socklen_t tolen) GCC_ATTR_VIS("hidden");

#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
typedef struct iovec my_iovec;
#else
# ifdef WIN32
typedef WSABUF my_iovec;
#  define iov_base buf
#  define iov_len len
# else
struct iovec
{
	void *iov_base;
	size_t iov_len;
};
typedef struct iovec my_iovec;
# endif
#endif

struct mfromto
{
	my_iovec iov;
	struct sockaddr *from, *to;
	socklen_t from_len, to_len;
};
ssize_t recvmfromto_pre(some_fd s, struct mfromto *info, size_t len, int flags) GCC_ATTR_VIS("hidden");
void recvmfromto_post(struct mfromto *info, size_t len) GCC_ATTR_VIS("hidden");
ssize_t recvmfromto(some_fd s, struct mfromto *info, size_t len, int flags) GCC_ATTR_VIS("hidden");

#endif
