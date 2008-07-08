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

/* helper funcs */
static always_inline int to2sec(int timeout)
{
	return timeout / 10;
}

static always_inline long to2nsec(int timeout)
{
	return ((long)(timeout % 10)) * DSEC2NSEC;
}

static always_inline long usec2nsec(long usec)
{
	return usec * 1000;
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

static always_inline void timespec_add(struct timespec *t, int timeout)
{
	t->tv_sec  += to2sec(timeout);
	t->tv_nsec += to2nsec(timeout);
}

/* handling funcs */ 
inline bool add_timeout(GCC_ATTR_UNUSED_PARAM(to_id *, new_timeout))
{
	return true;
}

inline bool cancel_timeout(GCC_ATTR_UNUSED_PARAM(const to_id, who_to_cancel))
{
	return true;
}

void timeout_timer_abort(void)
{
	wakeup.abort = true;
	pthread_cond_broadcast(&wakeup.cond);
}

void *timeout_timer_task(GCC_ATTR_UNUSED_PARAM(void *, param))
{
	logg(LOGF_DEBUG, "timeout_timer:\trunning\n");

	server.status.all_abord[THREAD_TIMER] = true;

	while(!wakeup.abort)
	{
		int ret_val;

		pthread_mutex_lock(&wakeup.mutex);

		timespec_fill(&wakeup.time);
		timespec_add(&wakeup.time, DEFAULT_TIMEOUT);

		ret_val = pthread_cond_timedwait(&wakeup.cond, &wakeup.mutex, &wakeup.time);

		/* someone woken us */
		if(!ret_val)
		{
			pthread_mutex_unlock(&wakeup.mutex);
			logg_devel("woken\n");
		}
		else if(ETIMEDOUT == ret_val)
		{
			pthread_mutex_unlock(&wakeup.mutex);
			logg_devel_old("timeout\n");
		}
		else
		{
			pthread_mutex_unlock(&wakeup.mutex);
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
