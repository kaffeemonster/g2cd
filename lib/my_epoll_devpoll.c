/*
 * my_epoll_devpoll.c
 * wrapper to get epoll on solaris with /dev/poll
 *
 * Copyright (c) 2005-2009 Jan Seiffert
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

/*
 * Ok, this is now a little bit up working. It compiles, and it
 * basicaly works, minor TODOs, testig and bug-fixing.
 */

/*
 * for info view manpage: poll(7d), for those having a
 * Solaris-system
 * or online: http://docs.sun.com/app/docs/doc/817-5430/ \
 * 6mksu57ic?q=%2fdev%2fpoll&a=view
 */
#include <sys/devpoll.h>
#include <fcntl.h>
#include <alloca.h>
#include "hzp.h"

/*
 * The Kernel handles the mapping epfd <-> fd so we don't have
 * to do that, only find the corresponding data.
 * And since mostly all of our fds chill out in one of our emulated
 * epolls we could do a table lookup to find the data to the fd.
 */
/* put the poison on the top, there should be kernel-mem -> sig11 */
#define EPDATA_POISON	0xFEEBDAEDFEEBDAEDUL
#define CAPACITY_INCREMENT 100
static pthread_mutex_t my_epoll_wmutex;
struct dev_poll_data
{
	struct hzp_free hzp;
	int max_fd;
	epoll_data_t data[DYN_ARRAY_LEN];
};
static struct dev_poll_data * volatile fds;

/*
 * Must be called with the writerlock held!
 */
static int realloc_fddata(int new_max)
{
	struct dev_poll_data *old_data;
	int old_max;
	struct dev_poll_data *tmp_data;

	tmp_data = malloc(sizeof(*tmp_data) + (sizeof(epoll_data_t) * (new_max + 1)));
	if(!tmp_data) {
		logg_errno(LOGF_ERR, "allocating fd_data memory");
		errno = ENOMEM;
		return -1;
	}
	old_data = fds;
	old_max = old_data->max_fd; /* we are under writer lock */

	/* copy data */
	memcpy(tmp_data, old_data, sizeof(*old_data) + (sizeof(epoll_data_t) * (((new_max > old_max) ? old_max : new_max) + 1)));
	/* poisen new array */
	if(new_max > old_max)
	{
		/* keep max_fd low till we filled the new entries with poison */
		/* -1 since fd 0 is not in the range */
		int tmp_fd = new_max - old_max - 1;
		/* +1 to get behind act. last entry */
		epoll_data_t *wptr = tmp_data->data + old_max + 1;
		/* at least one entry must be filled, or we would not be here */
		do
			wptr[tmp_fd].u64 = EPDATA_POISON;
		while(tmp_fd--);
	}
	/* test fd_data, just in case */
	if(old_data != fds)
	{
		logg_pos(LOGF_WARN, "fd_data changed even under writer lock???");
		free(tmp_data);
		errno = EAGAIN;
		return -1;
	}

	tmp_data->max_fd = new_max;
	/* make sure memory comes back in place */
	fds = tmp_data;
// TODO: maybe membar here...

	/* mark old data for free */
	hzp_deferfree(&old_data->hzp, old_data, free);

	return 0;
}

/*
 * Needs no locking, since it should be called before any
 * multithreading is involved...
 */
	/* you better not delete this proto, or it won't work */
static void my_epoll_init(void) GCC_ATTR_CONSTRUCT;
static void my_epoll_deinit(void) GCC_ATTR_DESTRUCT;

static void my_epoll_init(void)
{
	int tmp_fd;
	struct dev_poll_data *tmp_ptr;
	epoll_data_t *e_ptr;

	/* generate a lock for shared free_cons */
	if(pthread_mutex_init(&my_epoll_wmutex, NULL))
		diedie("creating my_epoll writes mutex");

	tmp_ptr = malloc(sizeof(*tmp_ptr) + sizeof(epoll_data_t) * ((size_t)EPOLL_QUEUES * 20 + 1));
	if(!tmp_ptr)
		diedie("allocating my_epoll memory");

	for(tmp_fd = 0, e_ptr = &tmp_ptr->data[0]; tmp_fd <= EPOLL_QUEUES * 20; tmp_fd++)
		e_ptr[tmp_fd].u64 = EPDATA_POISON;
	tmp_ptr->max_fd = EPOLL_QUEUES * 20;
	fds = tmp_ptr;
}

static void my_epoll_deinit(void)
{
	struct dev_poll_data *tmp;
	fds->max_fd = 0;
	tmp = fds;
	fds = NULL;
	free(tmp);
//	pthread_mutex_destroy(&my_epoll_mutex);
}

int my_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	struct pollfd tmp_poll;
	ssize_t w_val;
	int ret_val = 0;

	if(0 > epfd) {
		errno = EBADF;
		return -1;
	}
	
	if(!event) {
		errno = EFAULT;
		return -1; 
	}

	/* check for sane op before accuaring the lock */
	if(epfd == fd ||
	   !(EPOLL_CTL_ADD == op || EPOLL_CTL_MOD == op || EPOLL_CTL_DEL == op)) {
		errno = EINVAL;
		return -1;
	}

	if((EPOLL_CTL_ADD == op || EPOLL_CTL_MOD == op) && event->events & EPOLLET) {
		/* we cannot emulate EPOLLET */
		logg_develd("edge triggered request: efd: %i fd: %i e: 0x%x p: %p\n",
		            epfd, fd, event->events, event->data.ptr);
		errno = EINVAL;
		return -1;
	}

	if(pthread_mutex_lock(&my_epoll_wmutex)) {
		logg_errno(LOGF_ERR, "locking my_epoll writers mutex");
		return -1;
	}

	if(fd > fds->max_fd)
	{
		if(EPOLL_CTL_ADD == op) {
			if((ret_val = realloc_fddata(fd + CAPACITY_INCREMENT)))
				goto UNLOCK;
		} else {
			errno   = ENOENT;
			ret_val = -1;
			goto UNLOCK;
		}
	}

	/*
	 * DIRTY trick:
	 * We use the lowest LSB of the user data to save if it is oneshot.
	 * Userdata is in our case "always" a pointer, which is aligned and the LSB unused
	 * If this is not true -> *booom*
	 */

	if(EPDATA_POISON == fds->data[fd].u64)
	{
		if(EPOLL_CTL_ADD != op) {
			errno   = ENOENT;
			ret_val = -1;
			goto UNLOCK;
		}

		tmp_poll.fd      = fd;
		tmp_poll.events  = event->events & ~(EPOLLONESHOT | EPOLLET);
		tmp_poll.revents = 0;

		while(sizeof(tmp_poll) != (w_val = pwrite(epfd, &tmp_poll, sizeof(tmp_poll), 0)) && EINTR == errno);
		if(sizeof(tmp_poll) == w_val)
			fds->data[fd].ptr = (void *)(((uintptr_t)event->data.ptr & ~1) | (!!(event->events & EPOLLONESHOT)));
		else {
			logg_errno(LOGF_ERR, "adding new fd to /dev/poll");
			ret_val = -1;
		}
	}
	else
	{
		/* remove the fd for both CTL_MOD and CTL_DEL */
		if(EPOLL_CTL_ADD == op) {
			errno = EEXIST;
			ret_val = -1;
			goto UNLOCK;
		}

		tmp_poll.fd      = fd;
		tmp_poll.events  = POLLREMOVE;
		tmp_poll.revents = 0;

		while(sizeof(tmp_poll) != (w_val = pwrite(epfd, &tmp_poll, sizeof(tmp_poll), 0)) && EINTR == errno);
		if(sizeof(tmp_poll) == w_val)
			fds->data[fd].u64 = EPDATA_POISON;
		else {
			logg_errno(LOGF_ERR, "removing fd to /dev/poll");
			ret_val = -1;
		}

		if(EPOLL_CTL_MOD == op)
		{
			tmp_poll.events = event->events & ~(EPOLLONESHOT | EPOLLET);
			while(sizeof(tmp_poll) != (w_val = pwrite(epfd, &tmp_poll, sizeof(tmp_poll), 0)) && EINTR == errno);
			if(sizeof(tmp_poll) == w_val)
				fds->data[fd].ptr = (void *)(((uintptr_t)event->data.ptr & ~1) | (!!(event->events & EPOLLONESHOT)));
			else {
				logg_errno(LOGF_ERR, "readding modified fd to /dev/poll");
				ret_val = -1;
			}
		}
	}

UNLOCK:
	if(pthread_mutex_unlock(&my_epoll_wmutex))
		diedie("unlocking my_epoll writers mutex");

	return ret_val;
}

int my_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	struct pollfd *poll_buf, *back_buf;
	struct epoll_event *wptr;
	struct dvpoll do_poll;
	int num_poll, ret_val = 0;

	if(0 > epfd) {
		errno = EBADF;
		return -1;
	}

	if(!events) {
		errno = EFAULT;
		return -1;
	}

	wptr = events;
	if(!(poll_buf = alloca((size_t)maxevents * sizeof(*poll_buf)))) {
		logg_errno(LOGF_CRIT, "alloca for poll_buf failed");
		return -1;
	}

	do_poll.dp_fds = poll_buf;
	do_poll.dp_nfds = maxevents;
	do_poll.dp_timeout = timeout;

	num_poll = ioctl(epfd, DP_POLL, &do_poll);
	switch(num_poll)
	{
		struct dev_poll_data *loc_data;
	default:
		/* acquire a reference on the data array */
		do {
			mem_barrier(fds);
			hzp_ref(HZP_EPOLL, (loc_data = fds));
		} while(loc_data != fds);

		back_buf = poll_buf;
		for(; num_poll; num_poll--, wptr++, poll_buf++)
		{
			/* totaly bogus fd, close it */
			if(poll_buf->fd > loc_data->max_fd) {
// TODO: close it or not, racy...
				/* fd may be not in set, but reassigned to someone else */
				close(poll_buf->fd);
				logg_pos(LOGF_INFO, "hit FD out of lockup arry?");
				continue;
			}
// TODO: handle POLLNVAL, epoll does not deliver it
//			if(!(p_wptr->revents & POLLNVAL))

			wptr->data.ptr = (void *)((uintptr_t)loc_data->data[poll_buf->fd].ptr & ~1);
			wptr->events = poll_buf->revents;
			ret_val++;
			/* is this a oneshot fd? */
			if((uintptr_t)loc_data->data[poll_buf->fd].ptr & 1)
			{
				back_buf->fd      = poll_buf->fd;
				back_buf->events  = POLLREMOVE;
				back_buf->revents = 0;
				back_buf++;
			}
		}
		num_poll = (char *)back_buf - (char *)poll_buf;
		if(num_poll) {
			ssize_t w_val;
			while(num_poll != (w_val = pwrite(epfd, poll_buf, num_poll, 0)) && EINTR == errno);
			if(num_poll != w_val)
				logg_errno(LOGF_ERR, "removing oneshot fds from /dev/poll");
		}

		/* reset reference to data array */
		hzp_unref(HZP_EPOLL);
		break;
	case 0:
		break;
	case -1:
		if(EINTR == errno)
			break;
		ret_val = -1;
		logg_errno(LOGF_DEBUG, "ioctl");
		break;
	}

	return ret_val;
}

int my_epoll_create(int size)
{
	int dpfd;

	if(1 > size) {
		errno = EINVAL;
		return -1;
	}

	if(!hzp_alloc())
		return -1;

	if(0 > (dpfd = open("/dev/poll", O_RDWR)))
		return -1;

	if(!pthread_mutex_lock(&my_epoll_wmutex))
	{
		if(realloc_fddata(fds->max_fd + size)) {
			close(dpfd);
			dpfd = -1;
		}

		if(pthread_mutex_unlock(&my_epoll_wmutex))
			diedie("unlocking my_epoll writes mutex");
	}
	else {
		logg_errno(LOGF_ERR, "locking my_epoll writes mutex");
		close(dpfd);
		return -1;
	}

	return dpfd;
}

int my_epoll_close(int epfd)
{
	
	if(0 > epfd) {
		errno = EBADF;
		return -1;
	}

	return close(epfd);
}

static char const rcsid_mei[] GCC_ATTR_USED_VAR = "$Id: $";
