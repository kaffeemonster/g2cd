/*
 * my_epoll_poll.c
 * wrapper to get epoll on systems providing poll
 *
 * Copyright (c) 2004, 2005,2006 Jan Seiffert
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

//TODO: Major revamp to locking...
#define CAPACITY_INC 2

struct poll_info
{
	size_t limit;
	size_t capacity;
	struct pollfd data[DYN_ARRAY_LEN];
};

struct epoll_info
{
	size_t limit;
	size_t capacity;
	struct epoll_event data[DYN_ARRAY_LEN];
};

// TODO: this implementation needs more locking
static pthread_mutex_t my_epoll_mutex;
static struct poll_info **pi_control;
static struct epoll_info **ei_control;
static int count;

	/* you better not delete this proto, or it won't work */
static void my_epoll_init(void) GCC_ATTR_CONSTRUCT;
static void my_epoll_deinit(void) GCC_ATTR_DESTRUCT;

static void my_epoll_init(void)
{
	// generate a lock for shared free_cons
	if(pthread_mutex_init(&my_epoll_mutex, NULL))
		diedie("creating my_epoll mutex");
	
	pi_control = calloc(sizeof(struct poll_info *), (size_t)EPOLL_QUEUES);
	if(!pi_control)
		diedie("allocating my_epoll memory");

	ei_control = calloc(sizeof(struct epoll_info *), (size_t)EPOLL_QUEUES);
	if(!ei_control)
		diedie("allocating my_epoll memory");
	
	count = EPOLL_QUEUES;
}

static void my_epoll_deinit(void)
{
	count = 0;
	free(ei_control);
	ei_control = NULL;
	free(pi_control);
	pi_control = NULL;
//	pthread_mutex_destroy(&my_epoll_mutex);
}

int my_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	struct poll_info *p_wptr;
	struct epoll_info *e_wptr;
	struct pollfd *poll_ptr;
	struct epoll_event *epoll_ptr;
	size_t i;
	
	if(0 > epfd)
	{
		errno = EBADF;
		return -1;
	}

	if(count < epfd || epfd == fd)
	{
		errno = EINVAL;
		return -1;
	}

	if(!ei_control[epfd] || !pi_control[epfd])
	{
		errno = EINVAL;
		return -1;
	}

	if(!event)
	{
		errno = EFAULT;
		return -1; 
	}

	p_wptr = pi_control[epfd];
	e_wptr = ei_control[epfd];

	switch(op)
	{
	case EPOLL_CTL_ADD:
		for(i = 0; i < p_wptr->limit; i++)
		{
			if(p_wptr->data[i].fd == fd)
			{
				errno = EEXIST;
				return -1;
			}
		}

		if(p_wptr->limit == p_wptr->capacity || e_wptr->limit == e_wptr->capacity)
		{
			if(!(p_wptr = realloc(p_wptr, p_wptr->capacity * CAPACITY_INC)))
			{
				errno = ENOMEM;
				return -1;
			}
			else
				pi_control[epfd] = p_wptr;	

			if(!(e_wptr = realloc(e_wptr, e_wptr->capacity * CAPACITY_INC)))
			{
				errno = ENOMEM;
				return -1;
			}
			else
				ei_control[epfd] = e_wptr;

			p_wptr->capacity *= CAPACITY_INC;
			e_wptr->capacity *= CAPACITY_INC;
		}

		p_wptr->data[p_wptr->limit].fd = fd;
		// we have mapped the epoll-events direcly to the
		// poll-events except EPOLLET, so mask it out everytime,
		// poll knows nothing about it
		p_wptr->data[p_wptr->limit].events = event->events & ~((uint32_t)EPOLLET);
		e_wptr->data[e_wptr->limit] = *event;

		p_wptr->limit++;
		e_wptr->limit++;
		return 0;
		break;
	case EPOLL_CTL_MOD:
		for(i = p_wptr->limit, poll_ptr = p_wptr->data, epoll_ptr = e_wptr->data; i; i--, poll_ptr++, epoll_ptr++)
		{
			if(poll_ptr->fd == fd)
			{
				poll_ptr->events = event->events & ~((uint32_t)EPOLLET);
				*epoll_ptr = *event;

				return 0;
			}
		}
		errno = EINVAL;
		return -1;
		break;
	case EPOLL_CTL_DEL:
		for(i = p_wptr->limit, poll_ptr = p_wptr->data, epoll_ptr = e_wptr->data; i; i--, poll_ptr++, epoll_ptr++)
		{
			if(poll_ptr->fd == fd)
			{
				// setup pointer to last array-entry
				struct pollfd *p_last = &p_wptr->data[p_wptr->limit-1];
				struct epoll_event *e_last = &e_wptr->data[e_wptr->limit-1];
					
				// if we are not removing the last entry,
				// move the other
				if(poll_ptr < p_last)
					memmove(poll_ptr, poll_ptr+1, (p_last - poll_ptr) * sizeof(*poll_ptr));

				// wiping out the last entry
				p_last->fd = 0;
				p_last->revents = 0;
				p_last->events = 0;

				// if we are not removing the last entry,
				// move the other
				if(epoll_ptr < e_last)
					memmove(epoll_ptr, epoll_ptr+1, (e_last - epoll_ptr) * sizeof(*epoll_ptr));
					
				// wiping out the last entry
				e_last->events = 0;
				e_last->data.u64 = 0;

				// remember, we now have an entry less
				p_wptr->limit--;
				e_wptr->limit--;					

				return 0;
			}
		}
	default:
		errno = EINVAL;
		return -1;
		break;
	}
	
	// just in case of
	return -1;
}

int my_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	struct pollfd *p_wptr;
	struct epoll_event *e_wptr;
	int num_poll, i, ret_val = 0, one_removed = 0;

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
	
	if(1 > maxevents || count < epfd)
	{
		errno = EINVAL;
		return -1;
	}

	if(!ei_control[epfd] || !pi_control[epfd])
	{
		errno = EINVAL;
		return -1;
	}

	do
	{
		num_poll = poll(pi_control[epfd]->data, pi_control[epfd]->limit, timeout);
		switch(num_poll)
		{
		default:
			for(p_wptr = pi_control[epfd]->data, e_wptr = ei_control[epfd]->data, i = pi_control[epfd]->limit; i; p_wptr++, e_wptr++, i--)
			{
				if(!p_wptr->revents) continue;

				if(!(p_wptr->revents & POLLNVAL))
				{
					events->events =  p_wptr->revents;
					events->data = e_wptr->data;
					events++;

					ret_val++;
					if(!--maxevents)
						break;
				}
				else
				{
					// setup pointer to last array-entry
					struct pollfd *p_last = &pi_control[epfd]->data[pi_control[epfd]->limit-1];
					struct epoll_event *e_last = &ei_control[epfd]->data[ei_control[epfd]->limit-1];

					logg_develd("%p, %p, %i\n", (void *)(pi_control[epfd]->data + pi_control[epfd]->limit), (void *)p_wptr, (pi_control[epfd]->data + pi_control[epfd]->limit) - p_wptr);

					// if we are not removing the last entry,
					// move the other
					if(p_wptr < p_last)
						memmove(p_wptr, p_wptr+1, (p_last - p_wptr) * sizeof(*p_wptr));

					// wiping out the last entry
					p_last->fd = 0;
					p_last->revents = 0;
					p_last->events = 0;

					// if we are not removing the last entry,
					// move the other
					if(e_wptr < e_last)
						memmove(e_wptr, e_wptr+1, (e_last - e_wptr) * sizeof(*e_wptr));
					
					// wiping out the last entry
					e_last->events = 0;
					e_last->data.u64 = 0;

					// remember, we now have an entry less
					pi_control[epfd]->limit--;
					ei_control[epfd]->limit--;

					// adjust working-pointer, since we have
					// moved the entries
					p_wptr--;
					e_wptr--;

					one_removed = 1;
				}

				if(!--num_poll)
					break;
			}
			break;
		case 0:
			break;
		case -1:
			ret_val = -1;
			if(EINTR != errno)
				logg_errno(LOGF_DEBUG, "poll");
			break;
		}
	} while(one_removed && 0 == ret_val);
	
	return ret_val;
}

int my_epoll_create(int size)
{
	int efd, i;

	if(!size)
		return -1;

	if(!pthread_mutex_lock(&my_epoll_mutex))
	{
		for(i = 0; i < count; i++)
		{
			if(ei_control[i] || pi_control[i])
				continue;

			efd = i;

			pi_control[i] = calloc(1, sizeof(*pi_control) + sizeof(*(*pi_control)->data) * (size_t)size);
			if(!pi_control[i])
			{
				logg_errno(LOGF_ERR, "allocating my_epoll memory");
				errno = ENOMEM;
				efd = -1;
			}
			else
			{
				pi_control[i]->capacity = (size_t)size;

				ei_control[i] = calloc(1, sizeof(*ei_control) + sizeof(*(*ei_control)->data) * (size_t)size);
				if(!ei_control[i])
				{
					logg_errno(LOGF_ERR, "allocating my_epoll memory");
					free(pi_control[i]);
					errno = ENOMEM;
					pi_control[i] = NULL;
					efd = -1;
				}
				else
					ei_control[i]->capacity = (size_t)size;
			}

			if(pthread_mutex_unlock(&my_epoll_mutex))
				diedie("unlocking my_epoll mutex");
			return efd;
		}

		/* just in case we come here */
		if(pthread_mutex_unlock(&my_epoll_mutex))
			diedie("unlocking my_epoll mutex");

		return -1;
	}
	else
	{
		logg_errno(LOGF_ERR, "locking my_epoll mutex");
		return -1;
	}
}

int my_epoll_close(int epfd)
{
	if(0 > epfd)
	{
		errno = EBADF;
		return -1;
	}

	if(count < epfd)
	{
		errno = EINVAL;
		return -1;
	}

	if(!pthread_mutex_lock(&my_epoll_mutex))
	{
		if(ei_control[epfd] && pi_control[epfd])
		{
			free(pi_control[epfd]);
			
			pi_control[epfd] = NULL;

			free(ei_control[epfd]);
				
			ei_control[epfd] = NULL;

			if(pthread_mutex_unlock(&my_epoll_mutex))
				diedie("unlocking my_epoll mutex");
			else
				return 0;
		}
		else
			errno = EINVAL;

		if(pthread_mutex_unlock(&my_epoll_mutex))
			diedie("unlocking my_epoll mutex");

		return -1;
	}
	else
	{
		logg_errno(LOGF_ERR, "locking my_epoll mutex");
		return -1;
	}
}

static char const rcsid_mei[] GCC_ATTR_USED_VAR = "$Id: $";
