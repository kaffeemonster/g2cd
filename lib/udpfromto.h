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
 * IPv6 and changes etc. Copyright (C) 2009 Jan Seiffert
 */
#ifndef UDPFROMTO_H
# define UDPFROMTO_H

# include <sys/socket.h>
# include "other.h"

int udpfromto_init(int s, int fam) GCC_ATTR_VIS("hidden");
ssize_t recvfromto(int s, void *buf, size_t len, int flags,
                   struct sockaddr *from, socklen_t *fromlen,
                   struct sockaddr *to, socklen_t *tolen) GCC_ATTR_VIS("hidden");
ssize_t sendtofrom(int s, void *buf, size_t len, int flags,
                   struct sockaddr *from, socklen_t fromlen,
                   struct sockaddr *to, socklen_t tolen) GCC_ATTR_VIS("hidden");

#endif
