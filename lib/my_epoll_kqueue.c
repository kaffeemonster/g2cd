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
#include <sys/event.h>
#include <sys/time.h>

int my_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	struct timespec tsp;
	struct kevent chg_ev[2];

	memset(&tsp, 0, sizeof(tsp));
	memset(chg_ev, 0, sizeof(chg_ev));
	
	if(0 > epfd)
	{
		errno = EBADF;
		return -1;
	}

	if(!event)
	{
		errno = EFAULT;
		return -1;
	}

	/* check for sane op before doing anything */
	if(epfd == fd || !(EPOLL_CTL_ADD == op || EPOLL_CTL_MOD == op || EPOLL_CTL_DEL == op))
	{
		errno = EINVAL;
		return -1;
	}
		  

	switch(op)
	{
	case EPOLL_CTL_ADD:
		/* we first have to add them, so we can disable the unneeded later */
		EV_SET(&chg_ev[0], fd, EVFILT_READ,
			EV_ADD, 0, 0, event->data.ptr);
		EV_SET(&chg_ev[1], fd, EVFILT_WRITE,
			EV_ADD, 0, 0, event->data.ptr);
		if(-1 == kevent(epfd, chg_ev, 2, NULL, 0, &tsp))
			return -1;
	case EPOLL_CTL_MOD:
		EV_SET(&chg_ev[0], fd, EVFILT_READ,
			(event->events & EPOLLIN) ? EV_ENABLE : EV_DISABLE,
			0, 0, event->data.ptr);
		EV_SET(&chg_ev[1], fd, EVFILT_WRITE,
			(event->events & EPOLLOUT) ? EV_ENABLE : EV_DISABLE,
			0, 0, event->data.ptr);
		break;
	case EPOLL_CTL_DEL:
		EV_SET(&chg_ev[0], fd, EVFILT_READ,
			EV_DELETE, 0, 0, 0);
		EV_SET(&chg_ev[1], fd, EVFILT_WRITE,
			EV_DELETE, 0, 0, 0);
		break;
	}
	return kevent(epfd, chg_ev, 2, NULL, 0, &tsp);
}

int my_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	struct kevent ev_list[maxevents];
	struct timespec tsp, *tsp_tmp;
	int ret_val = 0, ev_val, i;

	if(0 > epfd)
	{
		errno = EBADF;
		return -1;
	}

	if(!events)
	{
		errno = EFAULT;
		return -1;
	} 

	if(-1 != timeout)
	{
		tsp.tv_sec = timeout / 1000;
		tsp.tv_nsec = (timeout % 1000) * 1000 * 1000;
		tsp_tmp = &tsp;
	}
	else
		tsp_tmp = NULL;

	ev_val = kevent(epfd, NULL, 0, ev_list, maxevents, tsp_tmp);
	if(-1 == ev_val)
		return -1;

	for(i = 0; i < ev_val; i++)
	{
		int j;
		struct epoll_event *epv_tmp;

		if(ev_list[i].filter != EVFILT_READ && ev_list[i].filter != EVFILT_WRITE)
		{
			logg_develd("Unknown filter! p: %p i: %d fi: %hi fl: %hu ffl: %u d: %X\n",
				ev_list[i].udata, ev_list[i].ident, ev_list[i].filter,
				ev_list[i].flags, ev_list[i].fflags, ev_list[i].data);
			continue;
		}
		
		for(epv_tmp = NULL, j = 0; j < ret_val; j++)
		{
			if(ev_list[i].udata == events[j].data.ptr)
			{
				epv_tmp = &events[j];
				break;
			}
		}
		if(!epv_tmp)
		{
			epv_tmp = &events[ret_val++];
			epv_tmp->events = 0;
		}
		
		switch(ev_list[i].filter)
		{
		case EVFILT_READ:
			epv_tmp->events |= EPOLLIN;
			break;
		case EVFILT_WRITE:
			epv_tmp->events |= EPOLLOUT;
			break;
		}
		if (ev_list[i].flags & EV_EOF)
			epv_tmp->events |= POLLHUP;
		if (ev_list[i].flags & EV_ERROR)
			epv_tmp->events |= POLLERR;
		epv_tmp->data.ptr = ev_list[i].udata;
	}
	

	return ret_val;
}

int my_epoll_create(GCC_ATTR_UNUSED_PARAM(int, size))
{
	return kqueue();
}

int my_epoll_close(int epfd)
{
	return close(epfd);
}

static char const rcsid_mei[] GCC_ATTR_USED_VAR = "$Id: $";
