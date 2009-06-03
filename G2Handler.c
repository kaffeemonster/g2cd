/*
 * G2Handler.c
 * thread to handle G2-Protocol
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
 * $Id: G2Handler.c,v 1.21 2005/07/29 09:24:04 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* System includes */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
//#include <ctype.h>
#include <zlib.h>
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#endif
/* System net-includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
/* other */
#include "lib/other.h"
/* Own includes */
#define _G2HANDLER_C
#define _NEED_G2_P_TYPE
#include "G2Packet.h"
#include "G2Handler.h"
#include "G2MainServer.h"
#include "G2Connection.h"
#include "G2ConHelper.h"
#include "G2ConRegistry.h"
#include "G2PacketSerializer.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/recv_buff.h"
#include "lib/my_epoll.h"

#define HANDLER_ACTIVE_TIMEOUT (91 * 10)

/* internal prototypes */
static inline bool init_memory_h(struct epoll_event **, struct norm_buff **, struct norm_buff **, int *);
static inline bool handle_from_accept(int, int);
static inline g2_connection_t *handle_socket_io_h(struct epoll_event *, int epoll_fd, struct norm_buff **, struct norm_buff **);
static void handler_active_timeout(void *);
/* do not inline, we take a pointer of it, and when its called, performance doesn't matter */
static void clean_up_h(struct epoll_event *, struct norm_buff *, struct norm_buff *, int, int);

void *G2Handler(void *param)
{
	/* data-structures */
	struct norm_buff *lrecv_buff = NULL, *lsend_buff = NULL;

	/* sock-things */
	int from_accept = -1;
	int epoll_fd = -1;
	int sock2main;

	/* other variables */
	struct epoll_event *eevents = NULL;
	struct epoll_event *e_wptr = NULL;
	size_t i;
	int num_poll = 0;
	bool keep_going = true;

	sock2main = *((int *)param);
	logg(LOGF_DEBUG, "Handler:\tOur SockFD -> %d\tMain SockFD -> %d\n", sock2main, *(((int *)param)-1));

	/* getting memory for our FD's and everything else */
	if(!init_memory_h(&eevents, &lrecv_buff, &lsend_buff, &epoll_fd))
	{ 
		if(0 > send(sock2main, "All lost", sizeof("All lost"), 0))
			diedie("initiating stop"); // hate doing this, but now it's to late
		logg_pos(LOGF_ERR, "should go down\n");
		server.status.all_abord[THREAD_HANDLER] = false;
		pthread_exit(NULL);
	}
	logg(LOGF_DEBUG, "Handler:\tEPoll-FD -> %i\n", epoll_fd);

	if(sizeof(from_accept) != recv(sock2main, &from_accept, sizeof(from_accept), 0)) {
		logg_errno(LOGF_ERR, "retrieving IPC Pipe");
		clean_up_h(eevents, lrecv_buff, lsend_buff, epoll_fd, sock2main);
		pthread_exit(NULL);
	}
	logg(LOGF_DEBUG, "Handler:\tfrom_accept -> %i\n", from_accept);

	/* Setting first entry to be polled, our Pipe from Acceptor */
	eevents->events = EPOLLIN;
	/* Attention - very ugly, but i have to distinguish these two sockets, and all the other */
	eevents->data.ptr = (void *)1;
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, from_accept, eevents)) {
		logg_errno(LOGF_ERR, "adding acceptor-pipe to epoll");
		clean_up_h(eevents, lrecv_buff, lsend_buff, epoll_fd, sock2main);
		pthread_exit(NULL);
	}
	eevents->data.ptr = (void *)0;
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock2main, eevents)) {
		logg_errno(LOGF_ERR, "adding main-pipe to epoll");
		clean_up_h(eevents, lrecv_buff, lsend_buff, epoll_fd, sock2main);
		pthread_exit(NULL);
	}

	/* we are up and running */
	server.status.all_abord[THREAD_HANDLER] = true;

	my_snprintf(buffer_start(*lsend_buff), buffer_remaining(*lsend_buff), OUR_PROC " Handler %i", 0);
	g2_set_thread_name(buffer_start(*lsend_buff));

	while(keep_going)
	{
		recv_buff_local_refill();
		/* Let's do it */
		num_poll = my_epoll_wait(epoll_fd, eevents, EVENT_SPACE, 8000);
		e_wptr = eevents;
		switch(num_poll)
		{
		/* Normally: see what has happened */
		default:
			local_time_now = time(NULL);
			for(i = num_poll; i; i--, e_wptr++)
			{
				/* A common Socket? */
				if(e_wptr->data.ptr)
				{
					if(likely(((void *)1) != e_wptr->data.ptr))
					{
						logg_develd_old("-------- Events: 0x%0X, PTR: %p\n", e_wptr->events, e_wptr->data.ptr);
						// Any problems?
						// 'Where did you go my lovely...'
						// Any invalid FD's remained in PollData?
						if(e_wptr->events & ~((uint32_t)(EPOLLIN|EPOLLOUT)))
						{
							g2_connection_t *tmp_con_holder = handle_socket_abnorm(e_wptr);
							if(tmp_con_holder)
								recycle_con(tmp_con_holder, epoll_fd, false);
							else {
								logg_pos(LOGF_ERR, "Somethings wrong with our polled FD's, couldn't solve it\n");
								keep_going = false;
							}
						}
						else
						{
							// Some FD's ready to be filled?
							// Some data ready to be read in?
							g2_connection_t *tmp_con_holder = handle_socket_io_h(e_wptr, epoll_fd, &lrecv_buff, &lsend_buff);
							if(tmp_con_holder)
							{
								if(lrecv_buff && tmp_con_holder->recv == lrecv_buff) {
									tmp_con_holder->recv = NULL;
									buffer_clear(*lrecv_buff);
								}
								if(lsend_buff && tmp_con_holder->send == lsend_buff) {
									tmp_con_holder->send = NULL;
									buffer_clear(*lsend_buff);
								}
								recycle_con(tmp_con_holder, epoll_fd, false);
							}
						}
					}
					/* the from_acceptor-pipe */
					else
					{
						if(e_wptr->events & (uint32_t) EPOLLIN)
							handle_from_accept(from_accept, epoll_fd);
						/* if there is no read-interrest, we're blown up */
						else
						{
							logg_pos(LOGF_ERR, "from_acceptor-pipe is wired\n");
							keep_going = false;
							break;
						}
					}
				}
				/* the abort-socket */
				else
				{
// TODO: Check for a proper stop-sequence ??
					// everything but 'ready for write' means:
					// we're finished...
					if(e_wptr->events & ~((uint32_t)EPOLLOUT)) {
						keep_going = false;
						break;
					}

					/* else stop this write interrest */
					e_wptr->events = (uint32_t)EPOLLIN;
					if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock2main, e_wptr)) {
						logg_errno(LOGF_ERR, "changing epoll-interrests for socket-2-main");
						keep_going = false;
						break;
					}
				}
			}
			break;
		/* Nothing happened (or just the Timeout) */
		case 0:
			break;
		/* Something bad happened */
		case -1:
			if(EINTR == errno)
				break;
			/* Print what happened */
			logg_errno(LOGF_ERR, "poll");
			/* and get out here (at the moment) */
			keep_going = false;
			break;
		}
	}

	clean_up_h(eevents, lrecv_buff, lsend_buff, epoll_fd, sock2main);
	pthread_exit(NULL);
	return NULL; /* to avoid warning about reaching end of non-void funktion */
}

static inline bool init_memory_h(struct epoll_event **poll_me, struct norm_buff **lrecv_buff,
                                 struct norm_buff **lsend_buff, int *epoll_fd)
{
	*poll_me = calloc(EVENT_SPACE, sizeof(**poll_me));
	if(NULL == *poll_me)
	{
		logg_errno(LOGF_ERR, "EPoll eventspace memory");
		return false;
	}

	*lrecv_buff = recv_buff_local_get();
	*lsend_buff = recv_buff_local_get();
	if(!(*lrecv_buff && *lsend_buff))
	{
		logg_errno(LOGF_ERR, "allocating local buffer");
		free(*poll_me);
		return false;
	}

	*epoll_fd = my_epoll_create(PD_START_CAPACITY);
	if(0 > *epoll_fd)
	{
		logg_errno(LOGF_ERR, "creating epoll-fd");
		return false;
	}

	return true;
}

static inline bool handle_from_accept(int from_acceptor, int epoll_fd)
{
	struct epoll_event tmp_eevent = {0,{0}};
	g2_connection_t *recvd_con = NULL;
	ssize_t ret_val;

	do {
		ret_val = read(from_acceptor, &recvd_con, sizeof(recvd_con));
	} while(-1 == ret_val && EINTR == errno);
	if(sizeof(recvd_con) != ret_val) {
		logg_errno(LOGF_INFO, "recieving new Connection");
		return false;
	}

	tmp_eevent.events = recvd_con->poll_interrests = (uint32_t)(EPOLLIN | EPOLLERR | EPOLLHUP);
	tmp_eevent.data.ptr = recvd_con;
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, recvd_con->com_socket, &tmp_eevent)) {
		logg_errno(LOGF_DEBUG, "adding new socket to EPoll");
		goto clean_up;
	}

	recvd_con->last_active = local_time_now;
	recvd_con->active_to.fun = handler_active_timeout;
	recvd_con->active_to.data = recvd_con;
	timeout_add(&recvd_con->active_to, HANDLER_ACTIVE_TIMEOUT);

	return true;

clean_up:
	while(-1 == close(recvd_con->com_socket) && EINTR == errno);
	g2_con_clear(recvd_con);
	g2_con_ret_free(recvd_con);
	return false;
}

static inline g2_connection_t *handle_socket_io_h(struct epoll_event *p_entry, int epoll_fd, struct norm_buff **lrecv_buff, struct norm_buff **lsend_buff)
{
	g2_connection_t *w_entry = (g2_connection_t *)p_entry->data.ptr;

	/* get buffer */
	if(!manage_buffer_before(&w_entry->recv, lrecv_buff)) {
		w_entry->flags.dismissed = true;
		return w_entry;
	}
	if(!manage_buffer_before(&w_entry->send, lsend_buff)) {
		w_entry->flags.dismissed = true;
		return w_entry;
	}

	/* write */
	w_entry->flags.has_written = false;
	if(p_entry->events & (uint32_t)EPOLLOUT)
	{
		struct norm_buff *d_target = NULL;
		struct list_head *e, *n;
		if(ENC_DEFLATE == w_entry->encoding_out)
			d_target = w_entry->send_u;
		else
			d_target = w_entry->send;

		if(buffer_remaining(*d_target) < 10)
			goto no_fill_before_write;

		list_for_each_safe(e, n, &w_entry->packets_to_send)
		{
			g2_packet_t *entry = list_entry(e, g2_packet_t, list);
			if(!g2_packet_serialize_to_buff(entry, d_target))
			{
				logg_posd(LOGF_DEBUG, "%s Ip: %p#I\tFDNum: %i\n",
				          "failed to encode packet-stream", &w_entry->remote_host,
				          w_entry->com_socket);
				w_entry->flags.dismissed = true;
				return w_entry;
			}
			if(ENCODE_FINISHED != entry->packet_encode)
				break;
			logg_develd_old("removing one: %s\n", g2_ptype_names[entry->type]);
			list_del(e);
			g2_packet_free(entry);
		}
// TODO: handle compression

no_fill_before_write:
		if(!do_write(p_entry, epoll_fd))
			return w_entry;
	}

	/* read */
	if(p_entry->events & (uint32_t)EPOLLIN)
	{
		g2_packet_t tmp_packet;
		/*
		 * shut up gcc! this cannot be used uninitialized, look at
		 * save_build_packet and when it is true...
		 */
		g2_packet_t *build_packet = NULL;
		struct norm_buff *d_source = NULL;
		struct norm_buff *d_target = NULL;
		bool retry, compact_cbuff = false, save_build_packet = false;

		w_entry->flags.last_data_active = false;
		if(!do_read(p_entry))
			return w_entry;
		if(buffer_cempty(*w_entry->recv))
			goto nothing_to_read;

		if(ENC_NONE == w_entry->encoding_in)
			d_source = w_entry->recv;
		else
		{
			buffer_flip(*w_entry->recv);
retry_unpack:
			logg_devel_old("--------- compressed\n");
			d_source = w_entry->recv_u;
			if(buffer_remaining(*w_entry->recv))
			{
		/*if(ENC_DEFLATE == (*w_entry)->encoding_in)*/
				w_entry->z_decoder.next_in = (Bytef *)buffer_start(*w_entry->recv);
				w_entry->z_decoder.avail_in = buffer_remaining(*w_entry->recv);
				w_entry->z_decoder.next_out = (Bytef *)buffer_start(*d_source);
				w_entry->z_decoder.avail_out = buffer_remaining(*d_source);

				logg_develd_old("++++ cbytes: %u\n", buffer_remaining(*w_entry->recv));
				logg_develd_old("**** space: %u\n", buffer_remaining(*d_source));

				switch(inflate(&w_entry->z_decoder, Z_SYNC_FLUSH))
				{
				case Z_OK:
					w_entry->recv->pos += (buffer_remaining(*w_entry->recv) - w_entry->z_decoder.avail_in);
					d_source->pos += (buffer_remaining(*d_source) - w_entry->z_decoder.avail_out);
					break;
				case Z_STREAM_END:
					logg_devel("Z_STREAM_END\n");
					return w_entry;
				case Z_NEED_DICT:
					logg_devel("Z_NEED_DICT\n");
					return w_entry;
				case Z_DATA_ERROR:
					logg_devel("Z_DATA_ERROR\n");
					return w_entry;
				case Z_STREAM_ERROR:
					logg_devel("Z_STREAM_ERROR\n");
					return w_entry;
				case Z_MEM_ERROR:
					logg_devel("Z_MEM_ERROR\n");
					return w_entry;
				case Z_BUF_ERROR:
					logg_devel("Z_BUF_ERROR\n");
					break;
				default:
					logg_devel("inflate was not Z_OK\n");
					return w_entry;
				}
			}
			compact_cbuff = true;
		}

		logg_develd_old("++++ ospace: %u\n", buffer_remaining(*w_entry->recv));
		logg_develd_old("**** space: %u\n", buffer_remaining(*d_source));
		buffer_flip(*d_source);
		if(buffer_remaining(*d_source)) do
		{
			logg_devel_old("---------- reread\n");
			build_packet = w_entry->build_packet;
			if(!build_packet) {
				build_packet = &tmp_packet;
				g2_packet_init_on_stack(build_packet);
			}
			else
				logg_develd("taking %p\n", build_packet);

			logg_develd_old("**** bytes: %u\n", buffer_remaining(*d_source));

			if(!g2_packet_extract_from_stream(d_source, build_packet, server.settings.default_max_g2_packet_length))
			{
				logg_posd(LOGF_DEBUG, "%s Ip: %p#I\tFDNum: %i\n",
				          "failed to decode packet-stream", &w_entry->remote_host,
				          w_entry->com_socket);
				w_entry->flags.dismissed = true;
				return w_entry;
			}
			else
			{
				if(DECODE_FINISHED == build_packet->packet_decode ||
				   PACKET_EXTRACTION_COMPLETE == build_packet->packet_decode)
				{
					struct ptype_action_args parg;
					if(ENC_DEFLATE == w_entry->encoding_out)
						d_target = w_entry->send_u;
					else
						d_target = w_entry->send;

					parg.connec   = w_entry;
					parg.src_addr = NULL;
					parg.dst_addr = NULL;
					parg.source   = build_packet;
					parg.target   = &w_entry->packets_to_send;
					parg.opaque   = NULL;
					if(g2_packet_decide_spec(&parg, g2_packet_dict))
					{
						if(!(w_entry->poll_interrests & (uint32_t)EPOLLOUT))
						{
							struct epoll_event tmp_eevent = {0,{0}};
							w_entry->poll_interrests |= (uint32_t)EPOLLOUT;
							tmp_eevent.events = w_entry->poll_interrests;
							tmp_eevent.data.ptr = w_entry;
							if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, w_entry->com_socket, &tmp_eevent)) {
								logg_errno(LOGF_DEBUG, "changing EPoll interrests");
								return w_entry;
							}
						}
					}

					save_build_packet = false;
					/* !!! packet is seen as finished here !!! */
					if(build_packet->is_freeable)
						logg_devel("freeing durable packet\n");
					else if(build_packet->data_trunk_is_freeable)
						logg_devel("datatrunk freed\n");

					if(w_entry->build_packet &&
					   w_entry->build_packet != build_packet)
						logg_devel("w_entrys packet is not the same???");

					g2_packet_free(build_packet);
					w_entry->build_packet = NULL;

					if(buffer_remaining(*d_source))
						retry = true;
					else
						retry = false;
				}
				else
				{
					/*
					 * it is assumed that a package ending here has state to
					 * be saved until next recv on the one hand, but does not
					 * contain volatile data
					 */
					if(build_packet->packet_decode != CHECK_CONTROLL_BYTE)
						save_build_packet = true;
					retry = false;
				}
			}
		} while(retry);

		buffer_compact(*d_source);

		if(ENC_NONE != w_entry->encoding_in)
		{
// TODO: loop till zlib says more data
			if(buffer_remaining(*w_entry->recv))
				goto retry_unpack;
			if(compact_cbuff)
				buffer_compact(*w_entry->recv);
		}

		if(w_entry->flags.last_data_active) {
			w_entry->last_active = local_time_now;
			timeout_advance(&w_entry->active_to, HANDLER_ACTIVE_TIMEOUT);
		}

		/* after last chance to get more data, save a partial decoded packet */
		if(save_build_packet && build_packet == &tmp_packet)
		{
			/* copy build packet to durable storage */
			g2_packet_t *t = g2_packet_clone(build_packet);
			if(!t) {
				logg_errno(LOGF_DEBUG, "allocating durable packet");
				return w_entry;
			}
			w_entry->build_packet = t;
		}

// TODO: try to write if not already written on this connec?
	}

nothing_to_read:
	manage_buffer_after(&w_entry->recv, lrecv_buff);
	manage_buffer_after(&w_entry->send, lsend_buff);

	return NULL;
}

static void handler_active_timeout(void *arg)
{
	g2_connection_t *con = arg;

// TODO: send a ping
	logg_develd("%p is inactive for %lus! last: %lu\n",
	            con, time(NULL) - con->last_active, con->last_active);
}

static void clean_up_h(struct epoll_event *poll_me, struct norm_buff *lrecv_buff,
                       struct norm_buff *lsend_buff, int epoll_fd, int who_to_say)
{
	if(0 > send(who_to_say, "All lost", sizeof("All lost"), 0))
		diedie("initiating stop"); // hate doing this, but now it's to late
	logg_pos(LOGF_NOTICE, "should go down\n");

	free(poll_me);

	recv_buff_local_ret(lrecv_buff);
	recv_buff_local_ret(lsend_buff);

	// If this happens, its maybe Dangerous, or trivial, so what to do?
	if(0 > my_epoll_close(epoll_fd))
		logg_errno(LOGF_ERR, "closing epoll-fd");

	server.status.all_abord[THREAD_HANDLER] = false;
}

static char const rcsid_h[] GCC_ATTR_USED_VAR = "$Id: G2Handler.c,v 1.21 2005/07/29 09:24:04 redbully Exp redbully $";
//EOF
