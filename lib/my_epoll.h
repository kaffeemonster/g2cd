/*
 * my_epoll.h
 * epoll -> other multiplexed I/O glue header
 *
 * Copyright (c) 2004, 2005 Jan Seiffert
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
 * $Id: my_epoll.h,v 1.9 2005/11/05 10:31:43 redbully Exp redbully $
 */

#ifndef LIB_MY_EPOLL_H
#define LIB_MY_EPOLL_H

#ifdef HAVE_CONFIG_H
# include "../config.h"
#endif /* HAVE_CONFIG_H */

/* no compat needed? simply map the calls */
#ifndef NEED_EPOLL_COMPAT
# include <sys/epoll.h>

# define my_epoll_ctl(w, x, y, z)	epoll_ctl(w, x, y, z)
# define my_epoll_wait(w, x, y, z)	epoll_wait(w, x, y, z)
# define my_epoll_create(x)	epoll_create(x)
# define my_epoll_close(x)	close(x)

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
		int fd;
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

_MY_E_EXTRN(int my_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event));
_MY_E_EXTRN(int my_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout));
_MY_E_EXTRN(int my_epoll_create(int size));
_MY_E_EXTRN(int my_epoll_close(int epfd));

/* now do the things special to the compat-layer */
# if defined HAVE_POLL || defined HAVE_KEPOLL || defined HAVE_KQUEUE || defined HAVE_DEVPOLL
/* Solaris /dev/poll also wraps to some poll things */
#  include <sys/poll.h>
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
#	define EPOLLMSG	POLLMSG
/* EPoll specials, must see, if i could emulate them */
	/* One-Shot */
#  define EPOLLONESHOT (1 << 30)
	/* Edge-Triggerd */
#  define EPOLLET	(1 << 31)
# endif /* HAVE_POLL || HAVE_KEPOLL || HAVE_DEVPOLL */

#endif /* NEED_EPOLL_COMPAT */
#endif /* LIB_MY_EPOLL_H */
