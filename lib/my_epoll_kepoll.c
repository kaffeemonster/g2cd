/*
 * my_epoll_kepoll.c
 * wrapper to get epoll on systems with old libc
 *
 * Copyright (c) 2005,2006 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2
 * of the License, or any later version.
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
 * $Id: $
 */

#include <sys/syscall.h>

#ifdef __NR_epoll_create
# define	SCALL_EPOLL_CREATE	__NR_epoll_create
#else
/* warning, only on some archs 254 */
# define	SCALL_EPOLL_CREATE	254
#endif

#ifdef __NR_epoll_ctl
# define	SCALL_EPOLL_CTL	__NR_epoll_ctl
#else
/* warning, only on some archs 255 */
# define SCALL_EPOLL_CTL	255
#endif

#ifdef __NR_epoll_wait
# define	SCALL_EPOLL_WAIT	__NR_epoll_wait
#else
/* warning, only on some archs 256 */
# define SCALL_EPOLL_WAIT	256
#endif

/* hate doing this, but seems like libc segfaults if it sees 
 * an fd not allocated by a libc-func */
#define	SCALL_CLOSE	__NR_close

int my_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	return syscall(SCALL_EPOLL_CTL, epfd, op, fd, event);
}

int my_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	return syscall(SCALL_EPOLL_WAIT, epfd, events, maxevents, timeout);
}

int my_epoll_create(int size)
{
	return syscall(SCALL_EPOLL_CREATE, size);
}

int my_epoll_close(int epfd)
{
	return syscall(SCALL_CLOSE, epfd);
}

static char const rcsidi[] GCC_ATTR_USED_VAR = "$Id: $";
