/*
 * G2Handler.c
 * thread to handle G2-Protocol
 *
 * Copyright (c) 2004, Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 * 
 * g2cd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with g2cd; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: G2Handler.c,v 1.21 2005/07/29 09:24:04 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
// System includes
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
//#include <ctype.h>
#include <zlib.h>
#include <alloca.h>
// System net-includes
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
// other
#include "other.h"
// Own includes
#define _G2HANDLER_C
#include "G2Handler.h"
#include "G2MainServer.h"
#include "G2Connection.h"
#include "G2ConHelper.h"
#define _NEED_G2_P_TYPE
#include "G2Packet.h"
#include "G2PacketSerializer.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/my_epoll.h"

#define EVENT_SPACE 16

//internal prototypes
static inline bool init_memory_h(struct epoll_event **, struct g2_con_info **, int *);
static inline bool handle_from_accept(struct g2_con_info **, int, int);
static inline g2_connection_t **handle_socket_io(struct epoll_event *, int epoll_fd);
// do not inline, we take a pointer of it, and when its called, performance doesn't matter
static void clean_up_h(struct epoll_event *, struct g2_con_info *, int, int);

void *G2Handler(void *param)
{
	//data-structures
	struct g2_con_info *work_cons = NULL;

	//sock-things
	int from_accept = -1;
	int epoll_fd = -1;
	int sock2main;

	//other variables
	struct epoll_event *eevents = NULL;
	struct epoll_event *e_wptr = NULL;
	size_t i;
	int num_poll = 0;
	bool keep_going = true;

	sock2main = *((int *)param);
	logg(LOGF_DEBUG, "Handler:\tOur SockFD -> %d\t\tMain SockFD -> %d\n", sock2main, *(((int *)param)-1));

	// getting memory for our FD's and everything else
	if(!init_memory_h(&eevents, &work_cons, &epoll_fd))
	{ 
		if(0 > send(sock2main, "All lost", sizeof("All lost"), 0))
			diedie("initiating stop"); // hate doing this, but now it's to late
		logg_pos(LOGF_ERR, "should go down\n");
		server.status.all_abord[THREAD_HANDLER] = false;
		pthread_exit(NULL);
	}
	logg(LOGF_DEBUG, "Handler:\tEPoll-FD -> %i\n", epoll_fd);

	if(sizeof(from_accept) != recv(sock2main, &from_accept, sizeof(from_accept), 0))
	{
		logg_errno(LOGF_ERR, "retrieving IPC Pipe");
		clean_up_h(eevents, work_cons, epoll_fd, sock2main);
		pthread_exit(NULL);
	}
	logg(LOGF_DEBUG, "Handler:\tfrom_accept -> %i\n", from_accept);

	// Setting first entry to be polled, our Pipe from Acceptor
	eevents->events = EPOLLIN;
	// Attention - very ugly, but i have to distinguish these two sockets, and all the other
	eevents->data.ptr = (void *)1;
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, from_accept, eevents))
	{
		logg_errno(LOGF_ERR, "adding acceptor-pipe to epoll");
		clean_up_h(eevents, work_cons, epoll_fd, sock2main);
		pthread_exit(NULL);
	}
	eevents->data.ptr = (void *)0;
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock2main, eevents))
	{
		logg_errno(LOGF_ERR, "adding main-pipe to epoll");
		clean_up_h(eevents, work_cons, epoll_fd, sock2main);
		pthread_exit(NULL);
	}

	// we are up and running
	server.status.all_abord[THREAD_HANDLER] = true;
	while(keep_going)
	{
		// Let's do it
		num_poll = my_epoll_wait(epoll_fd, eevents, EVENT_SPACE, 8000);
		e_wptr = eevents;
		switch(num_poll)
		{
		// Normally: see what has happened
		default:
			for(i = num_poll; i; i--, e_wptr++)
			{
				// A common Socket
				if(e_wptr->data.ptr)
				{
					if(((void *)1) != e_wptr->data.ptr)
					{
						logg_develd_old("-------- Events: 0x%0X, PTR: %p\n", e_wptr->events, e_wptr->data.ptr);
						// Any problems?
						// 'Where did you go my lovely...'
						// Any invalid FD's remained in PollData?
						if(e_wptr->events & ~((uint32_t)(EPOLLIN|EPOLLOUT)))
						{
							g2_connection_t **tmp_con_holder = handle_socket_abnorm(e_wptr);
							if(NULL != tmp_con_holder)
								recycle_con(eevents, tmp_con_holder, work_cons, epoll_fd, sock2main, false, &clean_up_h);
							else
							{
								logg_pos(LOGF_ERR, "Somethings wrong with our polled FD's, couldn't solve it\n");
								keep_going = false;
							}
						}
						else
						{
							// Some FD's ready to be filled?
							// Some data ready to be read in?
							g2_connection_t **tmp_con_holder = handle_socket_io(e_wptr, epoll_fd);
							if(NULL != tmp_con_holder)
							{
								recycle_con(eevents, tmp_con_holder, work_cons, epoll_fd, sock2main, false, &clean_up_h);
							}
						}
					}
					// the from_acceptor-pipe
					else
					{
						if(e_wptr->events & (uint32_t) EPOLLIN)
						{
							handle_from_accept(&work_cons, from_accept, epoll_fd);
						}
						// if there is no read-interrest, we're blown up
						else
						{
							logg_pos(LOGF_ERR, "from_acceptor-pipe is wired\n");
							keep_going = false;
							break;
						}
					}
				}
				// the abort-socket
				else
				{
// TODO: Check for a proper stop-sequence ??
					// everything but 'ready for write' means:
					// we're finished...
					if(e_wptr->events & ~((uint32_t)EPOLLOUT))
					{
						keep_going = false;
						break;
					}

					// else stop this write interrest
					e_wptr->events = (uint32_t)EPOLLIN;
					if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock2main, e_wptr))
					{
						logg_errno(LOGF_ERR, "changing epoll-interrests for socket-2-main");
						keep_going = false;
						break;
					}
				}
			}
			break;
		// Nothing happened (or just the Timeout)
		case 0:
			//putchar('*');
			//fflush(stdout);
			break;
		// Something bad happened
		case -1:
			if(EINTR == errno)
				break;
			// Print what happened
			logg_errno(LOGF_ERR, "poll");
			//and get out here (at the moment)
			keep_going = false;
			//exit(EXIT_FAILURE);
			break;
		}
	}

	clean_up_h(eevents, work_cons, epoll_fd, sock2main);
	pthread_exit(NULL);
	return NULL; // to avoid warning about reaching end of non-void funktion
}

static inline bool init_memory_h(struct epoll_event **poll_me, struct g2_con_info **work_cons, int *epoll_fd)
{
	*poll_me = calloc(EVENT_SPACE, sizeof(**poll_me));
	if(NULL == *poll_me)
	{
		logg_errno(LOGF_ERR, "EPoll eventspace memory");
		return false;
	}

	*work_cons = calloc(1, sizeof(struct g2_con_info) + (WC_START_CAPACITY * sizeof(g2_connection_t *)));
	if(NULL == *work_cons)
	{
		logg_errno(LOGF_ERR, "work_cons[] memory");
		free(*poll_me);
		return false;
	}
	(*work_cons)->capacity = WC_START_CAPACITY;

	*epoll_fd = my_epoll_create(PD_START_CAPACITY);
	if(0 > *epoll_fd)
	{
		logg_errno(LOGF_ERR, "creating epoll-fd");
		return false;
	}

	return true;
}

static inline bool handle_from_accept(struct g2_con_info **work_cons, int from_acceptor, int epoll_fd)
{
	struct epoll_event tmp_eevent;
	g2_connection_t *recvd_con = NULL;
	ssize_t ret_val;

	do
	{
		ret_val = read(from_acceptor, &recvd_con, sizeof(recvd_con));
	} while(-1 == ret_val && EINTR == errno);
	if(sizeof(recvd_con) != ret_val)
	{
		logg_errno(LOGF_INFO, "recieving new Connection");
		return false;
	}

	if((*work_cons)->limit == (*work_cons)->capacity)
	{
		struct g2_con_info *tmp_pointer = realloc(*work_cons, sizeof(struct g2_con_info) + (((*work_cons)->capacity + WC_CAPACITY_INCREMENT) * sizeof(g2_connection_t *)));
		if(NULL == tmp_pointer)
		{
			logg_errno(LOGF_DEBUG, "reallocing work_con[]");
			goto clean_up;
		}
		*work_cons = tmp_pointer;
		(*work_cons)->capacity += WC_CAPACITY_INCREMENT;
		logg_devel("work_con[] reallocated\n");
	}

	tmp_eevent.events = recvd_con->poll_interrests = (uint32_t)(EPOLLIN | EPOLLERR | EPOLLHUP);
	tmp_eevent.data.ptr = &(*work_cons)->data[(*work_cons)->limit];
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, recvd_con->com_socket, &tmp_eevent))
	{
		logg_errno(LOGF_DEBUG, "adding new socket to EPoll");
		goto clean_up;
	}

	(*work_cons)->data[(*work_cons)->limit] = recvd_con;
	(*work_cons)->limit++;	
	return true;
clean_up:
	while(-1 == close(recvd_con->com_socket) && EINTR == errno);
	g2_con_clear(recvd_con);
	if(!return_free_con(recvd_con, __FILE__, __func__, __LINE__))
		die("returning g2 con failed, will go down");
	return false;
}

static inline g2_connection_t **handle_socket_io(struct epoll_event *p_entry, int epoll_fd)
{
	g2_connection_t **w_entry = (g2_connection_t **)p_entry->data.ptr;

	if(p_entry->events & (uint32_t)EPOLLOUT)
	{
		if(!do_write(p_entry, epoll_fd))
			return w_entry;
	}

	if(p_entry->events & (uint32_t)EPOLLIN)
	{
		bool	retry;

		if(!do_read(p_entry))
			return w_entry;

		logg_devel_old("---------- reread\n");

		do
		{
			struct norm_buff *d_source = NULL;
			
			if(ENC_DEFLATE == (*w_entry)->encoding_in)
			{
				logg_devel_old("--------- compressed\n");
				d_source = (*w_entry)->recv_u;
				buffer_flip((*w_entry)->recv);
				if(buffer_remaining((*w_entry)->recv))
				{
					(*w_entry)->z_decoder.next_in = (Bytef *)buffer_start((*w_entry)->recv);
					(*w_entry)->z_decoder.avail_in = buffer_remaining((*w_entry)->recv);
					(*w_entry)->z_decoder.next_out = (Bytef *)buffer_start(*d_source);
					(*w_entry)->z_decoder.avail_out = buffer_remaining(*d_source);

					logg_develd_old("++++ bytes: %u\n", buffer_remaining((*w_entry)->recv));
					logg_develd_old("**** space: %u\n", buffer_remaining(*d_source));

					switch(inflate(&(*w_entry)->z_decoder, Z_SYNC_FLUSH))
					{
					case Z_OK:
						(*w_entry)->recv.pos += (buffer_remaining((*w_entry)->recv) - (*w_entry)->z_decoder.avail_in);
						d_source->pos += (buffer_remaining(*d_source) - (*w_entry)->z_decoder.avail_out);
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
				buffer_compact((*w_entry)->recv);
			}
			//if(ENC_NONE == (*w_entry)->encoding_in)
			else
				d_source = &(*w_entry)->recv;

			
/*			if(!(*w_entry)->build_packet)
			{
				(*w_entry)->build_packet = calloc(1, sizeof(*(*w_entry)->build_packet));
				if(!(*w_entry)->build_packet)
				{
					logg_errno(LOGF_DEBUG, "packet allocation");
					return w_entry;
				}
				(*w_entry)->build_packet->packet_decode = CHECK_CONTROLL_BYTE;
			}*/

			logg_develd_old("++++ space: %u\n", buffer_remaining((*w_entry)->recv));
			logg_develd_old("**** space: %u\n", buffer_remaining(*d_source));
			buffer_flip(*d_source);
			logg_develd_old("**** bytes: %u\n", buffer_remaining(*d_source));

			if(!g2_packet_extract_from_stream(d_source, (*w_entry)->build_packet, server.settings.default_max_g2_packet_length))
			{
				char addr_buf[INET6_ADDRSTRLEN];
				logg_posd(LOGF_DEBUG, "%s Ip: %s\tPort: %hu\tFDNum: %i\n",
					"failed to decode packet-stream",
					inet_ntop((*w_entry)->af_type, &(*w_entry)->remote_host.sin_addr, addr_buf, sizeof(addr_buf)),
					//inet_ntoa(w_entry->remote_host.sin_addr),
					ntohs((*w_entry)->remote_host.sin_port),
					(*w_entry)->com_socket);			
				(*w_entry)->flags.dismissed = true;
				return w_entry;
			}
			else
			{
				if(DECODE_FINISHED == (*w_entry)->build_packet->packet_decode ||
					PACKET_EXTRACTION_COMPLETE == (*w_entry)->build_packet->packet_decode)
				{
					g2_packet_t *tmp_packet = (*w_entry)->build_packet;
					(*w_entry)->build_packet = (*w_entry)->akt_packet;
					(*w_entry)->akt_packet = tmp_packet;

					g2_packet_clean((*w_entry)->build_packet);
/*					g2_packet_free((*w_entry)->last_packet);
					(*w_entry)->last_packet = (*w_entry)->akt_packet;
					(*w_entry)->akt_packet = (*w_entry)->build_packet;
					(*w_entry)->build_packet = NULL;*/

					if(ENC_DEFLATE == (*w_entry)->encoding_out)
						d_source = (*w_entry)->send_u;
					else
						d_source = &(*w_entry)->send;

					if(g2_packet_decide(*w_entry, d_source, &g2_packet_dict))
					{
						struct epoll_event tmp_eevent;

						(*w_entry)->poll_interrests |= (uint32_t)EPOLLOUT;
						tmp_eevent.events = (*w_entry)->poll_interrests;
						tmp_eevent.data.ptr = w_entry;
						if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, (*w_entry)->com_socket, &tmp_eevent))
						{
							logg_errno(LOGF_DEBUG, "changing EPoll interrests");
							return w_entry;
						}
					}

					retry = true;
				}
				else
					retry = false;
			}
		} while(retry);
	}
	return NULL;
}

static void clean_up_h(struct epoll_event *poll_me, struct g2_con_info *work_cons, int epoll_fd, int who_to_say)
{
	size_t i;

	if(0 > send(who_to_say, "All lost", sizeof("All lost"), 0))
		diedie("initiating stop"); // hate doing this, but now it's to late
	logg_pos(LOGF_NOTICE, "should go down\n");

	free(poll_me);

	for(i = 0; i < work_cons->limit; i++)
	{
		close(work_cons->data[i]->com_socket);
		g2_con_free(work_cons->data[i]);
	}
	
	free(work_cons);

	// If this happens, its maybe Dangerous, or trivial, so what to do?
	if(0 > my_epoll_close(epoll_fd))
		logg_errno(LOGF_ERR, "closing epoll-fd");

	server.status.all_abord[THREAD_HANDLER] = false;
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id: G2Handler.c,v 1.21 2005/07/29 09:24:04 redbully Exp redbully $";
//EOF
