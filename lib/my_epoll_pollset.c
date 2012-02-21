/*
 * my_epoll_pollset.c
 * wrapper to get epoll on systems providing pollset (AIX)
 *
 * Copyright (c) 2004-2012 Jan Seiffert
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

#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#endif
#include <sys/pollset.h>
#include <unistd.h>
#include <fcntl.h>
#include "hzp.h"
#include "atomic.h"

/*
 * This can only handle one epoll instance, which is EXACTLY the number in g2cd
 */

/* put the poison on the top, there should be kernel-mem -> sig11 */
#define EPDATA_POISON	0xFEEBDAEDFEEBDAEDUL
#define CAPACITY_INCREMENT 100
static pthread_mutex_t my_epoll_wmutex;
static pthread_mutex_t my_epoll_pmutex;
struct poll_data
{
	struct hzp_free hzp;
	int max_fd;
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
	wmb();

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

static __init void my_epoll_init(void)
{
	int tmp_fd;
	struct poll_data *tmp_ptr;
	struct epoll_event *e_ptr;

	/* generate a lock for shared free_cons */
	if((errno = pthread_mutex_init(&my_epoll_wmutex, NULL)))
		diedie("creating my_epoll writes mutex");
	/* generate a lock for emulating oneshot */
	if((errno = pthread_mutex_init(&my_epoll_pmutex, NULL)))
		diedie("creating my_epoll poll mutex");

	tmp_ptr = malloc(sizeof(*fds) + (sizeof(struct epoll_event) * ((size_t)EPOLL_QUEUES * 20 + 1)));
	if(!tmp_ptr)
		diedie("allocating my_epoll memory");

	for(tmp_fd = 0, e_ptr = &tmp_ptr->data[0]; tmp_fd <= EPOLL_QUEUES * 20; tmp_fd++) {
		e_ptr[tmp_fd].events = 0;
		e_ptr[tmp_fd].data.u64 = EPDATA_POISON;
	}
	tmp_ptr->max_fd = EPOLL_QUEUES * 20;
	fds = tmp_ptr;
}

static void my_epoll_deinit(void)
{
	struct poll_data *tmp;
	fds->max_fd = 0;
	tmp = fds;
	fds = NULL;
	free(tmp);
//	pthread_mutex_destroy(&my_epoll_mutex);
}

int my_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	struct poll_data *pd_ptr;
	struct poll_ctl pctl;
	int ret_val = 0, res;

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

	if((res = pthread_mutex_lock(&my_epoll_wmutex))) {
		errno = res;
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
	}
	else if(EPOLL_CTL_ADD == op) {
		errno = EEXIST;
		ret_val = -1;
		goto UNLOCK;
	}
	if(EPOLL_CTL_DEL != op)
	{
		if(EPOLL_CTL_MOD == op && pd_ptr->data[fd].events & ~event->events)
		{
			/* when we want to delete a flag, we have to remove and add again */
			pctl.cmd = PS_DELETE;
			pctl.events = 0;
			pctl.fd = fd;
			ret_val = pollset_ctl(epfd, &pctl, 1);
			op = EPOLL_CTL_ADD;
		}
		pd_ptr->data[fd] = *event;
	} else {
		pd_ptr->data[fd].events = 0;
		pd_ptr->data[fd].data.u64 = EPDATA_POISON;
	}
	if(EPOLL_CTL_ADD == op)
		pctl.cmd = PS_ADD;
	else if (EPOLL_CTL_MOD == op)
		pctl.cmd = PS_MOD;
	else
		pctl.cmd = PS_DELETE;
	pctl.events = pd_ptr->data[fd].events & ~(EPOLLONESHOT|EPOLLET);
	pctl.fd = fd;
	ret_val = pollset_clt(epfd, &pctl, 1);
UNLOCK:
	if((res = pthread_mutex_unlock(&my_epoll_wmutex))) {
		errno = res;
		diedie("unlocking my_epoll writers mutex");
	}

	return ret_val;
}

int my_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	struct poll_data *loc_data;
	struct pollfd poll_buf[maxevents];
	struct poll_ctl *back_buf, *back_buf_start;
	struct epoll_event *wptr;
	int num_poll, ret_val = 0, i, res;

	if(0 > epfd) {
		errno = EBADF;
		return -1;
	}

	if(!events) {
		errno = EFAULT;
		return -1;
	}

	if(unlikely(res = pthread_mutex_lock(&my_epoll_pmutex))) {
		errno = res;
		diedie("locking my_epoll pollset mutex");
	}

	num_poll = pollset_poll(epfd, poll_buf, maxevents, timeout);
	switch(num_poll)
	{
	default:
		/* acquire a reference on the data array */
		do {
			mb();
			hzp_ref(HZP_EPOLL, (loc_data = fds));
		} while(loc_data != fds);

		back_buf_start = back_buf = (struct poll_ctl *)poll_buf;
		for(wptr = events; num_poll; num_poll--, poll_buf++)
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
			if(EPDATA_POISON == loc_data->data[poll_buf->fd].data.ptr)
				continue;


			wptr->data.ptr = loc_data->data[poll_buf->fd].data.ptr;
			wptr->events = poll_buf->revents;
			wptr++;
			ret_val++;
			/* is this a oneshot fd? mark dormant */
			if(loc_data->data[poll_buf->fd].events & EPOLLONESHOT)
			{
				volatile int x = poll_buf->fd;
				back_buf->cmd = PS_DELETE;
				back_buf->events = 0;
				back_buf->fd = x;
				back_buf++;
			}
		}
		num_poll = back_buf - back_buf_start;
		if(num_poll)
			pollset_ctl(epfd, back_buf_start, num_poll);
		/* reset reference to data array */
		hzp_unref(HZP_EPOLL);
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
	if(unlikely(res = pthread_mutex_unlock(&my_epoll_pmutex))) {
		errno = res;
		diedie("unlocking my_epoll pollset mutex");
	}

	return ret_val;
}

int my_epoll_create(int size)
{
	int ret_val, res;

	if(1 > size) {
		errno = EINVAL;
		return -1;
	}

	if(!hzp_alloc())
		return -1;

	/* we simply go for the maximum, our arg is just a hint */
	ret_val = pollset_create(-1);
	if(-1 == ret_val)
		return -1;

	if((res = pthread_mutex_lock(&my_epoll_wmutex))) {
		errno = res;
		logg_errno(LOGF_ERR, "locking my_epoll writes mutex");
		return -1;
	}

	if(realloc_fddata(fds->max_fd + size)) {
		pollset_destroy(ret_val);
		ret_val = -1;
	}

	if((res = pthread_mutex_unlock(&my_epoll_wmutex))) {
		errno = res;
		diedie("unlocking my_epoll writes mutex");
	}

	return ret_val;
}

int my_epoll_close(int epfd)
{
	if(0 > epfd) {
		errno = EBADF;
		return -1;
	}

	return pollset_destroy(epfd);
}

/*@unused@*/
static char const rcsid_meps[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
