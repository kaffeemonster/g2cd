/*
 * timeout.c
 * thread to handle the varios server timeouts needed
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
 * $Id: timeout.c,v 1.14 2005/11/05 09:57:22 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* System Includes */
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include "lib/my_pthread.h"
#include <errno.h>
#include <unistd.h>
#ifdef _POSIX_PRIORITY_SCHEDULING
# include <sched.h>
#endif
#ifdef DRD_ME
# include <valgrind/drd.h>
#endif
/* Own Includes */
#define _TIMEOUT_C
#include "lib/other.h"
#include "timeout.h"
#include "lib/log_facility.h"
#include "lib/hzp.h"
#include "G2Packet.h"

/* from deka to nano d    u    n */
#define DSEC2NSEC (100*1000*1000)
/* how many nsec in a sec    m      u      n */
#define ONESEC_AS_NSEC (1 * 1000 * 1000 * 1000)
/* 100 * 0.1sec */
#define DEFAULT_TIMEOUT	111

/*
 * vars
 */
static struct
{
	pthread_cond_t  cond;
	pthread_mutex_t mutex;
	struct rb_root tree;
	struct timespec time;
	bool	abort;
} wakeup;


GCC_ATTR_CONSTRUCT __init static void init_timeout_system(void)
{
	if((errno = pthread_mutex_init(&wakeup.mutex, NULL)))
		diedie("failed to setup timeout mutex");
	if((errno = pthread_cond_init(&wakeup.cond, NULL)))
		diedie("failed to setup timeout conditional");
}

/*
 * helper funcs
 */
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

static always_inline long timespec_cmp(struct timespec *a, struct timespec *b)
{
	long ret = (long)a->tv_sec - (long)b->tv_sec;
	if(ret)
		return ret;
	if((ret = (long)a->tv_nsec - (long)b->tv_nsec))
		return ret;
	return (long)(a - b);
}

static always_inline void timespec_fill_local(struct timespec *t)
{
	t->tv_sec = local_time_now;
	t->tv_nsec = 0;
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
	/* normalize */
	if(t->tv_nsec >= ONESEC_AS_NSEC) {
		t->tv_sec  += 1;
		t->tv_nsec -= ONESEC_AS_NSEC;
	}
}

static always_inline struct timeout *timeout_nearest(void)
{
	struct rb_node *n = rb_first(&wakeup.tree);
	if(!n)
		return NULL;
	return rb_entry(n, struct timeout, rb);
}

static bool timeout_rb_insert(struct timeout *t)
{
	struct rb_node **p = &wakeup.tree.rb_node;
	struct rb_node *parent = NULL;

	while(*p)
	{
		struct timeout *n = rb_entry(*p, struct timeout, rb);
		long result = timespec_cmp(&t->t, &n->t);

		parent = *p;
		if(result < 0)
			p = &((*p)->rb_left);
		else if(result > 0)
			p = &((*p)->rb_right);
		else
			return false;
	}
	rb_link_node(&t->rb, parent, p);
	rb_insert_color(&t->rb, &wakeup.tree);

	return true;
}

#if 0
static void print_tree_dot_w(struct rb_node *node, FILE *out, unsigned *nil_count)
{
	if(!node)
	{
		fprintf(out, "nil%i;\n\tnil%i [label=\"nil\"];\n", *nil_count, *nil_count);
		(*nil_count)++;
	}
	else
	{
		struct timeout *n = container_of(node, struct timeout, rb);
		fprintf(out, "\"%p\" [label=\"%p\"];\n", node, rb_parent(node));
		fprintf(out, "\t\"%p\" [label=\"%p\\n%u.%lu\\nd:%p f:%p\\nr:%i\",style=filled,color=\"%s\",fontcolor=\"%s\"]\n",
			 node, node, (unsigned) n->t.tv_sec, n->t.tv_nsec, n->data, n->fun, n->rearm_in_progress,
			 rb_is_red(node) ? "red" : "black", rb_is_red(node) ? "black" : "red");
		fprintf(out, "\t\"%p\" -> ", node);
		print_tree_dot_w(node->rb_right, out, nil_count);
		fprintf(out, "\t\"%p\" -> ", node);
		print_tree_dot_w(node->rb_left, out, nil_count);
	}
}
#endif

static void print_tree_dot(struct rb_root *root GCC_ATTR_UNUSED_PARAM, const char *when GCC_ATTR_UNUSED_PARAM)
{
#if 0
	static char path[100];
	static unsigned fcount = 0;
	FILE *out;
	unsigned nil_count = 0;
	unsigned lfc = fcount = (fcount + 1) % 10000;

	sprintf(path, "rbtreex/%u/%03u.dot", lfc / 1000, lfc % 1000);
	out = fopen(path, "w");
	fputs("digraph G {\n", out); //\n\tsize=\"20,20\"", out);
	fprintf(out, "\t\"root\" -> ");
	print_tree_dot_w(root->rb_node, out, &nil_count);
	fprintf(out, "\nlabel=\"%s\"\nrankdir=LR\n}", when);
	fclose(out);
#endif
}


/*
 * handling funcs
 */
bool timeout_add(struct timeout *new_timeout, unsigned int timeout)
{
	struct timespec now;
	bool ret_val;

	timespec_fill_local(&now);
	timespec_add(&now, timeout);

	barrier();
	pthread_mutex_lock(&wakeup.mutex);
	pthread_mutex_lock(&new_timeout->lock);

	if(RB_EMPTY_NODE(&new_timeout->rb) && !new_timeout->rearm_in_progress)
	{
		logg_develd_old("%u adding %p\n", gettid(), new_timeout);
		new_timeout->t = now;
		print_tree_dot(&wakeup.tree, "add_before_insert");
		ret_val = timeout_rb_insert(new_timeout);
		print_tree_dot(&wakeup.tree, "add_after_insert");
		if(ret_val && !timespec_after(&new_timeout->t, &wakeup.time))
			pthread_cond_broadcast(&wakeup.cond);
	} else
		ret_val = false;

	pthread_mutex_unlock(&new_timeout->lock);
	pthread_mutex_unlock(&wakeup.mutex);

	return ret_val;
}

bool timeout_advance(struct timeout *timeout, unsigned int advancement)
{
	bool ret_val = false;
	struct timespec now;

	timespec_fill_local(&now);
	timespec_add(&now, advancement);

	barrier();
	pthread_mutex_lock(&wakeup.mutex);

	if(0 != pthread_mutex_trylock(&timeout->lock))
		goto out_unlock;

	if(RB_EMPTY_NODE(&timeout->rb) || timeout->rearm_in_progress) {
		pthread_mutex_unlock(&timeout->lock);
		goto out_unlock;
	}
	logg_develd_old("%u, advancing %p\n", gettid(), timeout);
	print_tree_dot(&wakeup.tree, "advance_before_erase");
	rb_erase(&timeout->rb, &wakeup.tree);
	print_tree_dot(&wakeup.tree, "advance_after_erase_before_insert");
	RB_CLEAR_NODE(&timeout->rb);

	timeout->t = now;

	ret_val = timeout_rb_insert(timeout);
	print_tree_dot(&wakeup.tree, "advance_after_insert");

	pthread_mutex_unlock(&timeout->lock);
	if(ret_val && !timespec_after(&now, &wakeup.time))
		pthread_cond_broadcast(&wakeup.cond);

out_unlock:
	pthread_mutex_unlock(&wakeup.mutex);

	return ret_val;
}

bool timeout_cancel(struct timeout *who_to_cancel)
{
	bool ret_val = false;

retry:
	pthread_mutex_lock(&wakeup.mutex);
	pthread_mutex_lock(&who_to_cancel->lock);
	rmb();
	if(who_to_cancel->rearm_in_progress)
	{
		pthread_mutex_unlock(&who_to_cancel->lock);
		pthread_mutex_unlock(&wakeup.mutex);
		cpu_relax();
#ifdef _POSIX_PRIORITY_SCHEDULING
		sched_yield();
#endif
		goto retry;
	}

	if(!RB_EMPTY_NODE(&who_to_cancel->rb))
	{
		logg_develd_old("%u canceling %p\n", gettid(), who_to_cancel);
		print_tree_dot(&wakeup.tree, "cancel_before_erase");
		rb_erase(&who_to_cancel->rb, &wakeup.tree);
		print_tree_dot(&wakeup.tree, "cancel_after_erase");
		RB_CLEAR_NODE(&who_to_cancel->rb);
		ret_val = true;
	}
	pthread_mutex_unlock(&who_to_cancel->lock);
	pthread_mutex_unlock(&wakeup.mutex);

	return ret_val;
}

void timeout_timer_task_abort(void)
{
	wakeup.abort = true;
#ifdef DRD_ME
	/*
	 * We do not lock this mutex by purpose:
	 * this may be forbidden, racy, whatever.
	 * But:
	 * this is only called by the main thread to
	 * bring the whole thing down.
	 * If the lock is somehow hosed (deadlock,
	 * livelock, corruption, whatever), we
	 * would lock the main thread while going
	 * down.
	 */
	pthread_mutex_lock(&wakeup.mutex);
#endif
	pthread_cond_broadcast(&wakeup.cond);
#ifdef DRD_ME
	pthread_mutex_unlock(&wakeup.mutex);
#endif
}

static int kick_timeouts(void)
{
	struct timespec now;
	struct timeout *t;
	int ret_val = 1;

	/*
	 * we hold the tree lock
	 * get the time now and the nearest timeout
	 * and work on timeouts while they are
	 * below now (timed out).
	 */
	if(timespec_fill(&now))
		return ret_val;
	local_time_now = now.tv_sec;
	set_master_time(now.tv_sec);
	for(t = timeout_nearest();
	    t && !timespec_after(&t->t, &now);
	    timespec_fill(&now), t = timeout_nearest())
	{
		int ret = 0;

		/* we "have" the master clock */
		local_time_now = now.tv_sec;
		set_master_time(now.tv_sec);
		/* lock the timeout */
		pthread_mutex_lock(&t->lock);
		logg_develd_old("now: %li.%li\tto: %li.%li\n",
		                now.tv_sec, now.tv_nsec, t->t.tv_sec, t->t.tv_nsec);
		/*
		 * remove it from the tree while we locked:
		 * - the tree
		 * - the timeout
		 */
		if(!RB_EMPTY_NODE(&t->rb)) {
			print_tree_dot(&wakeup.tree, "timeout_before_erase");
			rb_erase(&t->rb, &wakeup.tree);
			print_tree_dot(&wakeup.tree, "timeout_after_erase");
			RB_CLEAR_NODE(&t->rb);
		} else {
			pthread_mutex_unlock(&t->lock);
			continue;
		}
		hzp_ref(HZP_EPOLL, t->data);
		/* release the tree lock */
		pthread_mutex_unlock(&wakeup.mutex);

		/* work on the timeout */
		if(t->fun)
			ret = t->fun(t->data);
		else
			logg_devel("empty timeout??\n");

		ret_val = 0;
		if(ret)
		{
			/*
			 * the timer wants to get rearmed. Doh!
			 * Now the shit hits the fan...
			 *
			 * We have to leave our ABBA lock problem,
			 * first and foremost, or we can not readd
			 * the timeout.
			 * But when we do this, someone canceling
			 * the timeout may be happilly continue to
			 * prop. free the timeout, and we make a
			 * use after free even if only to find out
			 * it's canceled.
			 *
			 * So make the cancelee spin on the locks
			 * so we get a chance to sneak in.
			 */
			t->rearm_in_progress = true;
			wmb();
		}
		pthread_mutex_unlock(&t->lock);
		/*
		 * release the timeout, reaquire the tree lock
		 * ===============================================================
		 * in exactly this order, or ABBA-deadlock
		 */
		pthread_mutex_lock(&wakeup.mutex);
		if(ret)
		{
			pthread_mutex_lock(&t->lock);
			if(RB_EMPTY_NODE(&t->rb))
			{
				/*
				 * and now we can add it, maybe that someone
				 * removes it imitiadly again.
				 */
				timespec_fill_local(&t->t);
				timespec_add(&t->t, ret);
				print_tree_dot(&wakeup.tree, "timeout_before_reinsert");
				timeout_rb_insert(t);
				barrier();
				print_tree_dot(&wakeup.tree, "timeout_after_reinsert");
			}
			t->rearm_in_progress = false;
			wmb();
			pthread_mutex_unlock(&t->lock);
		}
		hzp_unref(HZP_EPOLL);
	}
	/* save last time for calculating next sleep */
	wakeup.time = now;
	return ret_val;
}

void *timeout_timer_task(void *param GCC_ATTR_UNUSED_PARAM)
{
	unsigned refill_count = EVENT_SPACE / 2;
	logg(LOGF_DEBUG, "timeout_timer:\trunning\n");

#ifdef DRD_ME
	DRD_IGNORE_VAR(wakeup.abort);
	ANNOTATE_THREAD_NAME("timeout thread");
#endif

	/* make our hzp ready */
	hzp_alloc();
	g2_packet_local_alloc_init();

	server.status.all_abord[THREAD_TIMER] = true;

	g2_set_thread_name(OUR_PROC " timeout");

	pthread_mutex_lock(&wakeup.mutex);
	/* initialise time first run */
	timespec_fill(&wakeup.time);
	while(!wakeup.abort)
	{
		int ret_val;

		if(!(--refill_count)) {
			g2_packet_local_refill();
			refill_count = EVENT_SPACE / 2;
		}

#if 0
		struct timeout *t;
		t = timeout_nearest();
		if(t)
			wakeup.time = t->t;
		else {
			timespec_fill(&wakeup.time);
			timespec_add(&wakeup.time, DEFAULT_TIMEOUT);
		}
#endif
		/*
		 * We wakeup every second to set the master clock
		 * If we handle timeouts once every second, thats "fast"
		 * enough.
		 */
		wakeup.time.tv_sec++;
		wakeup.time.tv_nsec = 0;

		ret_val = pthread_cond_timedwait(&wakeup.cond, &wakeup.mutex, &wakeup.time);
		/* someone woken us */
		if(!ret_val)
		{
			logg_devel("woken\n");
			if(wakeup.abort)
				break;
		}

		if(0 == ret_val || ETIMEDOUT == ret_val)
			refill_count += kick_timeouts();
		else {
			errno = ret_val;
			logg_errnod(LOGF_ERR, "pthread_cond_timedwait failed with \"%i\" ", ret_val);
			wakeup.abort = true;
		}
	}
	pthread_mutex_unlock(&wakeup.mutex);

	logg_pos(LOGF_NOTICE, "should go down\n");
	server.status.all_abord[THREAD_TIMER] = false;
	/* clean up our hzp */
	hzp_free();
	pthread_exit(NULL);
	return(NULL);
}

/*@unused@*/
static char const rcsid_t[] GCC_ATTR_USED_VAR = "$Id: timeout.c,v 1.14 2005/11/05 09:57:22 redbully Exp redbully $";
/* EOF */
