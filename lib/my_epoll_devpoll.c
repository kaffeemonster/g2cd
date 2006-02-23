/*
 * my_epoll_kepoll.c
 * wrapper to get epoll on solaris with /dev/poll
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
static volatile struct dev_poll_data *fds;

/*
 * Must be called with the writerlock held!
 */
static int realloc_fddata(int new_max)
{
	struct dev_poll_data *old_data = deatomic(fds);
	int old_max = old_data->max_fd; /* we are under writer lock */
	struct dev_poll_data *tmp_data = malloc(sizeof(*tmp_data) + (sizeof(epoll_data_t) * (new_max + 1)));
	if(!tmp_data)
	{
		logg_errno(LOGF_ERR, "allocating fd_data memory");
		errno = ENOMEM;
		return -1;
	}

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
static void my_epoll_init(void)
{
	int tmp_fd;

	/* generate a lock for shared free_cons */
	if(pthread_mutex_init(&my_epoll_wmutex, NULL))
	{
		logg_errno(LOGF_CRIT, "creating my_epoll writes mutex");
		exit(EXIT_FAILURE);
	}
	
	fds = malloc(sizeof(*fds) + (sizeof(epoll_data_t) * ((size_t)EPOLL_QUEUES * 20 + 1)));
	if(!fds)
	{
		logg_errno(LOGF_CRIT, "allocating my_epoll memory");
		exit(EXIT_FAILURE);
	}
	
	for(tmp_fd = 0; tmp_fd <= EPOLL_QUEUES * 20; tmp_fd++)
		fds->data[tmp_fd].u64 = EPDATA_POISON;	
	fds->max_fd = EPOLL_QUEUES * 20;
}

int my_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	int ret_val = 0;

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

	/* check for sane op before accuaring the lock */
	if(epfd == fd || !(EPOLL_CTL_ADD == op || EPOLL_CTL_MOD == op || EPOLL_CTL_DEL == op))
	{
		errno = EINVAL;
		return -1;
	}

	if(!pthread_mutex_lock(&my_epoll_wmutex))
	{
		if(fd > fds->max_fd)
		{
			if(EPOLL_CTL_ADD == op)
			{
					if(realloc_fddata(fd + CAPACITY_INCREMENT))
						goto UNLOCK;
			}
			else
			{
				errno   = ENOENT;
				ret_val = -1;
				goto UNLOCK;
			}
		}

		if(EPDATA_POISON == fds->data[fd].u64)
		{
			if(EPOLL_CTL_ADD == op)
			{
				struct pollfd tmp_poll;
				ssize_t w_val;

				tmp_poll.fd     = fd;
				tmp_poll.events = event->events;
				tmp_poll.revents = 0;

				while(sizeof(tmp_poll) != (w_val = pwrite(epfd, &tmp_poll, sizeof(tmp_poll), 0)) && EINTR == errno);
				if(sizeof(tmp_poll) == w_val)
					fds->data[fd] = event->data;
				else
				{
					logg_errno(LOGF_ERR, "adding new fd to /dev/poll");
					ret_val = -1;
				}
			}
			else
			{
				errno   = ENOENT;
				ret_val = -1;
			}
		}
		else
		{
			if(EPOLL_CTL_ADD == op)
			{
				errno = EEXIST;
				ret_val = -1;
			}
			else
			{
				/* remove the fd for both CTL_MOD and CTL_DEL */
				struct pollfd tmp_poll;
				ssize_t w_val;

				tmp_poll.fd      = fd;
				tmp_poll.events  = POLLREMOVE;
				tmp_poll.revents = 0;

				while(sizeof(tmp_poll) != (w_val = pwrite(epfd, &tmp_poll, sizeof(tmp_poll), 0)) && EINTR == errno);
				if(sizeof(tmp_poll) == w_val)
					fds->data[fd].u64 = EPDATA_POISON;
				else
				{
					logg_errno(LOGF_ERR, "removing fd to /dev/poll");
					ret_val = -1;
				}

				if(EPOLL_CTL_MOD == op)
				{
					tmp_poll.events = event->events;
					while(sizeof(tmp_poll) != (w_val = pwrite(epfd, &tmp_poll, sizeof(tmp_poll), 0)) && EINTR == errno);
					if(sizeof(tmp_poll) == w_val)
						fds->data[fd] = event->data;
					else
					{
						logg_errno(LOGF_ERR, "readding modified fd to /dev/poll");
						ret_val = -1;
					}
				}
				else if(EPOLL_CTL_DEL != op)
					logg_pos(LOGF_CRIT, "my_epoll op not in ADD|MOD|DEL?! This should not happen...");
			}
		}

UNLOCK:
		if(pthread_mutex_unlock(&my_epoll_wmutex))
		{
			logg_errno(LOGF_CRIT, "unlocking my_epoll writers mutex");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		logg_errno(LOGF_ERR, "locking my_epoll writers mutex");
		ret_val = -1;
	}
	
	return ret_val;
}

int my_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	struct pollfd *poll_buf;
	struct epoll_event *wptr = events;
	struct dvpoll do_poll;
	int num_poll, ret_val = 0;

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

	if(!(poll_buf = alloca((size_t)maxevents * sizeof(*poll_buf))))
	{
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
		/* accuire a reference on the data array */
		do
			hzp_ref(HZP_EPOLL, (loc_data = deatomic(fds)));
		while(loc_data != fds);

		for(; num_poll; num_poll--, wptr++, poll_buf++)
		{
			/* totaly bogus fd, close it */
			if(poll_buf->fd > loc_data->max_fd)
			{
				close(poll_buf->fd);
				logg_pos(LOGF_INFO, "hit FD out of lockup arry?")
				continue;
			}
// TODO: handle POLLNVAL, epoll does not deliver it 
//			if(!(p_wptr->revents & POLLNVAL))

			wptr->data = loc_data->data[poll_buf->fd];
			wptr->events = poll_buf->revents;
			ret_val++;
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

	if(1 > size)
		return -1;

	if(!hzp_alloc())
		return -1;
	
	if(0 > (dpfd = open("/dev/poll", O_RDWR)))
		return -1;

	if(!pthread_mutex_lock(&my_epoll_wmutex))
	{
		if(realloc_fddata(fds->max_fd + size))
		{
			close(dpfd);
			dpfd = -1;
		}

		if(pthread_mutex_unlock(&my_epoll_wmutex))
		{
			logg_errno(LOGF_CRIT, "unlocking my_epoll writes mutex");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		logg_errno(LOGF_ERR, "locking my_epoll writes mutex");
		close(dpfd);
		return -1;
	}

	return dpfd;
}

int my_epoll_close(int epfd)
{
	
	if(0 > epfd)
	{
		errno = EBADF;
		return -1;
	}

	return close(epfd);
}

static char const rcsidi[] GCC_ATTR_USED_VAR = "$Id: $";
