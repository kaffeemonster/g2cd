/*
 * G2Acceptor.c
 * thread to accept connection and handshake G2
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
 * $Id: G2Acceptor.c,v 1.19 2005/08/21 22:59:12 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* System includes */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <zlib.h>
/* System net-includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
/* other */
#include "lib/other.h"
/* Own includes */
#define _G2ACCEPTOR_C
#include "G2Acceptor.h"
#include "G2MainServer.h"
#include "G2Connection.h"
#include "G2ConHelper.h"
#include "G2ConRegistry.h"
#include "G2KHL.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/recv_buff.h"
#include "lib/my_epoll.h"
#include "lib/atomic.h"
#include "lib/itoa.h"

/* if there is no com for 15 seconds, something is wrong */
#define ACCEPT_ACTIVE_TIMEOUT (15 * 10)
/* if the header is not delivered in 50 seconds, go play somewhere else */
#define ACCEPT_HEADER_COMPLETE_TIMEOUT (50 * 10)

/* internal prototypes */
static inline bool init_con_a(int *, union combo_addr *);
static g2_connection_t *handle_socket_io_a(struct epoll_event *, int epoll_fd, struct norm_buff **lrecv_buff, struct norm_buff **lsend_buff);
static noinline bool initiate_g2(g2_connection_t *);

static struct simple_gup accept_sos[2];

bool init_accept(int epoll_fd)
{
	struct epoll_event tmp_ev;
	bool ipv4_ready;
	bool ipv6_ready;

	/* sock-things */
	accept_sos[0].fd  = -1;
	accept_sos[0].gup = GUP_ACCEPT;
	accept_sos[1].fd  = -1;
	accept_sos[1].gup = GUP_ACCEPT;

	/* Setting up the accepting Sockets */
	if(server.settings.bind.use_ip4)
		ipv4_ready = init_con_a(&accept_sos[0].fd, &server.settings.bind.ip4);
	else {
		ipv4_ready = false;
		accept_sos[0].fd = 0;
	}
	if(server.settings.bind.use_ip6)
		ipv6_ready = init_con_a(&accept_sos[1].fd, &server.settings.bind.ip6);
	else {
		ipv6_ready = false;
		accept_sos[1].fd = 0;
	}
	if(server.settings.bind.use_ip4 && !ipv4_ready)
		return false;
	if(server.settings.bind.use_ip6 && !ipv6_ready)
	{
		if(!server.settings.bind.use_ip4)
			return false;
		else
			logg(LOGF_ERR, "Error starting IPv6, but will keep going!\n");
	}

	/* Setting first entry to be polled, our Acceptsocket */
	if(accept_sos[0].fd > -1)
	{
		tmp_ev.events = (uint32_t)(EPOLLIN | EPOLLERR | EPOLLONESHOT);
		tmp_ev.data.u64 = 0;
		tmp_ev.data.ptr = &accept_sos[0];
		if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, accept_sos[0].fd, &tmp_ev))
		{
			logg_errno(LOGF_ERR, "adding acceptor-socket to epoll");
			clean_up_accept();
			return false;
		}
	}
	if(accept_sos[1].fd > -1)
	{
		tmp_ev.events = (uint32_t)(EPOLLIN | EPOLLERR | EPOLLONESHOT);
		tmp_ev.data.u64 = 0;
		tmp_ev.data.ptr = &accept_sos[1];
		if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, accept_sos[1].fd, &tmp_ev))
		{
			logg_errno(LOGF_ERR, "adding acceptor-socket to epoll");
			clean_up_accept();
			return false;
		}
	}
	return true;
}

void clean_up_accept(void)
{
	close(accept_sos[0].fd);
	close(accept_sos[1].fd);
}

#define OUT_ERR(x)	do {e = x; goto out_err;} while(0)
static inline bool init_con_a(int *accept_so, union combo_addr *our_addr)
{
	const char *e;
	int yes = 1; /* for setsocketopt() SO_REUSEADDR, below */

	if(-1 == (*accept_so = socket(our_addr->s_fam, SOCK_STREAM, 0))) {
		logg_errno(LOGF_ERR, "creating socket");
		return false;
	}
	
	/* lose the pesky "address already in use" error message */
	if(setsockopt(*accept_so, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
		OUT_ERR("setsockopt reuse");

#ifdef HAVE_IPV6_V6ONLY
	if(AF_INET6 == our_addr->s_fam && server.settings.bind.use_ip4) {
		if(setsockopt(*accept_so, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)))
			OUT_ERR("setsockopt V6ONLY");
	}
#endif

	if(bind(*accept_so, casa(our_addr),
	        AF_INET == our_addr->s_fam ? sizeof(our_addr->in) : sizeof(our_addr->in6)))
		OUT_ERR("binding accept fd");

	if(listen(*accept_so, BACKLOG))
		OUT_ERR("calling listen()");

	if(fcntl(*accept_so, F_SETFL, O_NONBLOCK))
		OUT_ERR("making accept fd non-blocking");

	return true;
out_err:
	logg_errno(LOGF_ERR, e);
	close(*accept_so);
	*accept_so = -1;
	return false;
}
#undef OUT_ERR

static noinline void handle_accept_give_msg(g2_connection_t *work_entry, enum loglevel l, const char *msg)
{
	if(l <= get_act_loglevel())
	{
		union combo_addr our_local_addr;
		socklen_t sin_size = sizeof(our_local_addr);

		if(!getsockname(work_entry->com_socket, casa(&our_local_addr), &sin_size))
			logg(l, "Connection\tFrom: %p#I\tTo: %p#I\tFDNum: %i -> %s\n",
			     &work_entry->remote_host, &our_local_addr, work_entry->com_socket, msg);
		else
			logg(l, "Connection\tFrom: %p#I\tTo: us\t\tFDNum: %i -> %s\n",
			     &work_entry->remote_host, work_entry->com_socket, msg);
	}
}

bool handle_accept_in(struct simple_gup *sg, void *wke_ptr, int epoll_fd)
{
	struct epoll_event tmp_eevent;
	g2_connection_t *work_entry = wke_ptr;
	socklen_t sin_size = sizeof(work_entry->remote_host); /* what to do with this info??? */
	int tmp_fd;
	int fd_flags, yes;

	do {
		tmp_fd = accept(sg->fd, casa(&work_entry->remote_host), &sin_size);
	} while(0 > tmp_fd && EINTR == errno);

	tmp_eevent.data.u64 = 0;
	tmp_eevent.data.ptr = sg;
	tmp_eevent.events = (uint32_t)(EPOLLIN | EPOLLERR | EPOLLONESHOT);
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sg->fd, &tmp_eevent))
		logg_errno(LOGF_NOTICE, "resetting accept-fd in EPoll to default-interrests");
		/* if we couldn't rearm our accept fd, we are screwed... */

	if(-1 == tmp_fd) {
		logg_errno(LOGF_NOTICE, "accepting");
		return false;
	}
	work_entry->com_socket = tmp_fd;

	/* increase and check if our total server connection limit is reached */
	if(atomic_inc_return(&server.status.act_connection_sum) > server.settings.max_connection_sum + 5) {
		/* have already counted it, so remove it from count */
		atomic_dec(&server.status.act_connection_sum);
		while(-1 == close(work_entry->com_socket) && EINTR == errno);
		handle_accept_give_msg(work_entry, LOGF_DEVEL_OLD, "too many connections");
		return false;
	}

	/* ip already connected? */
	if(unlikely(g2_conreg_have_ip(&work_entry->remote_host))) {
		/* have already counted it, so remove it from count */
		atomic_dec(&server.status.act_connection_sum);
		while(-1 == close(work_entry->com_socket) && EINTR == errno);
		handle_accept_give_msg(work_entry, LOGF_DEVEL_OLD, "ip already connected");
		return false;
	}
// TODO: add after search is not atomic
	if(!g2_conreg_add(work_entry))
		goto err_out_after_count;

// TODO: Find external addresses

	/* get the fd-flags and add nonblocking  */
	/* according to POSIX manpage EINTR is only encountered when the cmd was F_SETLKW */
	if(-1 == (fd_flags = fcntl(work_entry->com_socket, F_GETFL))) {
		logg_errno(LOGF_NOTICE, "getting socket fd-flags");
		goto err_out_after_add;
	}
	if(fcntl(work_entry->com_socket, F_SETFL, fd_flags | O_NONBLOCK)) {
		logg_errno(LOGF_NOTICE, "setting socket non-blocking");
		goto err_out_after_add;
	}

	yes = 1;
#ifdef HAVE_TCP_CORK
	setsockopt(work_entry->com_socket, SOL_TCP, TCP_CORK, &yes, sizeof(yes));
#endif

	/* No EINTR in epoll_ctl according to manpage :-/ */
	tmp_eevent.events = work_entry->poll_interrests = (uint32_t)(EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLONESHOT);
	tmp_eevent.data.u64 = 0;
	tmp_eevent.data.ptr = work_entry;
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, work_entry->com_socket, &tmp_eevent)) {
		logg_errno(LOGF_NOTICE, "adding new socket to EPoll");
		goto err_out_after_add;
	}

	work_entry->last_active = local_time_now;
	work_entry->active_to.fun = accept_timeout;
	work_entry->active_to.data = work_entry;
	timeout_add(&work_entry->active_to, ACCEPT_ACTIVE_TIMEOUT);

	work_entry->u.accept.header_complete_to.fun = accept_timeout;
	work_entry->u.accept.header_complete_to.data = work_entry;
	timeout_add(&work_entry->u.accept.header_complete_to, ACCEPT_HEADER_COMPLETE_TIMEOUT);

	handle_accept_give_msg(work_entry, LOGF_DEVEL_OLD, "going to handshake");

	return true;

err_out_after_add:
	g2_conreg_remove(work_entry);
err_out_after_count:
	/* the connection failed, but we have already counted it, so remove it from count */
	atomic_dec(&server.status.act_connection_sum);
	while(-1 == close(work_entry->com_socket) && EINTR == errno);
	handle_accept_give_msg(work_entry, LOGF_DEBUG, "problems adding it!");
	return false;
}

bool handle_accept_abnorm(struct simple_gup *sg, struct epoll_event *accept_ptr, int epoll_fd)
{
	bool ret_val = true;
	const char *msg = NULL;
	const char *extra_msg = NULL;
	enum loglevel needed_level = LOGF_INFO;
	
	if(accept_ptr->events & (uint32_t)EPOLLOUT)
	{
		accept_ptr->events = (uint32_t)(EPOLLIN | EPOLLERR | EPOLLONESHOT);
		if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sg->fd, accept_ptr))
			logg_errno(LOGF_NOTICE, "resetting accept-fd in EPoll to default-interrests");

		msg = "Out-Event for Accept-FD was set";
		extra_msg = "";
	}

	if(accept_ptr->events & (uint32_t)(EPOLLERR|EPOLLHUP /* |EPOLLNVAL */ ))
	{
		ret_val = false;
		extra_msg = "\n\tSTOPPING";
		if(accept_ptr->events & (uint32_t)EPOLLERR)
			msg = "error set for Accept-FD!";
		if(accept_ptr->events & (uint32_t)EPOLLHUP)
			msg = "HUP set for Accept-FD?";
/*
 * FUCK-SHIT: if sg->fd becomes invalid, we will not recognize it with EPoll,
 * waiting for ever for new connections
 * 	if(accept_ptr->events & (uint32_t)EPOLLNVAL)
 * 		msg = "NVal set for Accept-FD!";
 */
		needed_level = LOGF_ERR;
	}
	
	logg_posd(needed_level, "%s%s\n", msg, extra_msg);
	return ret_val;
}

void handle_con_a(struct epoll_event *ev, struct norm_buff *lbuff[2], int epoll_fd)
{
	g2_connection_t *tmp_con = ev->data.ptr;
	int lock_res;

	if((lock_res = pthread_mutex_trylock(&tmp_con->lock))) {
		/* somethings wrong */
		if(EBUSY == lock_res)
			return; /* if already locked, do nothing */
// TODO: what to do on locking error?
		return;
	}

	if(ev->events & ~((uint32_t)(EPOLLIN|EPOLLOUT|EPOLLONESHOT))) {
		tmp_con = handle_socket_abnorm(ev);
		recycle_con(tmp_con, epoll_fd, false);
		return;
	}
	/* Some FD's ready to be filled? Some data ready to be read in? */
	tmp_con = handle_socket_io_a(ev, epoll_fd, &lbuff[0], &lbuff[1]);
	if(!tmp_con)
		return;

	tmp_con->gup = GUP_G2CONNEC;
	shortlock_t_lock(&tmp_con->pts_lock);
	pthread_mutex_unlock(&tmp_con->lock);
	ev->events = tmp_con->poll_interrests;
	shortlock_t_unlock(&tmp_con->pts_lock);
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, tmp_con->com_socket, ev)) {
		logg_errno(LOGF_DEBUG, "changing EPoll interrests");
		/*
		 * recycle this bugger.
		 * This is prop. racy, but should not happen...
		 */
		recycle_con(tmp_con, epoll_fd, false);
	}
}

static g2_connection_t *handle_socket_io_a(struct epoll_event *p_entry, int epoll_fd, struct norm_buff **lrecv_buff, struct norm_buff **lsend_buff)
{
	g2_connection_t *w_entry = (g2_connection_t *)p_entry->data.ptr;
	g2_connection_t *ret_val = NULL;

	if(!manage_buffer_before(&w_entry->recv, lrecv_buff))
		goto killit;
	if(!manage_buffer_before(&w_entry->send, lsend_buff))
		goto killit;

	if(p_entry->events & (uint32_t)EPOLLOUT) {
		if(!do_write(p_entry, epoll_fd))
			goto killit_silent;
	}

	if(p_entry->events & (uint32_t)EPOLLIN)
	{
		w_entry->last_active = local_time_now;
		timeout_advance(&w_entry->active_to, ACCEPT_ACTIVE_TIMEOUT);
		if(!do_read(p_entry))
			goto killit_silent;

		if(initiate_g2(w_entry)) {
			shortlock_t_lock(&w_entry->pts_lock);
			w_entry->poll_interrests |= (uint32_t) EPOLLOUT;
			shortlock_t_unlock(&w_entry->pts_lock);
		} else if(w_entry->flags.dismissed)
			goto killit;
		else if (G2CONNECTED == w_entry->connect_state)
			ret_val = w_entry;
	}

	manage_buffer_after(&w_entry->recv, lrecv_buff);
	manage_buffer_after(&w_entry->send, lsend_buff);
	if(!ret_val)
	{
		shortlock_t_lock(&w_entry->pts_lock);
		pthread_mutex_unlock(&w_entry->lock);
		/*
		 * First release lock, than change epoll foo
		 * This is inherent racy, but prevents that another thread
		 * gets the next event and runs against our lock (oneshot, trylock)
		 * before we could get out of the locked region
		 */
		p_entry->events = w_entry->poll_interrests;
		shortlock_t_unlock(&w_entry->pts_lock);
		if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, w_entry->com_socket, p_entry)) {
			logg_errno(LOGF_NOTICE, "changing sockets Epoll-interrests");
			w_entry->flags.dismissed = true;
			goto killit;
		}
	}
	return ret_val;

killit:
	if(!w_entry->flags.dismissed &&
	   G2CONNECTED == w_entry->connect_state) {
		manage_buffer_after(&w_entry->recv, lrecv_buff);
		manage_buffer_after(&w_entry->send, lsend_buff);
		return w_entry;
	}

	logg_posd(LOGF_DEVEL_OLD, "%s Ip: %p#I\tFDNum: %i\n",
	          w_entry->flags.dismissed ? "Dismissed!" : "Problem!",
	          &w_entry->remote_host, w_entry->com_socket);

killit_silent:
	if(*lrecv_buff && w_entry->recv == *lrecv_buff) {
		w_entry->recv = NULL;
		buffer_clear(**lrecv_buff);
	}
	if(*lsend_buff && w_entry->send == *lsend_buff) {
		w_entry->send = NULL;
		buffer_clear(**lsend_buff);
	}
	recycle_con(w_entry, epoll_fd, false);
	return NULL;
}


/*
 * Helper for initiate_g2
 */
static inline bool abort_g2_500(g2_connection_t *to_con)
{
	if(str_size(GNUTELLA_STRING " " STATUS_500 "\r\n\r\n") < buffer_remaining(*to_con->send)) {
		strlitcpy(buffer_start(*to_con->send), GNUTELLA_STRING " " STATUS_500 "\r\n\r\n");
		to_con->send->pos += str_size(GNUTELLA_STRING " " STATUS_500 "\r\n\r\n");
	}
	to_con->flags.dismissed = true;
	return true;
}

static inline bool abort_g2_501_bare(g2_connection_t *to_con)
{
	if(str_size(STATUS_501 "\r\n\r\n") < buffer_remaining(*to_con->send)) {
		strlitcpy(buffer_start(*to_con->send), STATUS_501 "\r\n\r\n");
		to_con->send->pos += str_size(STATUS_501 "\r\n\r\n");
	}
	to_con->flags.dismissed = true;
	return true;
}

static inline bool abort_g2_501(g2_connection_t *to_con)
{
	if(str_size(GNUTELLA_STRING " " STATUS_501 "\r\n\r\n") < buffer_remaining(*to_con->send)) {
		strlitcpy(buffer_start(*to_con->send), GNUTELLA_STRING " " STATUS_501 "\r\n\r\n");
		to_con->send->pos += str_size(GNUTELLA_STRING " " STATUS_501 "\r\n\r\n");
	}
	to_con->flags.dismissed = true;
	return true;
}

static inline bool abort_g2_400(g2_connection_t *to_con)
{
	if(str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n") < buffer_remaining(*to_con->send)) {
		strlitcpy(buffer_start(*to_con->send), GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
		to_con->send->pos += str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
	}
	to_con->flags.dismissed = true;
	return true;
}

static noinline void header_handle_line(g2_connection_t *to_con, size_t len)
{
	char *line;
	char *ret_val, *f_start, *f_end, *c_start;
	size_t i, f_num = 0, old_pos;
	ssize_t f_dist, c_dist;
	bool f_found;

	line = buffer_start(*to_con->recv);
	ret_val = my_memchr(line, ':', len);
	if(unlikely(!ret_val)) /* no ':' on line? */
		goto out_fixup;
	if((ptrdiff_t)((line + len) - ret_val) < 2) /* any room for field data? */
		goto out_fixup;

	/* filter leading garbage */
	for(f_start = line; f_start < ret_val && !isalnum((int)*f_start);)
		f_start++;
	if(unlikely(1 >= ret_val - f_start)) /* nothing left? */
		goto out_fixup;

	/* filter trailing garbage */
	for(f_end = ret_val - 1; f_end >= f_start && !isgraph((int)*f_end);)
		f_end--;
	f_dist = (f_end + 1) - f_start;
	if(unlikely(1 > f_dist)) /* something left? */
		goto out_fixup;

	for(i = 0, f_found = false; i < KNOWN_HEADER_FIELDS_SUM; i++)
	{
		if((size_t)f_dist != KNOWN_HEADER_FIELDS[i]->length)
			continue;
		if(!strncasecmp_a(f_start, KNOWN_HEADER_FIELDS[i]->txt,
		                  KNOWN_HEADER_FIELDS[i]->length)) {
			f_num = i;
			f_found = true;
			break;
		}
	}

	if(unlikely(!f_found)) {
		logg_develd("unknown field:\t\"%.*s\"\tcontent:\n",
		            (int) (ret_val - line), line);
	} else {
		if(unlikely(NULL == KNOWN_HEADER_FIELDS[f_num]->action)) {
			logg_develd("no action field:\t\"%.*s\"\tcontent:\n",
			            (int) f_dist, f_start);
		} else {
			logg_develd_old("  known field:\t\"%.*s\"\tcontent:\n",
			            (int) f_dist, f_start);
		}
	}

	/* after this, move buffer forward */
	old_pos = to_con->recv->pos;
	to_con->recv->pos += (ret_val + 1) - line;
	/* chars left for field content? */
	c_start = ret_val + 1;
	c_dist = (line + len) - c_start;

	/* remove leading white-spaces in field-data */
	for(; c_dist > 0 && isspace((int)*c_start); c_dist--)
		c_start++, to_con->recv->pos++;

	/* now call the associated action for this field */
	if(likely(c_dist > 0))
	{
		if(f_found && NULL != KNOWN_HEADER_FIELDS[f_num]->action) {
			logg_develd_old("\"%.*s\"\n", (int) c_dist, c_start);
			KNOWN_HEADER_FIELDS[f_num]->action(to_con, c_dist);
		} else {
			logg_develd("\"%.*s\"\n", (int) c_dist, c_start);
		}
	} else {
		logg_devel("Field with no data recieved!\n");
	}
	to_con->recv->pos = old_pos;

out_fixup:
	/* skip this bullshit */
	to_con->recv->pos += len;
	return;
}

/*
 * This function handshakes a G2Connection from data in the recv-buffer
 * supplied by a g2_connect_t.
 *
 * return-value: true - there was aditional data put in the send-buffer
 *               false - no data was put in the send-buffer
 *
 * for information about the success (or failure) of this function, watch
 * the g2_connection_t-fields:
 *
 * to_con->flags.dismissed (true indicates a failure)
 * to_con->connect_state (G2CONNECTED indicates finished)
 *
 * gnarf, all this needs a rework/cleanup (sub-functioning), but at the
 * moment, there is a good chance for major changes, thats why all this
 * is in such a shape, but at least, it works.
 */
static noinline bool initiate_g2(g2_connection_t *to_con)
{
	size_t dist;
	char *cp_ret = NULL, *start, *next;
	bool ret_val = false;
	bool more_bytes_needed = false;
	bool header_end = false;

	buffer_flip(*to_con->recv);
	logg_develd_old("Act. content:\n\"%.*s\"", buffer_remaining(*to_con->recv), buffer_start(*to_con->recv));

	/* compute all data available */
	while(!more_bytes_needed)
	{
		switch(to_con->connect_state)
		{
	/* at this point we need at least the first line */
		case UNCONNECTED:
			if(unlikely((str_size(GNUTELLA_CONNECT_STRING) + 2) > buffer_remaining(*to_con->recv))) {
				more_bytes_needed = true;
				break;
			}
			/* let's see if we got the right string */
			if(likely(!strlitcmp(buffer_start(*to_con->recv), GNUTELLA_CONNECT_STRING)))
				to_con->recv->pos += str_size(GNUTELLA_CONNECT_STRING);
			else
				return abort_g2_501_bare(to_con);

			/* CR? */
			if(likely('\r' == *buffer_start(*to_con->recv)))
				to_con->recv->pos++;
			else
				return abort_g2_400(to_con);

			/* LF? */
			if(likely('\n' == *buffer_start(*to_con->recv))) {
				to_con->recv->pos++;
				to_con->connect_state++;
				to_con->u.accept.header_bytes_recv = str_size(GNUTELLA_CONNECT_STRING) + 2;
			} else
				return abort_g2_400(to_con);
	/* gravel over the header field as they arrive */
		case HAS_CONNECT_STRING:
			if(unlikely(!buffer_remaining(*to_con->recv))) {
				more_bytes_needed = true;
				break;
			}

			do
			{
				start = buffer_start(*to_con->recv);
				next  = mem_searchrn(start, buffer_remaining(*to_con->recv));
				if(!next)
					break;
				dist = next - start;
				logg_develd_old("line: %zu, %zu \"%.*s\"\n", dist, buffer_remaining(*to_con->recv), dist, start);
				if(likely(dist))
					header_handle_line(to_con, dist);
				to_con->recv->pos += 2;
				to_con->u.accept.header_bytes_recv += dist + 2;
				if(0 == dist) {
					header_end = true;
					break;
				}
			} while(buffer_remaining(*to_con->recv));

			if(likely(header_end)) {
				buffer_skip(*to_con->recv);
				logg_devel_old("\t------------------ Initiator\n");
				to_con->connect_state++;
			}
			else
			{
				/* swamping my in? */
				if(MAX_HEADER_LENGTH > to_con->u.accept.header_bytes_recv)
				{
					/* buffer full and no end? */
					if(to_con->recv->capacity - buffer_remaining(*to_con->recv)) {
						/* we need more bytes */
						more_bytes_needed = true;
						break;
					}
					else
						return abort_g2_400(to_con); /* go home */
				}
				else
					return abort_g2_400(to_con); /* go home */
			}
	/* the first header is comlete, give the poor soul new time */
		case ADVANCE_TIMEOUTS:
			timeout_advance(&to_con->u.accept.header_complete_to, ACCEPT_HEADER_COMPLETE_TIMEOUT);
			to_con->connect_state++;
	/* what's up with the accept-field? */
		case CHECK_ACCEPT:
			/* do we have one? */
			if(likely(to_con->u.accept.flags.accept_ok))
			{
				/* and is it G2? */
				if(likely(to_con->u.accept.flags.accept_g2))
					to_con->connect_state++;
				else
					return abort_g2_501(to_con);
			}
			else
				return abort_g2_400(to_con);
	/* and the remote_ip field? */
		case CHECK_ADDR:
			/* do we have one and there was a valid address inside? */
			if(likely(to_con->u.accept.flags.addr_ok))
				to_con->connect_state++;
			else
				return abort_g2_400(to_con);
	/* how about the user-agent */
		case CHECK_UAGENT:
			/* even if it's empty, the field should be send */
			if(likely(to_con->u.accept.flags.uagent_ok))
				to_con->connect_state++;
			else
				return abort_g2_400(to_con);
	/* last 'must-have' field: X-UltraPeer */
		case CHECK_UPEER:
			if(likely(to_con->u.accept.flags.upeer_ok))
				to_con->connect_state++;
			else
				return abort_g2_400(to_con);
	/* what's the accepted encoding? */
		case CHECK_ENC_OUT:
			/*
			 * if nothing was send, default to no encoding (Shareaza tries to
			 * save space in the header, so no failure if absent)
			 */
			if(!to_con->u.accept.flags.enc_out_ok)
				to_con->encoding_out = ENC_NONE;
			else
			{
				/* reset set encoding to none, if we could not agree about it */
				if(to_con->encoding_out !=
				   (to_con->flags.upeer ? server.settings.hub_out_encoding : server.settings.default_out_encoding))
					to_con->encoding_out = ENC_NONE;
				if((ENC_NONE != to_con->encoding_out) && (!to_con->send_u)) {
					to_con->send_u = recv_buff_alloc();
					if(!to_con->send_u)
						to_con->encoding_out = ENC_NONE;
				}

				if(ENC_DEFLATE == to_con->encoding_out)
				{
					to_con->z_encoder = malloc(sizeof(*to_con->z_encoder));
					if(to_con->z_encoder)
					{
						memset(to_con->z_encoder, 0, sizeof(*to_con->z_encoder));
						if(Z_OK != deflateInit(to_con->z_encoder, Z_DEFAULT_COMPRESSION))
						{
							if(to_con->z_encoder->msg)
								logg_posd(LOGF_DEBUG, "%s\n", to_con->z_encoder->msg);
							free(to_con->z_encoder);
							to_con->z_encoder = NULL;
							to_con->encoding_out = ENC_NONE;
						}
					}
					else
						to_con->encoding_out = ENC_NONE;
				}
			}
			/* set our desired ingress encoding */
			to_con->encoding_in =
				to_con->flags.upeer ? server.settings.hub_in_encoding : server.settings.default_in_encoding;
			to_con->connect_state++;
		case BUILD_ANSWER:
			/*
			 * it should be our first comunication, and if someone
			 * set capacity right everything will be fine
			 */
			buffer_clear(*to_con->send);
#if 0
		/* we could calculate the needed size but the header should be small enough */
			size_t needed_s = GNUTELLA_STRING + STATUS_200 + 3;
			needed_s += UAGENT_KEY + OUR_UA + 4;
			needed_s += LISTEN_ADR_KEY + INET6_ADDRSTRLEN + 5 + 4;
			needed_s += REMOTE_ADR_KEY + INET6_ADDRSTRLEN + 4;
			needed_s += CONTENT_KEY + ACCEPT_G2 + 4;
			needed_s += ACCEPT_KEY + ACCEPT_G2 + 4;
			if(ENC_NONE != to_con->encoding_in)
				needed_s += CONTENT_ENC_KEY + KNOWN_ENCODINGS[to_con->encoding_in]->length + 4;
			if(ENC_NONE != to_con->encoding_out)
				needed_s += ACCEPT_ENC_KEY  + KNOWN_ENCODINGS[to_con->encoding_out]->length + 4;
			needed_s += UPEER_KEY + ((our_server_upeer) ? G2_TRUE : G2_FALSE) + 4;
			needed_s += UPEER_NEEDED_KEY + ((our_server_upeer_needed) ? G2_TRUE : G2_FALSE) + 4;
			needed_s += 2;
#endif
	/* 2nd. header part till first ip (listen-ip) */
#define HED_2_PART_1				\
	GNUTELLA_STRING " " STATUS_200 "\r\n"	\
	UAGENT_KEY ": " OUR_UA "\r\n"		\
	LISTEN_ADR_KEY ": "

#define HED_2_PART_1_FULLH				\
	GNUTELLA_STRING " 400 to many hubs\r\n"	\
	UAGENT_KEY ": " OUR_UA "\r\n"		\
	LISTEN_ADR_KEY ": "

#define HED_2_PART_1_FULLC				\
	GNUTELLA_STRING " 400 to many connections\r\n"	\
	UAGENT_KEY ": " OUR_UA "\r\n"		\
	LISTEN_ADR_KEY ": "

#define HED_2_PART_3				\
	"\r\n" CONTENT_KEY ": " ACCEPT_G2 "\r\n"	\
	ACCEPT_KEY ": " ACCEPT_G2 "\r\n"

// TODO: maybe we want to refuse the connection

			/* copy start of header */
// TODO: finer granularity of stepping for smaller sendbuffer?
			/* could we place it all in our sendbuffer? */
			if(likely(str_size(HED_2_PART_1_FULLC) < buffer_remaining(*to_con->send)))
			{
				if(to_con->flags.upeer)
				{
					if(atomic_read(&server.status.act_hub_sum) >= server.settings.max_hub_sum) {
						strlitcpy(buffer_start(*to_con->send), HED_2_PART_1_FULLH);
						to_con->send->pos += str_size(HED_2_PART_1_FULLH);
						to_con->flags.dismissed = true;
					} else if(atomic_read(&server.status.act_connection_sum) >= server.settings.max_connection_sum) {
						strlitcpy(buffer_start(*to_con->send), HED_2_PART_1_FULLC);
						to_con->send->pos += str_size(HED_2_PART_1_FULLC);
						to_con->flags.dismissed = true;
					} else {
						strlitcpy(buffer_start(*to_con->send), HED_2_PART_1);
						to_con->send->pos += str_size(HED_2_PART_1);
					}
				}
				else if(atomic_read(&server.status.act_connection_sum) >=
					     (server.settings.max_connection_sum - server.settings.max_hub_sum)) {
					strlitcpy(buffer_start(*to_con->send), HED_2_PART_1_FULLC);
					to_con->send->pos += str_size(HED_2_PART_1_FULLC);
					to_con->flags.dismissed = true;
				} else {
					strlitcpy(buffer_start(*to_con->send), HED_2_PART_1);
					to_con->send->pos += str_size(HED_2_PART_1);
				}
			} else {
				/* if not there must be something very wrong -> go home */
				to_con->flags.dismissed = true;
				return false;
			}

// TODO: add prefernce what's our global ip?
			/*
			 * It's totaly fine for a REAL host to be multihomed.
			 * But the answer to the question "What's my IP?" then
			 * depends on the one who answers ;)
			 * This hole Listen-IP/Remote-IP was not build with
			 * multihoming in mind, quite the contrary, to detect NAT.
			 *
			 * So we can only lie to the client who has connected. The
			 * IP he reached was obviously the right one (we can exchange
			 * packets). There is only one problem: If we are NATed...
			 *
			 * SO WHO EVER THINKS RUNNING THIS SERVER CODE BEHIND A NAT IS A
			 * GOOD IDEA SHOULD DIE BY STEEL^w^w^w LIVE WITH THE CONSEQUENCES
			 */
			{
				union combo_addr local_addr;
				socklen_t sin_size = sizeof(local_addr);

				/*
				 * get our ip the remote host connected to from
				 * our socket handle
				 */
				if(unlikely(getsockname(to_con->com_socket, casa(&local_addr), &sin_size))) {
					logg_errno(LOGF_DEBUG, "getting local addr of socket");
					local_addr = AF_INET == to_con->remote_host.s_fam ?
					             server.settings.bind.ip4 : server.settings.bind.ip6;
				}
				cp_ret = combo_addr_print_c(&local_addr, buffer_start(*to_con->send),
				                            buffer_remaining(*to_con->send));
				if(unlikely(!cp_ret)) {
					/* we have a problem, that should not happen... */
					logg_errno(LOGF_DEBUG, "writing Listen-Ip-field");
					to_con->flags.dismissed = true;
					return false;
				}
				to_con->send->pos += cp_ret - buffer_start(*to_con->send);

				if(unlikely(6 + str_size("\r\n" REMOTE_ADR_KEY ": ") >=
				            buffer_remaining(*to_con->send))) {
					/* no space for port and foo? */
					to_con->flags.dismissed = true;
					return false;
				}

				cp_ret = buffer_start(*to_con->send);
				*cp_ret++ = ':';
				cp_ret = ustoa(cp_ret, ntohs(combo_addr_port(&local_addr)));
				cp_ret = strplitcpy(cp_ret,"\r\n" REMOTE_ADR_KEY ": ");
				to_con->send->pos += cp_ret - buffer_start(*to_con->send);
			}

			/* tell remote end from which ip we saw it comming */
			cp_ret = combo_addr_print_c(&to_con->remote_host, buffer_start(*to_con->send),
			                            buffer_remaining(*to_con->send));
			if(unlikely(!cp_ret)) {
				logg_errno(LOGF_DEBUG, "writing Remote-Ip-field");
				to_con->flags.dismissed = true;
				return false;
			}
			to_con->send->pos += cp_ret - buffer_start(*to_con->send);

			if(str_size(HED_2_PART_3) < buffer_remaining(*to_con->send)) {
				strlitcpy(buffer_start(*to_con->send), HED_2_PART_3);
				to_con->send->pos += str_size(HED_2_PART_3);
			} else {
				to_con->flags.dismissed = true;
				return false;
			}
	
			/* if we have encoding, put in the keys */
			if(ENC_NONE != to_con->encoding_out)
			{
				if(likely((str_size(CONTENT_ENC_KEY) + str_size(ENC_MAX_S) + 4) <
				          buffer_remaining(*to_con->send)))
				{
					cp_ret = strplitcpy(buffer_start(*to_con->send), CONTENT_ENC_KEY ": ");
					cp_ret = strpcpy(cp_ret, KNOWN_ENCODINGS[to_con->encoding_out]->txt);
					*cp_ret++ = '\r'; *cp_ret++ = '\n';
					to_con->send->pos += cp_ret - buffer_start(*to_con->send);
				} else {
					to_con->flags.dismissed = true;
					return false;
				}
			}
			if(ENC_NONE != to_con->encoding_in)
			{
				if(likely((str_size(ACCEPT_ENC_KEY) + str_size(ENC_MAX_S) + 4) <
				          buffer_remaining(*to_con->send)))
				{
					cp_ret = strplitcpy(buffer_start(*to_con->send), ACCEPT_ENC_KEY ": ");
					cp_ret = strpcpy(cp_ret, KNOWN_ENCODINGS[to_con->encoding_in]->txt);
					*cp_ret++ = '\r'; *cp_ret++ = '\n';
					to_con->send->pos += cp_ret - buffer_start(*to_con->send);
				} else {
					to_con->flags.dismissed = true;
					return false;
				}
			}

			/* and the rest of the header */
			if(likely((str_size(UPEER_KEY) + str_size(UPEER_NEEDED_KEY) +
			           str_size(HUB_KEY) + str_size(HUB_NEEDED_KEY) + 4 * 9 + 2) <=
			          buffer_remaining(*to_con->send)))
			{
				cp_ret = buffer_start(*to_con->send);
				if(server.status.our_server_upeer) {
					cp_ret = strplitcpy(cp_ret, UPEER_KEY ": " G2_TRUE);
					cp_ret = strplitcpy(cp_ret, "\r\n" HUB_KEY ": " G2_TRUE);
				} else {
					cp_ret = strplitcpy(buffer_start(*to_con->send), UPEER_KEY ": " G2_FALSE);
					cp_ret = strplitcpy(cp_ret, "\r\n" HUB_KEY ": " G2_FALSE);
				}
				if(atomic_read(&server.status.act_hub_sum) < server.settings.max_hub_sum) {
					cp_ret = strplitcpy(cp_ret, "\r\n" UPEER_NEEDED_KEY ": " G2_TRUE);
					cp_ret = strplitcpy(cp_ret, "\r\n" HUB_NEEDED_KEY ": " G2_TRUE);
				} else {
					cp_ret = strplitcpy(cp_ret, "\r\n" UPEER_NEEDED_KEY ": " G2_FALSE);
					cp_ret = strplitcpy(cp_ret, "\r\n" HUB_NEEDED_KEY ": " G2_FALSE);
				}
				*cp_ret++ = '\r'; *cp_ret++ = '\n';
				to_con->send->pos += cp_ret - buffer_start(*to_con->send);
			} else {
				to_con->flags.dismissed = true;
				return false;
			}

			if(to_con->flags.dismissed /* only give try hubs if we have to send him away */ &&
			   likely((str_size(X_TRY_HUB_KEY) + 4 + 2 +
			          (INET6_ADDRSTRLEN + 2 + 4 + 5 + str_size("2007-01-10T23:59Z")) * 2) <=
			         buffer_remaining(*to_con->send)))
			{
				struct khl_entry khl_e[8];
				size_t khl_num;
				size_t rem = buffer_remaining(*to_con->send);

				rem -= str_size(X_TRY_HUB_KEY) + 4 + 2;
				rem /= (INET6_ADDRSTRLEN + 2 + 4 + 5 + str_size("2007-01-10T23:59Z"));
				rem  = anum(khl_e) < rem ? anum(khl_e) : rem;

				khl_num = g2_khl_fill_p(khl_e, rem, to_con->remote_host.s_fam);

				if(khl_num)
				{
					cp_ret = strplitcpy(buffer_start(*to_con->send), X_TRY_HUB_KEY ": ");
					while(khl_num--)
					{
						if(AF_INET6 == khl_e[khl_num].na.s_fam)
							*cp_ret++ = '[';
						cp_ret = combo_addr_print_c(&khl_e[khl_num].na, cp_ret, INET6_ADDRSTRLEN);
						if(AF_INET6 == khl_e[khl_num].na.s_fam)
							*cp_ret++ = ']';
						*cp_ret++ = ':';
						cp_ret = ustoa(cp_ret, ntohs(combo_addr_port(&khl_e[khl_num].na)));
						*cp_ret++ = ' ';
						cp_ret += print_ts(cp_ret, 20, &khl_e[khl_num].when);
						*cp_ret++ = ',';
						*cp_ret++ = ' ';
					}
					cp_ret -= 2;
					*cp_ret++ = '\r'; *cp_ret++ = '\n';
					to_con->send->pos += cp_ret - buffer_start(*to_con->send);
				}
			}
			cp_ret = buffer_start(*to_con->send);
			*cp_ret++ = '\r'; *cp_ret++ = '\n';
			to_con->send->pos += cp_ret - buffer_start(*to_con->send);

			logg_devel_old("\t------------------ Response\n");
#if 0
			buffer_flip(*to_con->send);
			logg_develd("\"%.*s\"\nlength: %i\n",
				buffer_remaining(*to_con->send),
				buffer_start(*to_con->send),
				buffer_remaining(*to_con->send));
			buffer_compact(*to_con->send);
#endif
			to_con->connect_state++;
			ret_val = true;
			more_bytes_needed = true;
			break;
	/* Waiting for an answer */
		case HEADER_2:
			if(unlikely(str_size(GNUTELLA_STRING " 200") > buffer_remaining(*to_con->recv))) {
				more_bytes_needed = true;
				break;
			}

			/* did the other end like it */
			if(unlikely(strlitcmp(buffer_start(*to_con->recv), GNUTELLA_STRING " 200"))) {
				/* if not: no deal */
				to_con->flags.dismissed = true;
				return false;
			}
			to_con->recv->pos += str_size(GNUTELLA_STRING " 200");
			to_con->u.accept.flags.second_header = true;
			to_con->u.accept.header_bytes_recv = str_size(GNUTELLA_STRING " 200");
			to_con->connect_state++;
	/* search first CRLF (maybe someone sends something like 'Welcome' behind the 200) and skip it */
		case ANSWER_200:
			if(unlikely(!buffer_remaining(*to_con->recv))) {
				more_bytes_needed = true;
				break;
			}

			start = buffer_start(*to_con->recv);
			next  = mem_searchrn(start, buffer_remaining(*to_con->recv));
			if(unlikely(!next))
			{
				to_con->u.accept.header_bytes_recv += buffer_remaining(*to_con->recv)-1;
				/* swamping my in? */
				if(MAX_HEADER_LENGTH > to_con->u.accept.header_bytes_recv)
				{
					/* we need more bytes */
					/* skip between 200 and \r\n, but keep one char, it may be the \r */
					to_con->recv->pos = to_con->recv->limit - 1;
					more_bytes_needed = true;
					break;
				}
				else
					return abort_g2_400(to_con); /* go home */
			}
			dist = next - start;
			to_con->recv->pos += dist + 2;
			to_con->u.accept.header_bytes_recv += dist + 2;
			to_con->connect_state++;
	/* again fidel around with the header */
		case SEARCH_CAPS_2:
			if(unlikely(!buffer_remaining(*to_con->recv))) {
				more_bytes_needed = true;
				break;
			}

			do
			{
				start = buffer_start(*to_con->recv);
				next  = mem_searchrn(start, buffer_remaining(*to_con->recv));
				if(!next)
					break;
				dist = next - start;
				logg_develd_old("line: %u \"%.*s\"\n", dist, dist, start);
				if(likely(dist))
					header_handle_line(to_con, dist);
				to_con->recv->pos += 2;
				to_con->u.accept.header_bytes_recv += dist + 2;
				if(0 == dist) {
					header_end = true;
					break;
				}
			} while(buffer_remaining(*to_con->recv));

			if(likely(header_end))
			{
			/*
			 * retain data delivered directly behind the header if there is some.
			 * trust me, this is needed, so no skipping
			 */
				logg_devel_old("\t------------------ Initiator\n");
				to_con->connect_state++;
			}
			else
			{
				/* swamping my in? */
				if(MAX_HEADER_LENGTH > to_con->u.accept.header_bytes_recv)
				{
					/* buffer full and no end? */
					if(to_con->recv->capacity - buffer_remaining(*to_con->recv)) {
						/* we need more bytes */
						more_bytes_needed = true;
						break;
					}
					else
						return abort_g2_400(to_con); /* go home */
				}
				else
					return abort_g2_400(to_con); /* go home */
			}
	/* we have the second header, we are nearly finished, prevent from running out of time */
		case CANCEL_TIMEOUTS:
// TODO: cancel here or at end?
			timeout_cancel(&to_con->active_to);
			timeout_cancel(&to_con->u.accept.header_complete_to);
			to_con->connect_state++;
	/* Content-Key? */
		case CHECK_CONTENT:
			/* do we have one? */
			if(likely(to_con->u.accept.flags.content_ok))
			{
				/* and is it G2? */
				if(likely(to_con->u.accept.flags.content_g2))
					to_con->connect_state++;
				else
					return abort_g2_501(to_con);
			}
			else
				return abort_g2_400(to_con);
	/* what does the other likes to send as encoding */
		case CHECK_ENC_IN:
			/*
			 * if nothing was send, default to no encoding (Shareaza tries to
			 * save space in the header, so no failure if absent)
			 */
			if(!to_con->u.accept.flags.enc_in_ok)
				to_con->encoding_in = ENC_NONE;
			else
			{
				/* abort, if we could not agree about it */
				if(to_con->encoding_in != ENC_NONE &&
				   (to_con->encoding_in !=
				    (to_con->flags.upeer ? server.settings.hub_in_encoding : server.settings.default_in_encoding)))
					return abort_g2_400(to_con);

				if((ENC_NONE != to_con->encoding_in) && (!to_con->recv_u)) {
					to_con->recv_u = recv_buff_alloc();
					if(!to_con->recv_u)
						return abort_g2_501(to_con);
				}

				if(ENC_DEFLATE == to_con->encoding_in)
				{
					to_con->z_decoder = malloc(sizeof(*to_con->z_decoder));
					if(to_con->z_decoder)
					{
						memset(to_con->z_decoder, 0, sizeof(*to_con->z_decoder));
						if(Z_OK != inflateInit(to_con->z_decoder))
						{
							if(to_con->z_decoder->msg)
								logg_posd(LOGF_DEBUG, "%s\n", to_con->z_decoder->msg);
							free(to_con->z_decoder);
							to_con->z_decoder = NULL;
							return abort_g2_500(to_con);
						}
					}
					else
						return abort_g2_500(to_con);
				}
			}
			to_con->connect_state++;
	/* make everything ready for business */
		case FINISH_CONNECTION:
			DESTROY_TIMEOUT(&to_con->u.accept.header_complete_to);
			/* wipe out the shared space again */
			memset(&to_con->u.accept, 0, sizeof(to_con->u.accept));
			to_con->connect_state = G2CONNECTED;
			INIT_TIMEOUT(&to_con->u.handler.z_flush_to);
			/* OK, handshake was a success, now put the connec in the right place */
			if(to_con->flags.upeer) {
				if(!g2_conreg_promote_hub(to_con))
					to_con->flags.dismissed = true;
				/* connection is now a hub, remove from QHTs */
				g2_conreg_mark_dirty(to_con);
			}
			handle_accept_give_msg(to_con, LOGF_DEVEL_OLD, "accepted");
			more_bytes_needed = true;
			break;
		case G2CONNECTED:
			more_bytes_needed = true;
			break;
		}
	}

	buffer_compact(*to_con->recv);

	logg_develd_old("%p pos: %u lim: %u\n", (void*)buffer_start(*to_con->recv), to_con->recv->pos, to_con->recv->limit);
	return ret_val;
}

static char const rcsid_a[] GCC_ATTR_USED_VAR = "$Id: G2Acceptor.c,v 1.19 2005/08/21 22:59:12 redbully Exp redbully $";

/*			to_con.tmp_packet = new G2Packet();
			to_con.packete = new ArrayList();
			if(ENC_DEFLATE_ID == to_con.compression_in) to_con.de_comp = our_server.get_inflater();
			to_con.just_finished = true;
			try
			{
				to_con.out_file = (new FileOutputStream("test" + counter + ".bin", false)).getChannel();
				counter++;
			}
			catch(FileNotFoundException fnfe)
			{
				System.out.println(this.getClass().getName() + ": Konnte Testdatei nicht oeffnen!\n" + fnfe);
				to_con.send_buffers.add(std_utf8.encode(GNUTELLA_STRING + " " + STATUS_500 + "\r\n\r\n"));
				to_con.dismissed = true;
				return(true);
			}
			break;
*/
/* EOF */
