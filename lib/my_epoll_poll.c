/*
 * my_epoll_poll.c
 * wrapper to get epoll on systems providing poll
 *
 * Copyright (c) 2004-2009 Jan Seiffert
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

#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include "hzp.h"
#include "atomic.h"

/*
 * This can only handle one poll instance, which is broken ATM
 */

/* put the poison on the top, there should be kernel-mem -> sig11 */
#define EPDATA_POISON	0xFEEBDAEDFEEBDAEDUL
#define CAPACITY_INCREMENT 100
static pthread_mutex_t my_epoll_wmutex;
struct poll_data
{
	struct hzp_free hzp;
	int max_fd;
	int num_fd;
	struct epoll_event data[DYN_ARRAY_LEN];
};
static int poll_pipe_fd = -1;
static struct poll_data * volatile fds;

/*
 * Must be called with the writerlock held!
 */
static int realloc_fddata(int new_max)
{
	struct poll_data *old_data;
	int old_max;
	struct poll_data *tmp_data;

	tmp_data = malloc(sizeof(*tmp_data) + (sizeof(struct epoll_event) * (new_max + 1)));
	if(!tmp_data) {
		logg_errno(LOGF_ERR, "allocating fd_data memory");
		errno = ENOMEM;
		return -1;
	}
	old_data = fds;
	old_max = old_data->max_fd; /* we are under writer lock */

	/* copy data */
	memcpy(tmp_data, old_data, sizeof(*old_data) + (sizeof(struct epoll_event) * (((new_max > old_max) ? old_max : new_max) + 1)));
	/* poisen new array */
	if(new_max > old_max)
	{
		/* keep max_fd low till we filled the new entries with poison */
		/* -1 since fd 0 is not in the range */
		int tmp_fd = new_max - old_max - 1;
		/* +1 to get behind act. last entry */
		struct epoll_event *wptr = tmp_data->data + old_max + 1;
		/* at least one entry must be filled, or we would not be here */
		do {
			wptr[tmp_fd].events = 0;
			wptr[tmp_fd].data.u64 = EPDATA_POISON;
		} while(tmp_fd--);
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
	struct poll_data *tmp_ptr;
	struct epoll_event *e_ptr;

	/* generate a lock for shared free_cons */
	if(pthread_mutex_init(&my_epoll_wmutex, NULL))
		diedie("creating my_epoll writes mutex");

	tmp_ptr = malloc(sizeof(*fds) + (sizeof(struct epoll_event) * ((size_t)EPOLL_QUEUES * 20 + 1)));
	if(!tmp_ptr)
		diedie("allocating my_epoll memory");

	for(tmp_fd = 0, e_ptr = &tmp_ptr->data[0]; tmp_fd <= EPOLL_QUEUES * 20; tmp_fd++) {
		e_ptr[tmp_fd].events = 0;
		e_ptr[tmp_fd].data.u64 = EPDATA_POISON;
	}
	tmp_ptr->max_fd = EPOLL_QUEUES * 20;
	tmp_ptr->num_fd = 0;
	fds = tmp_ptr;
}

static void my_epoll_deinit(void)
{
	struct poll_data *tmp;
	fds->max_fd = 0;
	fds->num_fd = 0;
	tmp = fds;
	fds = NULL;
	free(tmp);
//	pthread_mutex_destroy(&my_epoll_mutex);
}

int my_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	struct poll_data *pd_ptr;
	int ret_val = 0;

	if(0 > epfd) {
		errno = EBADF;
		return -1;
	}
	
	if(!event) {
		errno = EFAULT;
		return -1; 
	}

	/* check for sane op before acquiring the lock */
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

	/* we are now under write lock */
	pd_ptr = fds;

	if(fd > pd_ptr->max_fd)
	{
		if(EPOLL_CTL_ADD == op) {
			if((ret_val = realloc_fddata(fd + CAPACITY_INCREMENT)))
				goto UNLOCK;
			pd_ptr = fds;
		} else {
			errno   = ENOENT;
			ret_val = -1;
			goto UNLOCK;
		}
	}

	if(EPDATA_POISON == pd_ptr->data[fd].data.u64)
	{
		if(EPOLL_CTL_ADD != op) {
			errno   = ENOENT;
			ret_val = -1;
			goto UNLOCK;
		}
		if(fd > pd_ptr->num_fd)
			pd_ptr->num_fd = fd;
	}
	else if(EPOLL_CTL_ADD == op) {
		errno = EEXIST;
		ret_val = -1;
		goto UNLOCK;
	}
	if(EPOLL_CTL_DEL != op)
		pd_ptr->data[fd] = *event;
	else {
		pd_ptr->data[fd].events = 0;
		pd_ptr->data[fd].data.u64 = EPDATA_POISON;
		pd_ptr->num_fd = pd_ptr->max_fd;
	}
// TODO: memory barrier?

	/* wake potential poller */
	ret_val = write(epfd, " ", 1);
	if(ret_val > 0)
		ret_val = 0;
UNLOCK:
	if(pthread_mutex_unlock(&my_epoll_wmutex))
		diedie("unlocking my_epoll writers mutex");

	return ret_val;
}

int my_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	struct poll_data *loc_data;
	struct pollfd *poll_buf, *f_buf;
	struct epoll_event *wptr;
	int num_poll, ret_val = 0, i, keep_going;
	char some_buf[16];

	if(0 > epfd) {
		errno = EBADF;
		return -1;
	}

	if(!events) {
		errno = EFAULT;
		return -1;
	}

retry:
	while(read(poll_pipe_fd, some_buf, sizeof(some_buf)) == sizeof(some_buf))
		/* empty poll wake pipe beforehand */;
	keep_going = 0;
	wptr = events;
	/* acquire a reference on the data array */
	do {
		mem_barrier(fds);
		hzp_ref(HZP_EPOLL, (loc_data = fds));
	} while(loc_data != fds);

	/*
	 * WARNING:
	 * poll is only a fallback, we can not handle many fds,
	 * because it would be slow, very slow.
	 */

	if(!(poll_buf = malloc(loc_data->max_fd * sizeof(*poll_buf)))) {
		logg_errno(LOGF_CRIT, "alloca for poll_buf failed");
		return -1;
	}

	for(f_buf = poll_buf, i = 0; i <= loc_data->num_fd; i++)
	{
// TODO: atomic bit test? membar?
		if(loc_data->data[i].data.u64 == EPDATA_POISON || loc_data->data[i].events & (1 << 28))
			continue;
		f_buf->fd      = i;
		f_buf->events  = loc_data->data[i].events & ~EPOLLONESHOT;
		f_buf->revents = 0;
		f_buf++;
	}
	num_poll = f_buf - poll_buf;
	if(num_poll)
		loc_data->num_fd = (f_buf - 1)->fd;

	num_poll = poll(poll_buf, num_poll, timeout);
	switch(num_poll)
	{
	default:
		keep_going = 1;
		for(f_buf = poll_buf; num_poll && maxevents; f_buf++)
		{
			if(!f_buf->revents)
					continue;

			num_poll--;
			if(f_buf->fd == poll_pipe_fd) { /* our interrupter */
				while(read(poll_pipe_fd, some_buf, sizeof(some_buf)) == sizeof(some_buf))
					/* k, thx, bye */;
				continue;
			}

			/* totaly bogus fd, close it */
			if(f_buf->fd > loc_data->max_fd) {
// TODO: close it or not, racy...
				/* fd may be not in set, but reassigned to someone else */
				close(f_buf->fd);
				logg_pos(LOGF_INFO, "hit FD out of lockup arry?");
				continue;
			}
// TODO: handle POLLNVAL, epoll does not deliver it
//			if(!(p_wptr->revents & POLLNVAL))

			wptr->data.ptr = loc_data->data[f_buf->fd].data.ptr;
			wptr->events = f_buf->revents;
			maxevents--;
			wptr++;
			ret_val++;
			/* is this a oneshot fd? mark dormant */
			if(loc_data->data[poll_buf->fd].events & EPOLLONESHOT)
				atomic_bit_set((atomic_t *)&loc_data->data[poll_buf->fd].events, 28);
		}
		break;
	case 0:
		break;
	case -1:
		ret_val = -1;
		if(EINTR == errno)
			break;
		logg_errno(LOGF_DEBUG, "poll");
		break;
	}
	/* reset reference to data array */
	hzp_unref(HZP_EPOLL);
	free(poll_buf);
	if(!ret_val && keep_going)
		goto retry;

	return ret_val;
}

int my_epoll_create(int size)
{
	int pfd[2], ret_val;
	struct poll_data *pd_ptr;

	if(1 > size) {
		errno = EINVAL;
		return -1;
	}

	if(!hzp_alloc())
		return -1;

	if(pthread_mutex_lock(&my_epoll_wmutex)) {
		logg_errno(LOGF_ERR, "locking my_epoll writes mutex");
		return -1;
	}

	ret_val = -1;
	if(poll_pipe_fd != -1) {
		errno = EEXIST;
		goto out_unlock;
	}

	if(0 > pipe(pfd))
		goto out_unlock;

	if(realloc_fddata(fds->max_fd + size)) {
		close(pfd[0]);
		close(pfd[1]);
		goto out_unlock;
	}

	if(fcntl(pfd[0], F_SETFL, O_NONBLOCK))
		logg_errno(LOGF_ERR, "setting poll interrupter fd to non blocking");
	poll_pipe_fd = pfd[0];
	ret_val = pfd[1];

	pd_ptr = fds;
	pd_ptr->data[poll_pipe_fd].events = EPOLLIN;
	pd_ptr->data[poll_pipe_fd].data.u64 = 0;
	pd_ptr->num_fd = poll_pipe_fd;
out_unlock:
	if(pthread_mutex_unlock(&my_epoll_wmutex))
		diedie("unlocking my_epoll writes mutex");

	return ret_val;
}

int my_epoll_close(int epfd)
{
	
	if(0 > epfd) {
		errno = EBADF;
		return -1;
	}

	close(poll_pipe_fd);
	poll_pipe_fd = -1;
	return close(epfd);
}

/*@unused@*/
static char const rcsid_mei[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
