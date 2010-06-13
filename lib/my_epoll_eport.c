/*
 * my_epoll_poll.c
 * wrapper to get epoll on systems providing event ports (Solaris 10)
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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
 * $Id: $
 */

#include <port.h>
#include <sys/time.h>

int my_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	unsigned events;

	if(0 > epfd) {
		errno = EBADF;
		return -1;
	}

	if(!event) {
		errno = EFAULT;
		return -1;
	}

	/* check for sane op before doing anything */
	if(epfd == fd) {
		errno = EINVAL;
		return -1;
	}

	switch(op)
	{
	case EPOLL_CTL_ADD:
	case EPOLL_CTL_MOD:
		events = event->events;
		/*
		 * since eport is always oneshot, and it's whole model depends
		 * on queued events, we have a problem:
		 * When you want non-oneshot operation, you have to requeue the
		 * fd AFTER you consumed the CAUSE of the event (so a kind of
		 * worst mix of oneshot, edge triggered and level).
		 * If you immideately requeue the fd after receiving the event,
		 * a new event will be queued because the cause of the event is
		 * still there and the event will stay avail. even if you
		 * removed the cause of the event, you have to consume by
		 * port_get.
		 *
		 * This means WE (as in this module) can not requeue the fd, the
		 * app has to at a proper moment, which means after read/write or
		 * something like that.
		 * Which means we can only support oneshot.
		 */
		if(!(events & EPOLLONESHOT)) {
			logg_develd("non one-shot request: efd: %i fd: %i e: 0x%x p: %p\n",
			            epfd, fd, events, event->data.ptr);
			break;
		}
		/* eport uses the normal poll(2) defines, mask out epoll specific stuff */
		events &= ~(EPOLLONESHOT | EPOLLET);
		/*
		 * eport ony takes a *void as user data, which means we can not
		 * emulate the u64 member of epoll_data on 32 bit
		 */
		return port_associate(epfd, PORT_SOURCE_FD, fd, events, event->data.ptr);
	case EPOLL_CTL_DEL:
		/*
		 * sometimes dissociate returns ENOENT, which isn't a deadly problem
		 * see other software like apache
		 */
		return port_dissociate(epfd, PORT_SOURCE_FD, fd) == 0 ? 0 : (ENOENT == errno ? 0 : -1);
	}
	errno = EINVAL;
	return -1;
}

int my_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	port_event_t ev_list[maxevents];
	timespec_t tsp, *tsp_tmp;
	unsigned nget, i;
	int ret_val, ev_val;

	if(0 > epfd) {
		errno = EBADF;
		return -1;
	}

	if(!events) {
		errno = EFAULT;
		return -1;
	} 

	if(-1 != timeout) {
		tsp.tv_sec = timeout / 1000;
		tsp.tv_nsec = (timeout % 1000) * 1000 * 1000;
		tsp_tmp = &tsp;
	} else
		tsp_tmp = NULL;

	nget = 1;
	ev_val = port_getn(epfd, ev_list, maxevents, &nget, tsp_tmp);
	if(-1 == ev_val) {
		if(ETIME == errno)
			return 0;
		return -1;
	}

	ret_val = 0;
	for(i = 0; i < nget; i++)
	{
		struct epoll_event *epv_tmp;

		if(ev_list[i].portev_source != PORT_SOURCE_FD)
		{
			logg_develd("Unknown port source! p: %p s: 0x%hx e: 0x%x o: %lu\n",
				ev_list[i].portev_user, ev_list[i].portev_source, ev_list[i].portev_events,
				(unsigned long)ev_list[i].portev_object);
			continue;
		}

// TODO: handle POLLNVAL cleanly, epoll does not deliver it
		if(ev_list[i].portev_events & POLLNVAL)
			continue; /* epoll discards nval, we missuse eport one shot charactaristics */

		epv_tmp = &events[ret_val++];
		epv_tmp->events = ev_list[i].portev_events;
		epv_tmp->data.ptr = ev_list[i].portev_user;;
	}

	return ret_val;
}

int my_epoll_create(int size GCC_ATTR_UNUSED_PARAM)
{
	return port_create();
}

int my_epoll_close(int epfd)
{
	return close(epfd);
}

/*@unused@*/
static char const rcsid_mei[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
