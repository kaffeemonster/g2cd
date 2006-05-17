/*
 * G2Acceptor.c
 * thread to accept connection and handshake G2
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
 * $Id: G2Acceptor.c,v 1.19 2005/08/21 22:59:12 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
// System includes
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <zlib.h>
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
#define _G2ACCEPTOR_C
#include "G2Acceptor.h"
#include "G2MainServer.h"
#include "G2Connection.h"
#include "G2ConHelper.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/my_epoll.h"
#include "lib/atomic.h"

#define EVENT_SPACE 8

//internal prototypes
static inline bool init_memory_a(struct epoll_event **, struct g2_con_info **, int *);
static inline bool init_con(int *, struct sockaddr_in *);
static inline bool handle_accept_in(int, struct g2_con_info *, g2_connection_t **, int, int, struct epoll_event *);
static inline bool handle_accept_abnorm(struct epoll_event *, int, int);
static inline g2_connection_t **handle_socket_io(struct epoll_event *, int epoll_fd);
static inline bool initiate_g2(g2_connection_t *);
// do not inline, we take a pointer of it, and when its called, performance doesn't matter
static void clean_up_a(struct epoll_event *, struct g2_con_info *, int, int);

void *G2Accept(void *param)
{
	//data-structures
	struct g2_con_info *work_cons = NULL;
	g2_connection_t *work_entry = NULL;

	//sock-things
	int accept_so = -1;
	int to_handler = -1;
	int epoll_fd = -1;
	int sock2main;
	struct sockaddr_in our_addr;

	//other variables
	struct epoll_event *eevents = NULL;
	struct epoll_event *e_wptr = NULL;
	size_t i = 0;
	int num_poll = 0;
	bool refresh_needed = true;
	bool keep_going = true;

	sock2main = *((int *)param);
	logg(LOGF_DEBUG, "Accept:\t\tOur SockFD -> %d\t\tMain SockFD -> %d\n", sock2main, *(((int *)param)-1));

	// Setting up the accepting Socket
	if(!init_con(&accept_so, &our_addr))
	{
		if(0 > send(sock2main, "All lost", sizeof("All lost"), 0))
			diedie("initiating stop"); // hate doing this, but now it's to late
		logg_pos(LOGF_ERR, "should go down\n");
		server.status.all_abord[THREAD_ACCEPTOR] = false;
		pthread_exit(NULL);
	}
	// getting memory for our FD's and everything else
	if(!init_memory_a(&eevents, &work_cons, &epoll_fd))
	{
		close(accept_so);
		if(0 > send(sock2main, "All lost", sizeof("All lost"), 0))
			diedie("initiating stop"); // hate doing this, but now it's to late
		logg_pos(LOGF_ERR, "should go down\n");
		server.status.all_abord[THREAD_ACCEPTOR] = false;
		pthread_exit(NULL);
	}
	logg(LOGF_DEBUG, "Accept:\t\tEPoll-FD -> %i\n", epoll_fd);

	if(sizeof(to_handler) != recv(sock2main, &to_handler, sizeof(to_handler), 0))
	{
		logg_errno(LOGF_ERR, "retrieving IPC Pipe");
		clean_up_a(eevents, work_cons, epoll_fd, sock2main);
		close(accept_so);
		pthread_exit(NULL);
	}

	logg(LOGF_DEBUG, "Accept:\t\tto_handler -> %d\n", to_handler);

	// Setting first entry to be polled, our Acceptsocket
	eevents->events = (uint32_t)(EPOLLIN | EPOLLERR);
	// Attention - very ugly, but i have to distinguish these two sockets, and all the other
	eevents->data.ptr = (void *)1;
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, accept_so, eevents))
	{
		logg_errno(LOGF_ERR, "adding acceptor-socket to epoll");
		clean_up_a(eevents, work_cons, epoll_fd, sock2main);
		close(accept_so);
		pthread_exit(NULL);
	}
	eevents->events = (uint32_t)EPOLLIN;
	eevents->data.ptr = (void *)0;
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock2main, eevents))
	{
		logg_errno(LOGF_ERR, "adding main-pipe to epoll");
		clean_up_a(eevents, work_cons, epoll_fd, sock2main);
		close(accept_so);
		pthread_exit(NULL);
	}

	// Getting the first Connections to work with
	if(!(work_entry = g2_con_get_free()))
	{
		clean_up_a(eevents, work_cons, epoll_fd, sock2main);
		close(accept_so);
		pthread_exit(NULL);
	}

	// we are up and running
	server.status.all_abord[THREAD_ACCEPTOR] = true;
	/* 
	 * We'll be doing this quite a long time, so we have to make sure, to get
	 * not confused by a EINTR and leak ressources (for example when close()ing
	 * a fd), execpt when we bail out.
	 */
	while(keep_going)
	{
		// Do we need to clean out the Connection? (maybe got tainted last round)
		if(refresh_needed)
		{
			g2_con_clear(work_entry);
			refresh_needed = false;
		}

		// Let's do it
		num_poll = my_epoll_wait(epoll_fd, eevents, EVENT_SPACE, 10000);
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
						logg_develd_old(" Events: 0x%0X\t  PTR: %p\n", e_wptr->events, (void *) e_wptr->data.ptr);
						logg_develd_old("Revents: 0x%0X\tFDNum: %i\n", p_wptr->revents, p_wptr->fd);

						// Any problems?
						// 'Where did you go my lovely...'
						// Any invalid FD's remained in PollData?
						if(e_wptr->events & ~((uint32_t)(EPOLLIN|EPOLLOUT)))
						{
							g2_connection_t **tmp_con_holder = handle_socket_abnorm(e_wptr);
							if(tmp_con_holder)
								recycle_con(eevents, tmp_con_holder, work_cons, epoll_fd, sock2main, false, &clean_up_a);
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
							if(tmp_con_holder)
							{
								if((*tmp_con_holder)->flags.dismissed)
									recycle_con(eevents, tmp_con_holder, work_cons, epoll_fd, sock2main, false, &clean_up_a);
								else if(G2CONNECTED == (*tmp_con_holder)->connect_state)
								{
									g2_connection_t *tmp_con = *tmp_con_holder;
									recycle_con(eevents, tmp_con_holder, work_cons, epoll_fd, sock2main, true, &clean_up_a);
									if(sizeof(tmp_con) != write(to_handler, &tmp_con, sizeof(tmp_con)))
									{
										logg_errno(LOGF_NOTICE, "sending connection to Handler");
										close(tmp_con->com_socket);
										g2_con_clear(tmp_con);
										if(!g2_con_ret_free(tmp_con))
										{
											clean_up_a(eevents, work_cons, epoll_fd, sock2main);
											close(accept_so);
											pthread_exit(NULL);
										}										
									}
								}	
								else
									recycle_con(eevents, tmp_con_holder, work_cons, epoll_fd, sock2main, false, &clean_up_a);
							}
						}
					}
					// the accept-socket
					else
					{
						// Some data ready to be read in?
						if(e_wptr->events & ((uint32_t)EPOLLIN))
						{
							// If our accept_so is ready reading, we have to handle it differently
							refresh_needed = true;
							handle_accept_in(accept_so, work_cons, &work_entry, epoll_fd, sock2main, eevents);
						}

						// If our accept_so is ready writing, we have to switch the interrest off
						// Any problems?
						// 'Where did you go my lovely...'
						// Accept_socket become invalid? Dooh
						if(e_wptr->events & ~((uint32_t)EPOLLIN))
							keep_going = handle_accept_abnorm(e_wptr, epoll_fd, accept_so);
					}
				}
				// the abort-socket
				else
				{
// TODO: Check for a proper stop-sequence ??
					// everything but 'ready for write' means:
					// we're finished...
					if(e_wptr->events & ~((uint32_t)EPOLLOUT))
						keep_going = false;
					else
					{
						// else stop this write interrest
						e_wptr->events = EPOLLIN;
						if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock2main, e_wptr))
						{
							logg_errno(LOGF_ERR, "changing epoll-interrests for socket-2-main");
							keep_going = false;
						}
					}
				}
			}
			break;
		// Nothing happened (or just the Timeout)
		case 0:
		//	putchar('.');
		//	fflush(stdout);
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

	g2_con_free(work_entry);
	clean_up_a(eevents, work_cons, epoll_fd, sock2main);
	close(accept_so);
	pthread_exit(NULL);
	return NULL; // to avoid warning about reaching end of non-void funktion
}

static inline bool init_con(int *accept_so, struct sockaddr_in *our_addr)
{
	int yes = 1; // for setsocketopt() SO_REUSEADDR, below

	if(0 > (*accept_so = socket(AF_INET, SOCK_STREAM, 0)))
	{
		logg_errno(LOGF_ERR, "socket");
		return false;
	}
	
	// lose the pesky "address already in use" error message
	if(setsockopt(*accept_so, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)))
	{
		logg_errno(LOGF_ERR, "setsockopt");
		close(*accept_so);
		return false;
	}

	memset(our_addr, 0, sizeof(*our_addr));
	our_addr->sin_family = AF_INET;
	our_addr->sin_port = server.settings.portnr_2_bind;
	our_addr->sin_addr.s_addr = server.settings.ip_2_bind;

	if(bind(*accept_so, (struct sockaddr *)our_addr, sizeof(struct sockaddr_in)))
	{
		logg_errno(LOGF_ERR, "bind");
		close(*accept_so);
		return false;
	}

	// Die gebundene Ip rausfinden?
	//socklen_t len;
	//getsockname(sock, (sockaddr *) &my_addr, &len);
	//logg_develd_old("Port: %d\n", ntohs(my_addr.sin_port));

	if(listen(*accept_so, BACKLOG))
	{
		logg_errno(LOGF_ERR, "listen");
		close(*accept_so);
		return false;
	}

	if(fcntl(*accept_so, F_SETFL, O_NONBLOCK))
	{
		logg_errno(LOGF_ERR, "accept non-blocking");
		close(*accept_so);
		return false;
	}

	return true;
}

static inline bool init_memory_a(struct epoll_event **poll_me, struct g2_con_info **work_cons, int *epoll_fd)
{
	*poll_me = calloc(EVENT_SPACE, sizeof(**poll_me));
	if(!*poll_me)
	{
		logg_errno(LOGF_ERR, "poll_data memory");
		return false;
	}

	*work_cons = calloc(1, sizeof(struct g2_con_info) + (WC_START_CAPACITY * sizeof(g2_connection_t *)));
	if(!*work_cons)
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

static inline bool handle_accept_in(
	int accept_so,
	struct g2_con_info *work_cons,
	g2_connection_t **work_entry,
	int epoll_fd,
	int abort_fd,
	struct epoll_event *poll_me)
{
	struct epoll_event tmp_eevent = {0,{0}};
	int tmp_fd;
	int fd_flags;

	do
		tmp_fd = accept(accept_so, (struct sockaddr *) &((*work_entry)->remote_host), &((*work_entry)->sin_size));
	while(0 > tmp_fd && EINTR == errno);
	if(-1 == tmp_fd)
	{
		logg_errno(LOGF_NOTICE, "accepting");
		return false;
	}
	(*work_entry)->com_socket = tmp_fd;
	{
		char addr_buf[INET6_ADDRSTRLEN];

		logg_posd(LOGF_DEBUG, "%s IP: %s\tPort: %hu\tFDNum: %i\n",
		"A connection!",
		inet_ntop((*work_entry)->af_type, &(*work_entry)->remote_host.sin_addr, addr_buf, sizeof(addr_buf)),
		//inet_ntoa((*work_entry)->remote_host.sin_addr),
		ntohs((*work_entry)->remote_host.sin_port),
		(*work_entry)->com_socket);
	}


	/* check if our total server connection limit is reached */
	if(atomic_read(&server.status.act_connection_sum) >= server.settings.max_connection_sum)
	{
		logg_pos(LOGF_INFO, "too many connections\n");
		while(-1 == close((*work_entry)->com_socket) && EINTR == errno);
		return false;
	}
// TODO: Little race, but its only the connection limit
	/* increase our total server connection sum */
	atomic_inc(&server.status.act_connection_sum);

	/* room for another connection? */
	/* originaly ment to be reallocated */
	if(work_cons->limit == work_cons->capacity)
	{
		goto err_out_after_count;
	}
/*	if(poll_me->limit == poll_me->capacity)
	{
		goto err_out_after_count;
	}*/

	
	/* get the fd-flags and add nonblocking  */
	/* according to POSIX manpage EINTR is only encountered when the cmd was F_SETLKW */
	if(-1 == (fd_flags = fcntl((*work_entry)->com_socket, F_GETFL)))
	{
		logg_errno(LOGF_NOTICE, "getting socket fd-flags");
		goto err_out_after_count;
	}
	if(fcntl((*work_entry)->com_socket, F_SETFL, fd_flags | O_NONBLOCK))
	{
		logg_errno(LOGF_NOTICE, "setting socket non-blocking");
		goto err_out_after_count;
	}

	
	/* No EINTR in epoll_ctl according to manpage :-/ */
	tmp_eevent.events = (*work_entry)->poll_interrests = (uint32_t)(EPOLLIN | EPOLLERR | EPOLLHUP);
	tmp_eevent.data.ptr = &work_cons->data[work_cons->limit];
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, (*work_entry)->com_socket, &tmp_eevent))
	{
		logg_errno(LOGF_NOTICE, "adding new socket to EPoll");
		goto err_out_after_count;
	}

	work_cons->data[work_cons->limit] = *work_entry;
	work_cons->limit++;
	*work_entry = g2_con_get_free();
	if(!*work_entry)
	{
		clean_up_a(poll_me, work_cons, epoll_fd, abort_fd);
		pthread_exit(NULL);
	}

	return true;

err_out_after_count:
	/* the connection failed, but we have already counted it, so remove it from count */
	atomic_dec(&server.status.act_connection_sum);
	
	while(-1 == close((*work_entry)->com_socket) && EINTR == errno);
	return false;
}

static inline bool handle_accept_abnorm(struct epoll_event *accept_ptr, int epoll_fd, int accept_so)
{
	bool ret_val = true;
	const char *msg = NULL;
	const char *extra_msg = NULL;
	enum loglevel needed_level = LOGF_INFO;
	
	if(accept_ptr->events & (uint32_t)EPOLLOUT)
	{
		accept_ptr->events = (uint32_t)(EPOLLIN | EPOLLERR);
		if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, accept_so, accept_ptr))
			logg_errno(LOGF_NOTICE, "resetting accept-fd in EPoll to default-interrests");

		msg = "Out-Event for Accept-FD was set";
		extra_msg = "";
	}

	if(accept_ptr->events & (uint32_t)(EPOLLERR|EPOLLHUP)) //|EPOLLNVAL))
	{
		ret_val = false;
		extra_msg = "\n\tSTOPPING";
		if(accept_ptr->events & (uint32_t)EPOLLERR)
			msg = "error set forAccept-FD!";
		if(accept_ptr->events & (uint32_t)EPOLLHUP)
			msg = "HUP set for Accept-FD?";
// FUCK-SHIT: if accept-so becomes invalid, we will not recognize it with EPoll,
// waiting for ever for new connections
//		if(accept_ptr->events & (uint32_t)EPOLLNVAL)
//			msg = "NVal set for Accept-FD!";
		needed_level = LOGF_ERR;
	}
	
	logg_posd(needed_level, "%s%s\n", msg, extra_msg);
	return ret_val;
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
		if(!do_read(p_entry))
			return w_entry;

		if(initiate_g2(*w_entry))
		{
			p_entry->events = (*w_entry)->poll_interrests |= (uint32_t) EPOLLOUT;
			if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, (*w_entry)->com_socket, p_entry))
			{
				logg_errno(LOGF_NOTICE, "changing sockets Epoll-interrests");
				(*w_entry)->flags.dismissed = true;
				return w_entry;
			}
		}
		else
		{
			if(G2CONNECTED == (*w_entry)->connect_state)
				return w_entry;
			else if((*w_entry)->flags.dismissed)
			{
				char addr_buf[INET6_ADDRSTRLEN];
				logg_posd(LOGF_DEBUG, "%s Ip: %s\tPort: %hu\tFDNum: %i\n",
					"Dismissed!",
					inet_ntop((*w_entry)->af_type, &(*w_entry)->remote_host.sin_addr, addr_buf, sizeof(addr_buf)),
					//inet_ntoa(w_entry->remote_host.sin_addr),
					ntohs((*w_entry)->remote_host.sin_port),
					(*w_entry)->com_socket);
				return w_entry;
			}
		}
	}
	return NULL;
}

static void clean_up_a(struct epoll_event *poll_me, struct g2_con_info *work_cons, int epoll_fd, int who_to_say)
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
	
	server.status.all_abord[THREAD_ACCEPTOR] = false;
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
#define my_mempcpy(x, y)	(memcpy((x), (y), str_size(y)))
static inline bool initiate_g2(g2_connection_t *to_con)
{
	size_t found = 0;
	size_t old_pos = 0;
	size_t old_limit = 0;
	size_t search_state = 0;
	size_t distance = 0;
	size_t field_num = 0;
	size_t pr_ch = 0;
	bool ret_val = false;
	bool more_bytes_needed = false;
	bool field_found = false;

	buffer_flip(to_con->recv);
	//printf("Ackt. Inhalt:\n%.*s", buffer_remaining(to_con->recv), buffer_start(to_con->recv));

	// compute all data available
	while(!more_bytes_needed)
	{
		switch(to_con->connect_state)
		{
	// at this point we need at least the first line
		case UNCONNECTED:
			if((str_size(GNUTELLA_CONNECT_STRING) + 2) > buffer_remaining(to_con->recv))
			{
				more_bytes_needed = true;
				break;
			}
			// let's see if we got the right string
			if(!strncmp(buffer_start(to_con->recv), GNUTELLA_CONNECT_STRING, str_size(GNUTELLA_CONNECT_STRING)))
				to_con->recv.pos += str_size(GNUTELLA_CONNECT_STRING);
			else
			{
				// could we place it all in our sendbuffer?
				if(str_size(STATUS_501 "\r\n\r\n") < buffer_remaining(to_con->send))
				{
					my_mempcpy(buffer_start(to_con->send), STATUS_501 "\r\n\r\n");
					to_con->send.pos += str_size(STATUS_501 "\r\n\r\n");
				}
				to_con->flags.dismissed = true;
				return true;
			}

			// CR?
			if('\r' == *buffer_start(to_con->recv))
				to_con->recv.pos++;
			else
			{
				// could we place it all in our sendbuffer?
				if(str_size(STATUS_501 "\r\n\r\n") < buffer_remaining(to_con->send))
				{
					my_mempcpy(buffer_start(to_con->send), STATUS_501 "\r\n\r\n");
					to_con->send.pos += str_size(STATUS_501 "\r\n\r\n");
				}
				to_con->flags.dismissed = true;
				return true;
			}

			// LF?
			if('\n' == *buffer_start(to_con->recv))
			{
				to_con->recv.pos++;
				to_con->connect_state++;
			}
			else
			{
				// could we place it all in our sendbuffer?
				if(str_size(STATUS_501 "\r\n\r\n") < buffer_remaining(to_con->send))
				{
					my_mempcpy(buffer_start(to_con->send), STATUS_501 "\r\n\r\n");
					to_con->send.pos += str_size(STATUS_501 "\r\n\r\n");
				}
				to_con->flags.dismissed = true;
				return true;
			}
// TODO: Use more std.-func.(strcspn, strchr, strstr), but problem: we are not working with propper strings, foreign input!
	// wait till we got the whole header
		case HAS_CONNECT_STRING:
			if(!buffer_remaining(to_con->recv))
			{
				more_bytes_needed = true;
				break;
			}
			old_pos = to_con->recv.pos;
			while(true)
			{
				// CR 0x0D  LF 0x0A
// TODO: Check for a proper CR LF CR LF Sequence
				found = ('\r' != *(buffer_start(to_con->recv)) && '\n'!= *(buffer_start(to_con->recv))) ? 0: found + 1;
				to_con->recv.pos++;
				// there was CRLF 2 times
				if(4 == found)
				{
					to_con->recv.limit = to_con->recv.pos;
					to_con->recv.pos = old_pos;
					logg_devel("\t------------------ Initiator\n");
					//logg_develd("\"%.*s\"\n", buffer_remaining(to_con->recv), buffer_start(to_con->recv));
					//to_con.connect_header1 = linesplit.split(std_utf8.decode(recv_buffer).toString());//tmp_header);
					to_con->connect_state++;
					break;					
				}
				else if(!buffer_remaining(to_con->recv)) // End of Buffer?
				{
					// Header not to long (someone DoS us?)
					if(MAX_HEADER_LENGTH > (to_con->recv.limit - old_pos))
					{
						// we need more bytes
						to_con->recv.pos = old_pos;
						more_bytes_needed = true;
						break;
					}
					else //  or go home
					{
						if(str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n") < buffer_remaining(to_con->send))
						{
							my_mempcpy(buffer_start(to_con->send), GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
							to_con->send.pos += str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
						}
						to_con->flags.dismissed = true;
						return true;
					}
				}
			}
			break;
	// Now the header is complete, we can search it
		case SEARCH_CAPS:
			old_pos = to_con->recv.pos;
			while(buffer_remaining(to_con->recv))
			{
				switch(search_state)
				{
			//search for fieldseparator ':'
				case 0:
					if(':' == *buffer_start(to_con->recv))
					{
						size_t k;
						size_t real_distance = 0;

						// back to start of field
						distance = to_con->recv.pos - old_pos;
						to_con->recv.pos = old_pos;
						
// TODO: use isalnum & isgraph?? They are locale-dependent. hopefully none sends multibyte & non-ascii field-names.
						// filter bogus beginning (skip leading space)
						while(distance && !isalnum((int)(*buffer_start(to_con->recv))))
						{
							to_con->recv.pos++;
							distance--;
						}
						
						// lets see what we can filter to find the "real" end
						// of the field (skip trailing space)
						old_pos = to_con->recv.pos;
						while(distance && isgraph((int)(*buffer_start(to_con->recv))))
						{
							to_con->recv.pos++;
							distance--;
							real_distance++;
						}
						to_con->recv.pos = old_pos;

						// even if we may have filtered all, we are continueing,
						// we could only sync again at a "\r\n" (the field-data may contain ':').
						// Maybe this "\r\n" is the last (it must be send, see above),
						// Ok, then handshaking fails. Not my fault if someone sends such nonsense.
						for(k = 0; k < KNOWN_HEADER_FIELDS_SUM; k++)
						{
							if(real_distance != KNOWN_HEADER_FIELDS[k]->length)
								continue;
						
							if(!strncasecmp(buffer_start(to_con->recv), KNOWN_HEADER_FIELDS[k]->txt, KNOWN_HEADER_FIELDS[k]->length))
							{
								field_num = k;
								field_found = true;
								break;
							}
						}

						if(!field_found)
						{
							logg_develd("unknown field:\t\"%.*s\"\tcontent:\n",
								(int) real_distance,
								buffer_start(to_con->recv));
						}
						else
						{
							if(NULL == KNOWN_HEADER_FIELDS[field_num]->action)
							{
								logg_develd("no action field:\t\"%.*s\"\tcontent:\n",
									(int) real_distance,
									buffer_start(to_con->recv));
							}
							else
							{
								logg_develd_old("  known field:\t\"%.*s\"\tcontent:\n",
									real_distance,
									buffer_start(to_con->recv));
							}
						}

						// since we may have shortened the string due to filtering,
						// we have to use all lengths, to get behind the ':'
						to_con->recv.pos += real_distance + distance + 1;
						old_pos = to_con->recv.pos;
						search_state++;
					}
					break;
			//find end of line
				case 1:
					if('\r' == *buffer_start(to_con->recv))
					{
						search_state++;
					}
					break;
				case 2:
					if('\n' == *buffer_start(to_con->recv))
					{
						// Field-data complete
						// back to start of field-data
						distance = to_con->recv.pos - old_pos - 1;
						to_con->recv.pos = old_pos;

						// remove leading white-spaces in field-data
						while(distance && isspace((int)(*buffer_start(to_con->recv))))
						{
							to_con->recv.pos++;
							distance--;
						}

						// now call the associated action for this field
						if(distance && field_found && NULL != KNOWN_HEADER_FIELDS[field_num]->action)
						{
							KNOWN_HEADER_FIELDS[field_num]->action(to_con, distance);
						}
						else
						{
							if(distance)
							{
								logg_develd("\"%.*s\"\n", (int) distance, buffer_start(to_con->recv));
							}
							else
							{
								logg_devel("Field with no data recieved!\n");
							}
						}
						search_state = 0;
						to_con->recv.pos += distance + 2;
						old_pos = to_con->recv.pos;
						field_found = false;
					}
					else
						search_state--;
					break;
				}
				to_con->recv.pos++;
			}

			to_con->connect_state++;
	// what's up with the accept-field?
		case CHECK_ACCEPT:
			// do we have one?
			if(to_con->flags.accept_ok)
			{
				// and is it G2?
				if(to_con->flags.accept_g2)
					to_con->connect_state++;
				else
				{
					if(str_size(GNUTELLA_STRING " " STATUS_501 "\r\n\r\n") < buffer_remaining(to_con->send))
					{
						my_mempcpy(buffer_start(to_con->send), GNUTELLA_STRING " " STATUS_501 "\r\n\r\n");
						to_con->send.pos += str_size(GNUTELLA_STRING " " STATUS_501 "\r\n\r\n");
					}
					to_con->flags.dismissed = true;
					return true;

				}
			}
			else
			{
				if(str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n") < buffer_remaining(to_con->send))
				{
					my_mempcpy(buffer_start(to_con->send), GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
					to_con->send.pos += str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
				}
				to_con->flags.dismissed = true;
				return true;
			}
	// and the remote_ip field?
		case CHECK_ADDR:
			// do we have one and there was a valid address inside?
			if(to_con->flags.addr_ok) to_con->connect_state++;
			else
			{
				if(str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n") < buffer_remaining(to_con->send))
				{
					my_mempcpy(buffer_start(to_con->send), GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
					to_con->send.pos += str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
				}
				to_con->flags.dismissed = true;
				return true;
			}
	// what's the accepted encoding?
		case CHECK_ENC_OUT:
			// if nothing was send, default to no encoding (Shareaza tries to
			// save space in the header, so no failure if absent)
			if(!to_con->flags.enc_out_ok)
				to_con->encoding_out = ENC_NONE;
			else
			{
				// reset set encoding to none, if we could not agree about it
				if(to_con->encoding_out != server.settings.default_out_encoding)
					to_con->encoding_out = ENC_NONE;
// TODO: deflateInit if the other side and we want to deflate
				if((ENC_NONE != to_con->encoding_out) && (!to_con->send_u))
				{
					to_con->send_u = calloc(1, sizeof(*to_con->send_u));
					if(!to_con->send_u)
					{
						if(str_size(GNUTELLA_STRING " " STATUS_500 "\r\n\r\n") < buffer_remaining(to_con->send))
						{
							my_mempcpy(buffer_start(to_con->send), GNUTELLA_STRING " " STATUS_500 "\r\n\r\n");
							to_con->send.pos += str_size(GNUTELLA_STRING " " STATUS_500 "\r\n\r\n");
						}
						to_con->flags.dismissed = true;
						return true;
					}
					to_con->send_u->limit = to_con->send_u->capacity = sizeof(to_con->send_u->data);
					logg_devel("Allocated send_u\n");
				}
			}
			//set our desired ingoing encoding
			to_con->encoding_in = server.settings.default_in_encoding;
			to_con->connect_state++;
	// how about the user-agent
		case CHECK_UAGENT:
			// even if it's empty, the field should be send
			if(to_con->flags.uagent_ok) to_con->connect_state++;
			else
			{
				if(str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n") < buffer_remaining(to_con->send))
				{
					my_mempcpy(buffer_start(to_con->send), GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
					to_con->send.pos += str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
				}
				to_con->flags.dismissed = true;
				return true;
			}
	// last 'must-have' field: X-UltraPeer
		case CHECK_UPEER:
			if(to_con->flags.upeer_ok) to_con->connect_state++;
			else
			{
				if(str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n") < buffer_remaining(to_con->send))
				{
					my_mempcpy(buffer_start(to_con->send), GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
					to_con->send.pos += str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
				}
				to_con->flags.dismissed = true;
				return true;
			}
		case BUILD_ANSWER:
			// it should be our first comunication, and if someone set capacity right
			// everything will be fine
			buffer_clear(to_con->send);
		/* we could calculate the needed size but the header should be small enough
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
			needed_s += 2;*/
	// 2nd. header part till first ip (listen-ip)
#define HED_2_PART_1				\
	GNUTELLA_STRING " " STATUS_200 "\r\n"	\
	UAGENT_KEY ": " OUR_UA "\r\n"		\
	LISTEN_ADR_KEY ": "

#define HED_2_PART_3				\
	"\r\n" CONTENT_KEY ": " ACCEPT_G2 "\r\n"	\
	ACCEPT_KEY ": " ACCEPT_G2 "\r\n"

			// copy start of header
// TODO: finer granularity of stepping for smaller sendbuffer?
			// could we place it all in our sendbuffer?
			if(str_size(HED_2_PART_1) < buffer_remaining(to_con->send))
			{
				my_mempcpy(buffer_start(to_con->send), HED_2_PART_1);
				to_con->send.pos += str_size(HED_2_PART_1);
			}
			else
			{
				// if not there must be something very wrong -> go home
				to_con->flags.dismissed = true;
				return false;
			}
// TODO: what's our global address?
			/*if(NULL == inet_ntop((*w_entry)->af_type,
				&(*w_entry)->remote_host.sin_addr,
				buffer_start(to_con->send),
				buffer_remaining(to_con->send)))
			{
				logg_errno(LOGF_DEBUG, "writing Listen-Ip-field");
				to_con->dismissed = true;
				return(false);
			}
			to_con->send.pos += strnlen(buffer_start(to_con->send), buffer_remaining(to_con->send));
			*/

			pr_ch = (size_t)
			snprintf(buffer_start(to_con->send), buffer_remaining(to_con->send),
				"192.168.0.2:%hu\r\n"
				REMOTE_ADR_KEY ": ",
				ntohs(server.settings.portnr_2_bind)
			);
			if(pr_ch < buffer_remaining(to_con->send))
				to_con->send.pos += pr_ch;
			else
			{
				to_con->flags.dismissed = true;
				return false;
			}

			if(NULL == inet_ntop(to_con->af_type,
				&to_con->remote_host.sin_addr,
				buffer_start(to_con->send),
				buffer_remaining(to_con->send)))
			{
				logg_errno(LOGF_DEBUG, "writing Remote-Ip-field");
				to_con->flags.dismissed = true;
				return false;
			}
			to_con->send.pos += strnlen(buffer_start(to_con->send), buffer_remaining(to_con->send));

			if(str_size(HED_2_PART_3) < buffer_remaining(to_con->send))
			{
				my_mempcpy(buffer_start(to_con->send), HED_2_PART_3);
				to_con->send.pos += str_size(HED_2_PART_3);
			}
			else
			{
				to_con->flags.dismissed = true;
				return false;
			}
	
			// if we have encoding, put in the keys
			if(ENC_NONE != to_con->encoding_out)
			{
				pr_ch = (size_t)
				snprintf(buffer_start(to_con->send), buffer_remaining(to_con->send),
					CONTENT_ENC_KEY ": %s\r\n",
					KNOWN_ENCODINGS[to_con->encoding_out]->txt
				);
				if(pr_ch < buffer_remaining(to_con->send))
					to_con->send.pos += pr_ch;
				else
				{
					to_con->flags.dismissed = true;
					return false;
				}
			}
			if(ENC_NONE != to_con->encoding_in)
			{
				pr_ch = (size_t)
				snprintf(buffer_start(to_con->send), buffer_remaining(to_con->send),
					ACCEPT_ENC_KEY ": %s\r\n",
					KNOWN_ENCODINGS[to_con->encoding_in]->txt
				);
				if(pr_ch < buffer_remaining(to_con->send))
					to_con->send.pos += pr_ch;
				else
				{
					to_con->flags.dismissed = true;
					return false;
				}
			}

			// and the rest of the header
			pr_ch = (size_t)
			snprintf(buffer_start(to_con->send), buffer_remaining(to_con->send),
				UPEER_KEY ": %s\r\n"
				UPEER_NEEDED_KEY ": %s\r\n\r\n",
				(server.status.our_server_upeer) ? G2_TRUE : G2_FALSE,
				(server.status.our_server_upeer_needed) ? G2_TRUE : G2_FALSE
			);
			if(pr_ch < buffer_remaining(to_con->send))
				to_con->send.pos += pr_ch;
			else
			{
				to_con->flags.dismissed = true;
				return false;
			}	

			logg_devel("\t------------------ Response\n");
			//buffer_flip(to_con->send);
			//logg_develd("\"%.*s\"\nlength: %i\n",
			//	buffer_remaining(to_con->send),
			//	buffer_start(to_con->send),
			//	buffer_remaining(to_con->send));
			//buffer_compact(to_con->send);
			to_con->connect_state++;
			ret_val = true;
			more_bytes_needed = true;
			break;
	// Waiting for an answer
		case HEADER_2:
			if(str_size(GNUTELLA_STRING " 200") > buffer_remaining(to_con->recv))
			{
				more_bytes_needed = true;
				break;
			}

			// did the other like it
			if(!strncmp(buffer_start(to_con->recv), GNUTELLA_STRING " 200", str_size(GNUTELLA_STRING " 200")))
				to_con->recv.pos += str_size(GNUTELLA_STRING " 200");
			else
			{
				// if not: no deal
				to_con->flags.dismissed = true;
				return false;
			}

			to_con->connect_state++;
	//wait till we have the hole 2.nd header
		case ANSWER_200:
			if(!buffer_remaining(to_con->recv))
			{
				more_bytes_needed = true;
				break;
			}
			old_pos = to_con->recv.pos;
			while(true)
			{
				// CR 0x0D  LF 0x0A
// TODO: Check for a proper CR LF CR LF Sequence
				found = ('\r' != *(buffer_start(to_con->recv)) && '\n'!= *(buffer_start(to_con->recv))) ? 0: found + 1;
				to_con->recv.pos++;
				// there was CRLF 2 times
				if(4 == found)
				{
					old_limit = to_con->recv.limit;
					to_con->recv.limit = to_con->recv.pos;
					to_con->recv.pos = old_pos;
					logg_devel("\t------------------ Initiator\n");
					//logg_develd("\"%.*s\"\n", buffer_remaining(to_con->recv), buffer_start(to_con->recv));
					//to_con.connect_header1 = linesplit.split(std_utf8.decode(recv_buffer).toString());//tmp_header);
					to_con->connect_state++;
					break;
				}
				else if(!buffer_remaining(to_con->recv)) // End of Buffer?
				{
					// Header not to long (someone DoS us?)
					if(MAX_HEADER_LENGTH > (to_con->recv.limit - old_pos))
					{
						// we need more bytes
						to_con->recv.pos = old_pos;
						more_bytes_needed = true;
						break;
					}
					else //  or go home
					{
						if(str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n") < buffer_remaining(to_con->send))
						{
							my_mempcpy(buffer_start(to_con->send), GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
							to_con->send.pos += str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
						}
						to_con->flags.dismissed = true;
						return true;
					}
				}
			}
			break;
	// again fidel around with the list
		case SEARCH_CAPS_2:
			old_pos = to_con->recv.pos;
			found = 0;
			to_con->flags.second_header = true;
			while(buffer_remaining(to_con->recv))
			{
				switch(search_state)
				{
			//search first CRLF (maybe someone sends something like 'Welcome' behind the 200) and skip it
				case 0:
					found = ('\r' != *(buffer_start(to_con->recv)) && '\n'!= *(buffer_start(to_con->recv))) ? 0: found + 1;
					// there was a CRLF
					if(2 == found)
						search_state++;
					break;
				case 1:
					old_pos = to_con->recv.pos;
					search_state++;
			//search for fieldseparator ':'
				case 2:
					if(':' == *buffer_start(to_con->recv))
					{
						size_t k;
						size_t real_distance = 0;

						// back to start of field
						distance = to_con->recv.pos - old_pos;
						to_con->recv.pos = old_pos;
						
// TODO: use isalnum & isgraph?? They are locale-dependent.
// 	hopefully none sends multibyte & non-ascii field-names.
						// filter bogus beginning (skip leading space)
						while(distance && !isalnum((int)(*buffer_start(to_con->recv))))
						{
							to_con->recv.pos++;
							distance--;
						}
						
						// lets see what we can filter to find the "real" end
						// of the field (skip trailing space)
						old_pos = to_con->recv.pos;
						while(distance && isgraph((int)(*buffer_start(to_con->recv))))
						{
							to_con->recv.pos++;
							distance--;
							real_distance++;
						}
						to_con->recv.pos = old_pos;

						// even if we may have filtered all, we are continueing,
						// we could only sync again at a "\r\n" (the field-data may contain ':').
						// Maybe this "\r\n" is the last (it must be send, see above),
						// Ok, then handshaking fails. Not my fault if someone sends such nonsense.
						for(k = 0; k < KNOWN_HEADER_FIELDS_SUM; k++)
						{
							if(real_distance != KNOWN_HEADER_FIELDS[k]->length)
								continue;
						
							if(!strncasecmp(buffer_start(to_con->recv), KNOWN_HEADER_FIELDS[k]->txt, KNOWN_HEADER_FIELDS[k]->length))
							{
								field_num = k;
								field_found = true;
								break;
							}
						}

						if(!field_found)
						{
							logg_develd("unknown field:\t\"%.*s\"\tcontent:\n",
								(int) real_distance,
								buffer_start(to_con->recv));
						}
						else
						{
							if(NULL == KNOWN_HEADER_FIELDS[field_num]->action)
							{
								logg_develd("no action field:\t\"%.*s\"\tcontent:\n",
									(int) real_distance,
									buffer_start(to_con->recv));
							}
							else
							{
								logg_develd_old("  known field:\t\"%.*s\"\tcontent:\n",
									real_distance,
									buffer_start(to_con->recv));
							}
						}

						// since we may have shortened the string due to filtering,
						// we have to use all lengths, to get behind the ':'
						to_con->recv.pos += real_distance + distance + 1;
						old_pos = to_con->recv.pos;
						search_state++;
					}
					break;
			//find end of line
				case 3:
					if('\r' == *buffer_start(to_con->recv))
					{
						search_state++;
					}
					break;
				case 4:
					if('\n' == *buffer_start(to_con->recv))
					{
						// Field-data complete
						// back to start of field-data
						distance = to_con->recv.pos - old_pos - 1;
						to_con->recv.pos = old_pos;

						// remove leading white-spaces in field-data
						while(distance && isspace((int)(*buffer_start(to_con->recv))))
						{
							to_con->recv.pos++;
							distance--;
						}

						// now call the associated action for this field
						if(distance && field_found && NULL != KNOWN_HEADER_FIELDS[field_num]->action)
						{
// TODO: Better save information gained in first header from being
// overwritten in second (Bad guys are always doing bad things)
							KNOWN_HEADER_FIELDS[field_num]->action(to_con, distance);
						}
						else
						{
							if(distance)
							{
								logg_develd("\"%.*s\"\n", (int) distance, buffer_start(to_con->recv));
							}
							else
							{
								logg_devel("Field with no data recieved!\n");
							}
						}
						search_state = 2;
						to_con->recv.pos += distance + 2;
						old_pos = to_con->recv.pos;
						field_found = false;
					}
					else
						search_state--;
					break;
				}
				to_con->recv.pos++;
			}

			to_con->connect_state++;
	// Content-Key?
		case CHECK_CONTENT:
			// do we have one?
			if(to_con->flags.content_ok)
			{
				// and is it G2?
				if(to_con->flags.content_g2)
					to_con->connect_state++;
				else
				{
					if(str_size(GNUTELLA_STRING " " STATUS_501 "\r\n\r\n") < buffer_remaining(to_con->send))
					{
						my_mempcpy(buffer_start(to_con->send), GNUTELLA_STRING " " STATUS_501 "\r\n\r\n");
						to_con->send.pos += str_size(GNUTELLA_STRING " " STATUS_501 "\r\n\r\n");
					}
					to_con->flags.dismissed = true;
					return true;

				}
			}
			else
			{
				if(str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n") < buffer_remaining(to_con->send))
				{
					my_mempcpy(buffer_start(to_con->send), GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
					to_con->send.pos += str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
				}
				to_con->flags.dismissed = true;
				return true;
			}
	// what does the other likes to send as encoding
		case CHECK_ENC_IN:
			// if nothing was send, default to no encoding (Shareaza tries to
			// save space in the header, so no failure if absent)
			if(!to_con->flags.enc_in_ok)
			{
				to_con->encoding_in = ENC_NONE;
			}
			else
			{
				// abort, if we are could not agree about it
				if(to_con->encoding_in != server.settings.default_in_encoding)
				{
					if(str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n") < buffer_remaining(to_con->send))
					{
						my_mempcpy(buffer_start(to_con->send), GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
						to_con->send.pos += str_size(GNUTELLA_STRING " " STATUS_400 "\r\n\r\n");
					}
					to_con->flags.dismissed = true;
					return true;
				}

				if((ENC_NONE != to_con->encoding_in) && (!to_con->recv_u))
				{
					to_con->recv_u = calloc(1, sizeof(*to_con->recv_u));
					if(!to_con->recv_u)
					{
						if(str_size(GNUTELLA_STRING " " STATUS_500 "\r\n\r\n") < buffer_remaining(to_con->send))
						{
							my_mempcpy(buffer_start(to_con->send), GNUTELLA_STRING " " STATUS_500 "\r\n\r\n");
							to_con->send.pos += str_size(GNUTELLA_STRING " " STATUS_500 "\r\n\r\n");
						}
						to_con->flags.dismissed = true;
						return true;
					}
					to_con->recv_u->limit = to_con->recv_u->capacity = sizeof(to_con->recv_u->data);
				}

				if(ENC_DEFLATE == to_con->encoding_in)
				{
					if(Z_OK != inflateInit(&to_con->z_decoder))
					{
						if(to_con->z_decoder.msg)
							logg_posd(LOGF_DEBUG, "%s\n", to_con->z_decoder.msg);
						if(str_size(GNUTELLA_STRING " " STATUS_500 "\r\n\r\n") < buffer_remaining(to_con->send))
						{
							my_mempcpy(buffer_start(to_con->send), GNUTELLA_STRING " " STATUS_500 "\r\n\r\n");
							to_con->send.pos += str_size(GNUTELLA_STRING " " STATUS_500 "\r\n\r\n");
						}
						to_con->flags.dismissed = true;
						return true;
					}
				}
			}
			to_con->connect_state++;
	// make everything ready for business
		case FINISH_CONNECTION:
			// reclaim data delivered directly behind the header if there is some
			// trust me, this is needed
			to_con->recv.limit = old_limit;
			to_con->connect_state = G2CONNECTED;
			logg_devel("Connect succesfull\n");
			more_bytes_needed = true;
			break;
		case G2CONNECTED:
			more_bytes_needed = true;
			break;
		}
	}

	buffer_compact(to_con->recv);

	//printf("%p pos: %u lim: %u ol: %u\n", (void*)buffer_start(to_con->recv), to_con->recv.pos, to_con->recv.limit, old_limit);
	return ret_val;
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id: G2Acceptor.c,v 1.19 2005/08/21 22:59:12 redbully Exp redbully $";

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
//EOF
