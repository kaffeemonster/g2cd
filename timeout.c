/*
 * timeout.c
 * thread to handle the varios server timeouts needed
 *
 * Copyright (c) 2004,2008, Jan Seiffert
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
 * $Id: timeout.c,v 1.14 2005/11/05 09:57:22 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
// System Includes
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
// Own Includes
#define _TIMEOUT_C
#include "other.h"
#include "timeout.h"
#include "lib/log_facility.h"

/* from deka to nano d    u    n */
#define DSEC2NSEC (100*1000*1000)
/* 100 * 0.1sec */
#define DEFAULT_TIMEOUT	100
/*  */
#define TWHEEL_SLOTS 10

/* vars */
static struct
{
	pthread_cond_t	cond;
	pthread_mutex_t	mutex;
	struct timespec	time;
	bool	abort;
} wakeup = {
	.cond  = PTHREAD_COND_INITIALIZER,
	.mutex = PTHREAD_MUTEX_INITIALIZER,
};

static struct
{
	pthread_mutex_t	mutex;
	struct list_head	slot;
} twheel[TWHEEL_SLOTS];

/* helper funcs */
static always_inline unsigned int to2sec(unsigned int timeout)
{
	return timeout / 10;
}

static always_inline unsigned long to2nsec(unsigned int timeout)
{
	return ((unsigned long)(timeout % 10)) * DSEC2NSEC;
}

static always_inline unsigned long usec2nsec(unsigned long usec)
{
	return usec * 1000;
}

static always_inline int delta2index(long delta)
{
	int i;

	if(delta <= 0)
		i = 0;
	else if(delta <= 5)
		i = 1;
	else if(delta <= 12)
		i = 2;
	else if(delta <= 25)
		i = 3;
	else if(delta <= 45)
		i = 4;
	else if(delta <= 60)
		i = 5;
	else if(delta <= 120)
		i = 6;
	else if(delta <= 180)
		i = 7;
	else if(delta <= 300)
		i = 8;
	else
		i = 9;

	return i;
}

static always_inline long timespec_delta_sec(struct timespec *a, struct timespec *b)
{
	return (long)a->tv_sec - b->tv_sec;
}

static always_inline bool timespec_before(struct timespec *a, struct timespec *b)
{
	if(a->tv_sec < b->tv_sec)
		return true;
	if(a->tv_nsec < b->tv_nsec)
		return true;
	return false;
}

static always_inline bool timespec_after(struct timespec *a, struct timespec *b)
{
	if(a->tv_sec > b->tv_sec)
		return true;
	if(a->tv_nsec >= b->tv_nsec)
		return true;
	return false;
}

static always_inline int timespec_fill(struct timespec *t)
{
	struct timeval x;
	int ret_val;

	ret_val = gettimeofday(&x, NULL);
	if(ret_val)
		return ret_val;

	t->tv_sec = x.tv_sec;
	t->tv_nsec = usec2nsec(x.tv_usec);

	return ret_val;
}

static always_inline void timespec_add(struct timespec *t, unsigned int timeout)
{
	t->tv_sec  += to2sec(timeout);
	t->tv_nsec += to2nsec(timeout);
}

/* handling funcs */ 
bool timeout_add(struct timeout *new_timeout, unsigned int timeout)
{
	struct timespec now;
	struct list_head *p;
	int i;

	if(timespec_fill(&now))
		return false;
	new_timeout->t = now;
	timespec_add(&new_timeout->t, timeout);

	i = delta2index(timespec_delta_sec(&new_timeout->t, &now));

	if(pthread_mutex_lock(&twheel[i].mutex))
		return false;

	if(!list_empty(&twheel[i].slot))
	{
		list_for_each(p, &twheel[i].slot)
		{
			struct timeout *e = list_entry(p, struct timeout, l);
			if(timespec_after(&e->t, &new_timeout->t))
			{
				list_add_tail(&new_timeout->l, p);
				break;
			}
		}
	}
	else
		list_add(&new_timeout->l, &twheel[i].slot);

	if(pthread_mutex_unlock(&twheel[i].mutex))
		diedie("ahhhh, timer wheel stuck!");

	return true;
}

bool timeout_cancel(GCC_ATTR_UNUSED_PARAM(struct timeout *, who_to_cancel))
{
	struct timespec now;

	timespec_fill(&now);

	return true;
}

void timeout_timer_task_abort(void)
{
	wakeup.abort = true;
	pthread_cond_broadcast(&wakeup.cond);
}

void *timeout_timer_task(GCC_ATTR_UNUSED_PARAM(void *, param))
{
	int i;
	for(i = 0; i < TWHEEL_SLOTS; i++)
		INIT_LIST_HEAD(&twheel[i].slot);

	logg(LOGF_DEBUG, "timeout_timer:\trunning\n");

	server.status.all_abord[THREAD_TIMER] = true;

	while(!wakeup.abort)
	{
		int ret_val;

		pthread_mutex_lock(&wakeup.mutex);

		timespec_fill(&wakeup.time);
		timespec_add(&wakeup.time, DEFAULT_TIMEOUT);

		ret_val = pthread_cond_timedwait(&wakeup.cond, &wakeup.mutex, &wakeup.time);
		pthread_mutex_unlock(&wakeup.mutex);

		/* someone woken us */
		if(!ret_val)
		{
			logg_devel("woken\n");
		}
		else if(ETIMEDOUT == ret_val)
		{
			logg_devel_old("timeout\n");
		}
		else
		{
			errno = ret_val;
			logg_errnod(LOGF_ERR, "pthread_cond_timedwait failed with \"%i\" ", ret_val);
			wakeup.abort = true;
		}
	}

	logg_pos(LOGF_NOTICE, "should go down\n");
	server.status.all_abord[THREAD_TIMER] = false;	
	pthread_exit(NULL);
	return(NULL);
}

static char const rcsid_t[] GCC_ATTR_USED_VAR = "$Id: timeout.c,v 1.14 2005/11/05 09:57:22 redbully Exp redbully $";
// EOF
