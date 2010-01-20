/*
 * gup.c
 * grand unified poller
 *
 * Copyright (c) 2004-2010 Jan Seiffert
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* System includes */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
/* System net-includes */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
#include "lib/my_epoll.h"
#include "lib/sec_buffer.h"
#include "lib/recv_buff.h"
#include "lib/log_facility.h"
#include "lib/hzp.h"
#include "lib/my_bitops.h"

/* data-structures */
static struct worker_sync {
	int epollfd;
	char padding[128]; /* move at least a cacheline between data */
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int num_poll;
	int index;
	struct epoll_event eevents[EVENT_SPACE];
	volatile bool wait;
	volatile bool keep_going;
} worker;

static void clean_up_gup(int);
static bool handle_abort(struct simple_gup *, struct epoll_event *);

static void *gup_loop(void *param)
{
	struct norm_buff *lbuff[2];
	g2_connection_t *lcon = NULL;

	unsigned refill_count = EVENT_SPACE / 2;
	bool lcon_refresh_needed = false;
	/* make our hzp ready */
	hzp_alloc();
	g2_packet_local_alloc_init();
	recv_buff_local_refill();

	/* fill receive buffer */
	lbuff[0] = recv_buff_local_get();
	lbuff[1] = recv_buff_local_get();
	if(!(lbuff[0] && lbuff[1])) {
		logg_errno(LOGF_ERR, "allocating local buffer");
		worker.keep_going = false;
		goto out;
	}
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
		union gup *guppie;
		struct epoll_event ev;
		bool repoll, kg;

		if(lcon_refresh_needed) {
			g2_con_clear(lcon);
			lcon_refresh_needed = false;
		}
		if(!lbuff[0])
			lbuff[0] = recv_buff_local_get();
		if(!lbuff[1])
			lbuff[1] = recv_buff_local_get();
		if(!(--refill_count)) {
			recv_buff_local_refill();
			g2_packet_local_refill();
			refill_count = EVENT_SPACE / 2;
		}
		/*
		 * let only one thread at a time poll
		 *
		 * We could keep them out with a simple lock, but to avoid
		 * a "thundering herd" Problem, when the reciever leaves the
		 * section, we put them on a conditional (in the hope,
		 * cond_signal to wake one is implemented sensible...)
		 */
		pthread_mutex_lock(&worker.lock);
		while(worker.wait)
			pthread_cond_wait(&worker.cond, &worker.lock);
		worker.wait = worker.keep_going ? true : false;
		pthread_mutex_unlock(&worker.lock);
		if(!worker.keep_going) {
			pthread_cond_broadcast(&worker.cond);
			break;
		}

//TODO: rethink event distribution
		/*
		 * ATM we let on thread poll and then hand out the event one
		 * by one to the worker threads.
		 * This gives low response times and "even" distribution.
		 * But is prop. only scallable with low event rates.
		 * At high event rates it is better to hand out batches,
		 * or in other words: A thread polls and wanders off
		 * with _all_ results, the next threads polls for the
		 * next batch of results etc..
		 * This is much better if we are under heavy fire.
		 * Ideally we want both, but switching this at runtime...
		 */

		do
		{
			if(worker.index >= worker.num_poll) {
				worker.num_poll = my_epoll_wait(worker.epollfd, worker.eevents, anum(worker.eevents), 10000);
				if(0 < worker.num_poll)
					time(&local_time_now);
				worker.index = 0;
			}
			repoll = false;
			switch(worker.num_poll)
			{
			default:
				ev = worker.eevents[worker.index++];
				barrier();
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
		} while(repoll && worker.keep_going);

		pthread_mutex_lock(&worker.lock);
		worker.wait = false;
		pthread_cond_signal(&worker.cond);
		pthread_mutex_unlock(&worker.lock);
		if(!worker.keep_going)
			break;

		/* use result */
		guppie = ev.data.ptr;
		if(unlikely(!guppie)) {
			logg_devel("no guppie?");
			continue;
		}
		kg = true;
		switch(guppie->gup)
		{
		case GUP_G2CONNEC_HANDSHAKE:
			handle_con_a(&ev, lbuff, worker.epollfd);
			break;
		case GUP_G2CONNEC:
			handle_con(&ev, lbuff, worker.epollfd);
			break;
		case GUP_ACCEPT:
			if(ev.events & (uint32_t)EPOLLIN)
			{
				if(handle_accept_in(&guppie->s_gup, lcon, worker.epollfd)) {
					lcon = g2_con_get_free();
					if(unlikely(!lcon))
						kg = false;
//TODO: a master abort is not the best way...
				} else
					lcon_refresh_needed = true;
			}
			if(ev.events & ~((uint32_t)EPOLLIN|EPOLLONESHOT))
				kg = handle_accept_abnorm(&guppie->s_gup, &ev, worker.epollfd);
			break;
		case GUP_UDP:
			{
				struct norm_buff **t_buff, *tt_buff = NULL;
				t_buff = lbuff[0] ? &lbuff[0] : (lbuff[1] ? &lbuff[1] : &tt_buff);
				if(!*t_buff)
				{
					if(!(lbuff[0] = recv_buff_local_get())) {
						logg_errno(LOGF_ERR, "no recv buff!!");
						kg = false;
						break;
					}
					t_buff = &lbuff[0];
				}
				handle_udp(&ev, t_buff, worker.epollfd);
			}
			break;
		case GUP_ABORT:
			kg = handle_abort(&guppie->s_gup, &ev);
			break;
		}
		if(!kg) {
			pthread_mutex_lock(&worker.lock);
			worker.wait = false;
			worker.keep_going = false;
			pthread_cond_broadcast(&worker.cond);
			pthread_mutex_unlock(&worker.lock);
			break;
		}
	}

out:
	g2_con_free(lcon);
	recv_buff_local_ret(lbuff[0]);
	recv_buff_local_ret(lbuff[1]);
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
	static pthread_t *helper;
	static struct simple_gup from_main;
	size_t num_helper, i;

	from_main.gup = GUP_ABORT;
	from_main.fd = *((int *)param);
	logg(LOGF_INFO, "gup:\t\tOur SockFD -> %d\tMain SockFD -> %d\n", from_main.fd, *(((int *)param)-1));

	worker.epollfd = my_epoll_create(PD_START_CAPACITY);
	if(0 > worker.epollfd) {
		logg_errno(LOGF_ERR, "creating epoll-fd");
		goto out;
	}

	worker.eevents[0].events   = (uint32_t)(EPOLLIN|EPOLLONESHOT);
	worker.eevents[0].data.ptr = &from_main;
	if(0 > my_epoll_ctl(worker.epollfd, EPOLL_CTL_ADD, from_main.fd, &worker.eevents[0]))
	{
		logg_errno(LOGF_ERR, "adding main pipe to epoll");
		goto out_epoll;
	}

	num_helper  = server.settings.num_threads;
	num_helper  = num_helper ? num_helper : get_cpus_online();
	num_helper -= 1;
	if(num_helper)
	{
		helper = malloc(num_helper * sizeof(*helper));
		if(!helper) {
			logg_errno(LOGF_CRIT, "No mem for gup helper threads, will run with one");
			num_helper = 0;
		}
	}
	/* Setup locks */
	if(pthread_mutex_init(&worker.lock, NULL))
		goto out_lock;
	if(pthread_cond_init(&worker.cond, NULL))
		goto out_cond;

	if(!init_accept(worker.epollfd))
		goto out_no_accept;
	if(!init_udp(worker.epollfd))
		goto out_no_udp;

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

	clean_up_udp();
out_no_udp:
	clean_up_accept();
out_no_accept:
	clean_up_gup(from_main.fd);
	pthread_cond_destroy(&worker.cond);
out_cond:
	pthread_mutex_destroy(&worker.lock);
out_lock:
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
	if(local_time_now > (con->last_active + (3 * HANDLER_ACTIVE_TIMEOUT)))
	{
		shortlock_t_lock(&con->pts_lock);
		con->flags.dismissed = true;
		p_entry.events = con->poll_interrests |= (uint32_t)EPOLLOUT;
		shortlock_t_unlock(&con->pts_lock);
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

void g2_handler_con_mark_write(struct g2_packet *p, struct g2_connection *con)
{
	struct epoll_event p_entry = {0,{0}};

	p_entry.data.ptr = con;

	shortlock_t_lock(&con->pts_lock);

	list_add_tail(&p->list, &con->packets_to_send);
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
