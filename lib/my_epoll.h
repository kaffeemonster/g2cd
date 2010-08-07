/*
 * my_epoll.h
 * epoll -> other multiplexed I/O glue header
 *
 * Copyright (c) 2004-2010 Jan Seiffert
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
 * $Id: my_epoll.h,v 1.9 2005/11/05 10:31:43 redbully Exp redbully $
 */

#ifndef LIB_MY_EPOLL_H
#define LIB_MY_EPOLL_H

#ifdef HAVE_CONFIG_H
# include "../config.h"
#endif /* HAVE_CONFIG_H */

#ifndef WIN32
# include <sys/poll.h>
# define my_epoll_accept(a, b, c, d) accept(b, c, d)
# define my_epoll_connect(b, c, d) connect(b, c, d)
# define my_epoll_socket(a, b, c) socket(a, b, c)
# define my_epoll_fcntl(x, y, ...) fcntl(x, y, ## __VA_ARGS__)
# define my_epoll_bind(a, b, c) bind(a, b, c)
# define my_epoll_listen(a, b) listen(a, b)
# define my_epoll_setsockopt(a, b, c, d, e) setsockopt(a, b, c, d, e)
# define my_epoll_send(a, b, c, d, e) send(b, c, d, e)
# define my_epoll_sendto(a, b, c, d, e, f) sendto(a, b, c, d, e, f)
# define my_epoll_recv(a, b, c, d, e) recv(b, c, d, e)
# define my_epoll_recvmsg(a, b, c) recvmsg(a, b, c)
# define my_epoll_closesocket(x) close(x)
# define closesocket(x) close(x)
typedef int some_fd;
# else
typedef intptr_t some_fd;
#endif
/* no compat needed? simply map the calls */
#ifndef NEED_EPOLL_COMPAT
# include <sys/epoll.h>

# define my_epoll_ctl(w, x, y, z)	epoll_ctl(w, x, y, z)
# define my_epoll_wait(w, x, y, z)	epoll_wait(w, x, y, z)
# define my_epoll_create(x)	epoll_create(x)
# define my_epoll_close(x)	close(x)

typedef struct pollfd my_pollfd;
#else
/* Ok, we need Compat, lets do things common to
 * all Compat-layers
 */

# include "other.h"

/* how many concurrent epfds will be needed? */
# define EPOLL_QUEUES 3

# define EPOLL_CTL_ADD	0x0001
# define EPOLL_CTL_DEL	0x0002
# define EPOLL_CTL_MOD	0x0003

typedef union epoll_data
{
		void *ptr;
		some_fd fd;
		uint32_t u32;
		uint64_t u64;
} epoll_data_t;

/* If we talk directly to the kernel, they do funny things
 * to make 32-bit emulation "easier" */
# if defined HAVE_KEPOLL && defined __x86_64__
#  define MY_EPOLL_PACKED GCC_ATTR_PACKED
# else
#  define MY_EPOLL_PACKED
# endif /* HAVE_KEPOLL && __x86_64__ */

struct epoll_event
{
	uint32_t events;
	epoll_data_t data;
} MY_EPOLL_PACKED;

# ifndef _MY_EPOLL_C
#  define _MY_E_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define _MY_E_EXTRN(x) x GCC_ATTR_VIS("hidden")
# endif /* _MY_EPOLL_C */

_MY_E_EXTRN(int my_epoll_ctl(some_fd epfd, int op, some_fd fd, struct epoll_event *event));
_MY_E_EXTRN(int my_epoll_wait(some_fd epfd, struct epoll_event *events, int maxevents, int timeout));
_MY_E_EXTRN(some_fd my_epoll_create(int size));
_MY_E_EXTRN(int my_epoll_close(some_fd epfd));

# ifdef WIN32
/* vista and greater know WSAPoll, guess what... */
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <mswsock.h>
#  ifndef POLLIN
#   define POLLERR     (1<<0)
#   define POLLHUP     (1<<1)
#   define POLLNVAL    (1<<2)
#   define POLLWRNORM  (1<<4)
#   define POLLWRBAND  (1<<5)
#   define POLLOUT     POLLWRNORM
#   define POLLRDNORM  (1<<8)
#   define POLLRDBAND  (1<<9)
#   define POLLIN      (POLLRDNORM | POLLRDBAND)
#   define POLLPRI     (1<<10)
typedef struct pollfd {
	SOCKET	fd;   /* file descriptor */
	short	events;  /* requested events */
	short	revents; /* returned events */
} WSAPOLLFD;
#  endif
typedef WSAPOLLFD my_pollfd;

#  define F_GETFL 0
#  define F_SETFL 1
#  define O_NONBLOCK (1<<0)
typedef WSAMSG my_msghdr;
typedef WSACMSGHDR my_cmsghdr;
# define msg_control Control.buf
# define msg_controllen Control.len
# define msg_name name
# define msg_namelen namelen
# define msg_flags dwFlags
# define msg_iov lpBuffers
# define msg_iovlen dwBufferCount

# define EWOULDBLOCK WSAEWOULDBLOCK
_MY_E_EXTRN(int fcntl(int fd, int cmd, ...));
_MY_E_EXTRN(int PASCAL poll(my_pollfd *fds, unsigned long nfds, int timeout));
_MY_E_EXTRN(int socketpair(int domain, int type, int protocol, some_fd sv[2]));
_MY_E_EXTRN(int my_epoll_closesocket(some_fd sockfd));
_MY_E_EXTRN(int my_epoll_fcntl(some_fd fd, int cmd, ...));
_MY_E_EXTRN(int my_epoll_bind(some_fd sockfd, const struct sockaddr *addr, socklen_t addrlen));
_MY_E_EXTRN(int my_epoll_listen(some_fd sockfd, int backlog));
_MY_E_EXTRN(int my_epoll_setsockopt(some_fd sockfd, int level, int optname, const void *optval, socklen_t optlen));
_MY_E_EXTRN(some_fd my_epoll_accept(some_fd epfd, some_fd sockfd, struct sockaddr *addr, socklen_t *addrlen));
_MY_E_EXTRN(int my_epoll_connect(some_fd sockfd, const struct sockaddr *addr, socklen_t addrlen));
_MY_E_EXTRN(some_fd my_epoll_socket(int domain, int type, int protocol));
_MY_E_EXTRN(ssize_t my_epoll_send(some_fd epfd, some_fd sockfd, const void *buf, size_t len, int flags));
_MY_E_EXTRN(ssize_t my_epoll_sendto(some_fd sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen));
_MY_E_EXTRN(ssize_t my_epoll_recv(some_fd epfd, some_fd sockfd, void *buf, size_t len, int flags));
_MY_E_EXTRN(ssize_t my_epoll_recvmsg(some_fd sockfd, my_msghdr *msg, int flags));

# else
typedef struct pollfd my_pollfd;
# endif
/* now do the things special to the compat-layer */
# if (defined(HAVE_POLL) || defined(HAVE_POLLSET) || defined(HAVE_KEPOLL) || defined(HAVE_KQUEUE) || defined(HAVE_DEVPOLL) || defined (HAVE_EPORT)) || defined(WIN32)
/* Solaris /dev/poll also wraps to some poll things */
/* Basics */
#  define EPOLLIN	POLLIN
#  define EPOLLPRI	POLLPRI
#  define EPOLLOUT	POLLOUT
/* Errs */
#  define EPOLLERR	POLLERR
#  define EPOLLHUP	POLLHUP
/* EPoll does not know NVAL, it silently discards NVAL fd's */
/* #  define EPOLLNVAL	POLLNVAL */
/* OOB-Msg specific, just for completness */
#  define EPOLLRDNORM	POLLRDNORM
#  define EPOLLRDBAND	POLLRDBAND
#  define EPOLLWRNORM	POLLWRNORM
#  define EPOLLWRBAND	POLLWRBAND
#  define EPOLLMSG	POLLMSG
/* EPoll specials, must see, if i could emulate them */
	/* One-Shot */
#  define EPOLLONESHOT (1 << 30)
	/* Edge-Triggerd */
#  define EPOLLET	(1 << 31)
# endif /* HAVE_POLL || HAVE_KEPOLL || HAVE_DEVPOLL || HAVE_EPORT */

#endif /* NEED_EPOLL_COMPAT */
#endif /* LIB_MY_EPOLL_H */
