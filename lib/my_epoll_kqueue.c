/*
 * my_epoll_kqueue.c
 * wrapper to get epoll on systems providing kqueue (BSDs)
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
 * $Id: $
 */

#include <sys/event.h>
#include <sys/time.h>

#define built_flags(action, events, en) \
	(((en) ? EV_ENABLE : EV_DISABLE) | \
	(((events) & EPOLLONESHOT) ? EV_ONESHOT : 0) | \
	(((events) & EPOLLET) ? EV_CLEAR : 0) | \
	 action )
#define ep_i(events) ((events) & EPOLLIN)
#define ep_o(events) ((events) & EPOLLOUT)

int my_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	struct timespec tsp;
	struct kevent chg_ev[2];
	struct kevent ev_list[2];
	struct kevent *ev;
	int num, i;
	unsigned short flags;

	if(0 > epfd) {
		errno = EBADF;
		return -1;
	}

	if(!event) {
		errno = EFAULT;
		return -1;
	}

	/* check for sane op before doing anything */
	if(epfd == fd || !(EPOLL_CTL_ADD == op || EPOLL_CTL_MOD == op || EPOLL_CTL_DEL == op)) {
		errno = EINVAL;
		return -1;
	}

	memset(&tsp, 0, sizeof(tsp));
	memset(chg_ev, 0, sizeof(chg_ev));
	num = 0;
	ev = &chg_ev[0];
	switch(op)
	{
	case EPOLL_CTL_ADD:
	case EPOLL_CTL_MOD:
		flags = built_flags(EV_ADD, event->events, ep_i(event->events));
		EV_SET(ev, fd, EVFILT_READ, flags, 0, 0, event->data.ptr);
		ev++;
		num++;
		flags = built_flags(EV_ADD, event->events, ep_o(event->events));
		EV_SET(ev, fd, EVFILT_WRITE, flags, 0, 0, event->data.ptr);
		num++;
		break;
	case EPOLL_CTL_DEL:
		EV_SET(ev, fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
		ev++;
		num++;
		EV_SET(ev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
		num++;
		break;
	}
#if 0
	for(i = 0; i < num; i++) {
		logg_develd("p: %p i: %d fi: %i fl: 0x%hx ffl: %u d: %d\n",
			chg_ev[i].udata, chg_ev[i].ident, chg_ev[i].filter,
			chg_ev[i].flags, chg_ev[i].fflags, chg_ev[i].data);
	}
#endif
	num = kevent(epfd, chg_ev, num, ev_list, 2, &tsp);
	if(-1 == num)
		return num;
	for(i = 0; i < num; i++)
	{
		if(!(ev_list[i].flags & EV_ERROR))
			continue;
		errno = ev_list[i].data;
		logg_develd("p: %p i: %d fi: %hi fl: 0x%hx ffl: %u d: %d\n",
			ev_list[i].udata, ev_list[i].ident, ev_list[i].filter,
			ev_list[i].flags, ev_list[i].fflags, ev_list[i].data);
		return -1;
	}
	return 0;
}

int my_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	struct kevent ev_list[maxevents];
	struct timespec tsp, *tsp_tmp;
	int ret_val = 0, ev_val, i;

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

// TODO: this prop. needs a lock, to disable the other oneshot
	ev_val = kevent(epfd, NULL, 0, ev_list, maxevents, tsp_tmp);
	if(-1 == ev_val)
		return -1;

	for(i = 0; i < ev_val; i++)
	{
		int j;
		struct epoll_event *epv_tmp;

		if(ev_list[i].filter != EVFILT_READ && ev_list[i].filter != EVFILT_WRITE)
		{
			logg_develd("Unknown filter! p: %p i: %d fi: %hi fl: 0x%hx ffl: %u d: 0x%X\n",
				ev_list[i].udata, ev_list[i].ident, ev_list[i].filter,
				ev_list[i].flags, ev_list[i].fflags, ev_list[i].data);
			continue;
		}

		for(epv_tmp = NULL, j = 0; j < ret_val; j++) {
			if(ev_list[i].udata == events[j].data.ptr) {
				epv_tmp = &events[j];
				break;
			}
		}
		if(!epv_tmp) {
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
		if (ev_list[i].flags & EV_ONESHOT) {
// TODO: disable other event at oneshot
			epv_tmp->events |= EPOLLONESHOT;
		}
		epv_tmp->data.ptr = ev_list[i].udata;
	}

	return ret_val;
}

int my_epoll_create(int size GCC_ATTR_UNUSED_PARAM)
{
	return kqueue();
}

int my_epoll_close(int epfd)
{
	return close(epfd);
}

/*@unused@*/
static char const rcsid_mei[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
