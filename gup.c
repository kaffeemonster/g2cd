/*
 * gup.c
 * grand unified poller
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* System includes */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "lib/my_pthread.h"
#include <errno.h>
#include <unistd.h>
/* System net-includes */
#include <sys/types.h>
#include <sys/stat.h>
#include "lib/my_epoll.h"
#include "lib/combo_addr.h"
#include <fcntl.h>
/* other */
#include "lib/other.h"
/* Own includes */
#define GUP_C
#include "gup.h"
#include "G2MainServer.h"
#include "G2Acceptor.h"
#include "G2Handler.h"
#include "G2UDP.h"
#include "G2ConHelper.h"
#include "lib/sec_buffer.h"
#include "lib/recv_buff.h"
#include "lib/log_facility.h"
#include "lib/hzp.h"
#include "lib/my_bitops.h"

/* data-structures */
static struct worker_sync {
	int epollfd;
	int from_main;
	char padding[128 - 2 * sizeof(int)]; /* move at least a cacheline between data */
	volatile bool wait;
	volatile bool keep_going;
} worker;

static void clean_up_gup(int);
static bool handle_abort(struct simple_gup *, struct epoll_event *);

static void *gup_loop(void *param)
{
	struct norm_buff *lbuff[MULTI_RECV_NUM];
	g2_connection_t *lcon = NULL;
	int num_poll = 0;
	int index_poll = 0;
	unsigned i;

	unsigned refill_count = EVENT_SPACE / 2;
	bool lcon_refresh_needed = false;
	/* make our hzp ready */
	hzp_alloc();
	g2_packet_local_alloc_init();
	recv_buff_local_refill();

	/* fill receive buffer */
	memset(lbuff, 0, sizeof(lbuff));
	for(i = 0; i < MULTI_RECV_NUM; i++)
	{
		lbuff[i] = recv_buff_local_get();
		if(!lbuff[i]) {
			logg_errno(LOGF_ERR, "allocating local buffer");
			worker.keep_going = false;
			goto out;
		}
	}
	num_poll = 0;
	/* fill connection buffer */
	lcon = g2_con_get_free();
	if(!lcon) {
		logg_errno(LOGF_ERR, "allocating local conection");
		worker.keep_going = false;
		goto out;
	}

	my_snprintf(lbuff[0]->data, lbuff[0]->limit, OUR_PROC " guppie %ti", (intptr_t)param);
	g2_set_thread_name(lbuff[0]->data);

	while(worker.keep_going)
	{
// TODO: maybe take 4, or more?
		struct epoll_event eevents[3];
		union gup *guppie;
		struct epoll_event *ev = &eevents[0];
		bool repoll = true, kg;

		if(lcon_refresh_needed) {
			g2_con_clear(lcon);
			lcon_refresh_needed = false;
		}
		for(i = 0; i < MULTI_RECV_NUM; i++) {
			if(!lbuff[i])
				lbuff[i] = recv_buff_local_get();
		}
		if(!(--refill_count)) {
			recv_buff_local_refill();
			g2_packet_local_refill();
			refill_count = EVENT_SPACE / 2;
		}

		if(!worker.keep_going)
			break;

		/*
		 * Originaly we let one worker thread poll, retrieving a big
		 * batch of events and then hand out the events one by one
		 * to all the worker threads.
		 *
		 * This gives low response times and "even" distribution.
		 * But is prop. only scalable with low event rates.
		 *
		 * At high event rates it is better to hand out batches,
		 * or in other words: A thread polls and wanders off
		 * with _all_ results, the next threads polls for the
		 * next batch of results etc..
		 * This is much better if we are under heavy fire.
		 * Ideally we want both, but switching this at runtime...
		 *
		 * And i removed it, because:
		 * This included a lock and a condidional (that was a
		 * little bit superflous, but to help with thundering
		 * herds).
		 * Unfortunatly, when not running with one worker or/and
		 * at high event rates this gave quite a lock firework.
		 * (Which is not bad perse, somewhere there has to be a
		 * lock, at least in the kernel to make the syscall
		 * concurrent save).
		 * But to make matters worse reality is a bitch:
		 * When a lock can not be taken in userspace the
		 * libpthread has to fall back to some kernel support
		 * to get the sleeping, and conditionals are an extra
		 * complicated beast.
		 *
		 * Under (2.6) Linux this is where futexes come into
		 * play. They are the "new" fast kernel support for such
		 * things. But locking is hard and since this was new
		 * from the ground up there where some bugs in obscure
		 * corner cases (normally cases not used by libpthread,
		 * stuff if you would use the syscall directly with
		 * rigged parameters), fixed in later kernel versions.
		 *
		 * Still i managed to run into one on an old 2.6.9-vserver
		 * when the 4 core machine got busy (no cputime usage,
		 * but high lock rates).
		 * Total userspace lockup, you can still ping the machine,
		 * tcp connections still 3-way handshaked (so the kernel
		 * is alive and kicking), but userspace is dead, ALL of
		 * userspace, you can not even ssh into the machine.
		 *
		 * Thats why i hate those enterprise distros, outdated
		 * bullshit, seldomly updated by its customers, and to
		 * totally screw you:
		 * Propritary extentions like virtualisation which can
		 * not be updated.
		 * That stuff "works", it not like its totally broken. Lot's
		 * of machines fullfilling their dutys, mail, web, etc.
		 *
		 * But apparently i'm pushing the limits far beyond...
		 *
		 * So we let the threads directly run into the kernel, he
		 * should manage the locking.
		 * Our epoll emulations have to do the locking if they
		 * need one.
		 * We take 3 events (which may hurt the poll based
		 * emulation badly), to not wander off for to long, but
		 * still "suck up" the worst offending events, udp & accept
		 * while still consuming at least one event from a tcp
		 * connection.
		 *
		 * And it works beautifull, the kernel "round robins"
		 * the threads as early as an event arrives giving nice
		 * concurency.
		 * Oneshot mode was the right decission.
		 */
//TODO: rethink event distribution
		do
		{
			if(!worker.keep_going)
				goto out;

			if(index_poll >= num_poll) {
				num_poll = my_epoll_wait(worker.epollfd, eevents, anum(eevents), 10000);
				if(0 < num_poll)
					update_local_time();
				index_poll = 0;
			}
			repoll = false;
			switch(num_poll)
			{
			default:
				ev = &eevents[index_poll++];
				break;
			/* Something bad happened */
			case -1:
				if(EINTR != errno)
				{
					/* Print what happened */
					logg_errno(LOGF_ERR, "poll");
					/* and get out here (at the moment) */
					worker.keep_going = false;
					break;
				}
			/* Nothing happened (or just the Timeout) */
			case 0:
				repoll = true;
				break;
			}
		} while(repoll);

		/* use result */
		guppie = ev->data.ptr;
		if(unlikely(!guppie)) {
			logg_devel("no guppie?");
			continue;
		}
		kg = true;
		switch(guppie->gup)
		{
		case GUP_G2CONNEC_HANDSHAKE:
			handle_con_a(ev, lbuff, worker.epollfd);
			break;
		case GUP_G2CONNEC:
			handle_con(ev, lbuff, worker.epollfd);
			break;
		case GUP_ACCEPT:
			if(ev->events & (uint32_t)EPOLLIN)
			{
				if(handle_accept_in(&guppie->s_gup, lcon, worker.epollfd)) {
					lcon = g2_con_get_free();
					if(unlikely(!lcon))
						kg = false;
//TODO: a master abort is not the best way...
				} else
					lcon_refresh_needed = true;
			}
			if(ev->events & ~((uint32_t)EPOLLIN|EPOLLONESHOT))
				kg = handle_accept_abnorm(&guppie->s_gup, ev, worker.epollfd);
			break;
		case GUP_UDP:
			{
				for(i = 0; i < MULTI_RECV_NUM; i++) {
					if(!lbuff[i])
						lbuff[i] = recv_buff_local_get(); /* try to fill in missing buffers */
				}
				handle_udp(ev, lbuff, worker.epollfd);
			}
			break;
		case GUP_ABORT:
			kg = handle_abort(&guppie->s_gup, ev);
			break;
		}
		if(!kg && worker.keep_going) {
			ssize_t w_res;
			worker.keep_going = false;
			do {
				w_res = write(worker.from_main, "stop!", str_size("stop!"));
			} while(str_size("stop!") != w_res && EINTR == errno);
			break;
		}
	}

out:
	g2_con_free(lcon);
	for(i = 0; i < MULTI_RECV_NUM; i++)
		recv_buff_local_ret(lbuff[i]);
	recv_buff_local_free();

	/* clean up our hzp */
	hzp_free();
	if(param)
		pthread_exit(NULL);
	/* only the creator thread should get out here */
	return NULL;
}

void *gup(void *param)
{
	struct epoll_event eevents;
	static pthread_t *helper;
	static struct simple_gup from_main;
	size_t num_helper, i;

	from_main.gup = GUP_ABORT;
	from_main.fd = *((int *)param);
	worker.from_main = *(((int *)param)-1);
	logg(LOGF_INFO, "gup:\t\tOur SockFD -> %d\tMain SockFD -> %d\n", from_main.fd, *(((int *)param)-1));

	worker.epollfd = my_epoll_create(PD_START_CAPACITY);
	if(0 > worker.epollfd) {
		logg_errno(LOGF_ERR, "creating epoll-fd");
		goto out;
	}

	eevents.events   = (uint32_t)(EPOLLIN|EPOLLONESHOT);
	eevents.data.u64 = 0;
	eevents.data.ptr = &from_main;
	if(0 > my_epoll_ctl(worker.epollfd, EPOLL_CTL_ADD, from_main.fd, &eevents))
	{
		logg_errno(LOGF_ERR, "adding main pipe to epoll");
		goto out_epoll;
	}

	num_helper  = server.settings.num_threads;
	if(!num_helper)
	{
		num_helper = get_cpus_online();
		/*
		 * if we only have a single CPU, 99.999% of the time locks
		 * will be uncontended and programm flow is near linear.
		 * Keep it that way by only using a single worker (this thread).
		 *
		 * When we have more CPUs, oversubcribe by one thread
		 * (num_cpu + ourself) to have a thread ready running if a thread
		 * blocks another on a lock.
		 */
		num_helper = 1 == num_helper ? 0 : num_helper;
	}
	if(num_helper)
	{
		helper = malloc(num_helper * sizeof(*helper));
		if(!helper) {
			logg_errno(LOGF_CRIT, "No mem for gup helper threads, will run with one");
			num_helper = 0;
		}
	}
	/*
	 * init udp first, if it fails to set the port right for ipv6
	 * we have to disable ipv6
	 */
	if(!init_udp(worker.epollfd))
		goto out_no_udp;
	if(!init_accept(worker.epollfd))
		goto out_no_accept;

	worker.keep_going = true;
	for(i = 0; i < num_helper; i++)
	{
		if(pthread_create(&helper[i], &server.settings.t_def_attr, gup_loop, (void *)(i + 1))) {
			logg_errnod(LOGF_WARN, "starting gup helper threads, will run with %zu", i);
			num_helper = i;
			break;
		}
	}

	/* we are up and running */
	server.status.all_abord[THREAD_GUP] = true;
	/* we become one of them, only the special one which cleans up */
	gup_loop(NULL);

	for(i = 0; i < num_helper; i++)
	{
		if(pthread_join(helper[i], NULL)) {
			logg_errno(LOGF_WARN, "taking down gup helper threads");
			break;
		}
	}

	clean_up_accept();
out_no_accept:
	clean_up_udp();
out_no_udp:
	clean_up_gup(from_main.fd);
	free(helper);
out_epoll:
	my_epoll_close(worker.epollfd);
out:
	close(from_main.fd);
	pthread_exit(NULL);
	/* to avoid warning about reaching end of non-void function */
	return NULL;
}

/*
 * Epoll manipulators
 */
int accept_timeout(void *arg)
{
	struct epoll_event p_entry = {0,{0}};
	g2_connection_t *con = arg;

	logg_develd_old("%p is inactive for %lus! last: %lu\n",
	                con, time(NULL) - con->last_active, con->last_active);

	p_entry.data.u64 = 0;
	p_entry.data.ptr = con;
	shortlock_t_lock(&con->pts_lock);
	con->flags.dismissed = true;
// TODO: also try to teardown?
	/* recheck with rsp. to the conreg & locking on accept */
	p_entry.events = con->poll_interrests |= (uint32_t)EPOLLOUT;
	shortlock_t_unlock(&con->pts_lock);
	my_epoll_ctl(worker.epollfd, EPOLL_CTL_MOD, con->com_socket, &p_entry);
	return 0;
}

int handler_active_timeout(void *arg)
{
	struct epoll_event p_entry = {0,{0}};
	g2_connection_t *con = arg;
	int ret_val = 0;

	logg_develd_old("%p is inactive for %lus! last: %lu\n",
	                con, time(NULL) - con->last_active, con->last_active);

	p_entry.data.ptr = con;
	if(local_time_now >= (con->last_active + (3 * HANDLER_ACTIVE_TIMEOUT)))
	{
		logg_develd("run into timeout for %p#I\n", &con->remote_host);
		if(EBUSY == pthread_mutex_trylock(&con->lock))
		{
			/*
			 * the connection is already locked, we can not tear it down,
			 * or we would face a deadlock vs. timer handling, retry in a second
			 */
			ret_val = 10;
			shortlock_t_lock(&con->pts_lock);
			con->flags.dismissed = true;
			p_entry.events = con->poll_interrests |= (uint32_t)EPOLLOUT;
			shortlock_t_unlock(&con->pts_lock);
		}
		else
		{
			/* we have the con, tear it down */
			logg_devel("direct teardown\n");
			teardown_con(con, worker.epollfd);
			/* we hold the hzp ref on it */
			pthread_mutex_unlock(&con->lock);
			return 0;
		}
	}
	else
	{
		g2_packet_t *pi = g2_packet_calloc();

		ret_val = HANDLER_ACTIVE_TIMEOUT;
		if(!pi)
			return ret_val;
		pi->type = PT_PI;
		shortlock_t_lock(&con->pts_lock);
		list_add_tail(&pi->list, &con->packets_to_send);
		p_entry.events = con->poll_interrests |= (uint32_t)EPOLLOUT;
		shortlock_t_unlock(&con->pts_lock);
	}
	my_epoll_ctl(worker.epollfd, EPOLL_CTL_MOD, con->com_socket, &p_entry);
	return ret_val;
}

int handler_z_flush_timeout(void *arg)
{
	struct epoll_event p_entry = {0,{0}};
	g2_connection_t *con = arg;

	p_entry.data.ptr = con;
	con->u.handler.z_flush = true;
	wmb();
	shortlock_t_lock(&con->pts_lock);
	p_entry.events = con->poll_interrests |= (uint32_t)EPOLLOUT;
	shortlock_t_unlock(&con->pts_lock);
	my_epoll_ctl(worker.epollfd, EPOLL_CTL_MOD, con->com_socket, &p_entry);
	return 0;
}

void g2_handler_con_mark_write(struct g2_packet *p, struct g2_connection *con)
{
	struct epoll_event p_entry = {0,{0}};

	p_entry.data.ptr = con;

	/*
	 * MB:
	 * This spinlock does not only protect the list,
	 * it also barriers our packet creation before another
	 * cpu reads the packets to write them to the socket
	 */
	shortlock_t_lock(&con->pts_lock);

	list_add_tail(&p->list, &con->packets_to_send);
	p_entry.events = con->poll_interrests |= (uint32_t)EPOLLOUT;

	shortlock_t_unlock(&con->pts_lock);
	my_epoll_ctl(worker.epollfd, EPOLL_CTL_MOD, con->com_socket, &p_entry);
}

void gup_teardown_con(struct g2_connection *con)
{
	return teardown_con(con, worker.epollfd);
}

void gup_con_mark_write(struct g2_connection *con)
{
	struct epoll_event p_entry = {0,{0}};

	p_entry.data.ptr = con;

	/*
	 * MB:
	 * This spinlock does not only protect the list,
	 * it also barriers our packet creation before another
	 * cpu reads the packets to write them to the socket
	 */
	shortlock_t_lock(&con->pts_lock);

	p_entry.events = con->poll_interrests |= (uint32_t)EPOLLOUT;

	shortlock_t_unlock(&con->pts_lock);
	my_epoll_ctl(worker.epollfd, EPOLL_CTL_MOD, con->com_socket, &p_entry);
}

/*
 * Utility
 */
static bool handle_abort(struct simple_gup *sg, struct epoll_event *ev)
{
	bool ret_val = true;
// TODO: Check for a proper stop-sequence ??
	/* everything but 'ready for write' means: we're finished... */
	if(ev->events & ~((uint32_t)EPOLLOUT|EPOLLONESHOT))
		ret_val = false;
	/* stop this write interrest and retrigger the oneshot socket to wake other */
	ev->events = (uint32_t)(EPOLLIN|EPOLLONESHOT);
	if(0 > my_epoll_ctl(worker.epollfd, EPOLL_CTL_MOD, sg->fd, ev)) {
		logg_errno(LOGF_ERR, "changing epoll-interrests for socket-2-main");
		ret_val = false;
	}
	return ret_val;
}

static void clean_up_gup(int who_to_say)
{
	if(0 > send(who_to_say, "All lost", sizeof("All lost"), 0))
		diedie("initiating stop"); /* hate doing this, but now it's to late */
	logg_pos(LOGF_NOTICE, "should go down\n");

	server.status.all_abord[THREAD_GUP] = false;
}

/*@unsused@*/
static char const rcsid_gup[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
