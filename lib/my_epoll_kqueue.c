/*
 * my_epoll_poll.c
 * wrapper to get epoll on systems providing kqueue (BSDs)
 *
 * Copyright (c) 2004, 2005,2006 Jan Seiffert
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
 * $Id: $
 */

// TODO: write this stuff?

#error "oh-oh"

	/* you better not delete this proto, or it won't work */
static void my_epoll_init(void) GCC_ATTR_CONSTRUCT;
static void my_epoll_deinit(void) GCC_ATTR_DESTRUCT;

static void my_epoll_init(void)
{
// num = EPOLL_QUEUES
	die("Your kidding? No epoll emulation, no go");
}

static void my_epoll_deinit(void)
{
}

int my_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	return -1;
}

int my_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	return -1;
}

int my_epoll_create(int size)
{
	return -1;
}

int my_epoll_close(int epfd)
{
	return -1;
}

static char const rcsid_mei[] GCC_ATTR_USED_VAR = "$Id: $";
