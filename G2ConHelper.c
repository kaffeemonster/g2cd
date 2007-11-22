/*
 * G2ConHelper.c
 * G2-specific network-helper functions
 *
 * Copyright (c) 2004, Jan Seiffert
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
 * $Id: G2ConHelper.c,v 1.12 2005/07/29 09:24:04 redbully Exp redbully $
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
// System net-includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
// other
#include "other.h"
// Own includes
#define _G2CONHELPER_C
#include "G2ConHelper.h"
#include "G2MainServer.h"
#include "G2Connection.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/recv_buff.h"
#include "lib/my_epoll.h"
#include "lib/atomic.h"

inline g2_connection_t **handle_socket_abnorm(struct epoll_event *p_entry)
{
	g2_connection_t **w_entry = (g2_connection_t **)p_entry->data.ptr;
	const char *msg = NULL;

	if(p_entry->events & (uint32_t)EPOLLERR)
		msg = "error in connection!";
	if(p_entry->events & (uint32_t)EPOLLHUP)
		msg = "HUP in connection";
// grmpf... EPoll removes NVal-fd automagicly...
//	if(p_entry->events & (uint32_t)POLLNVAL)
//		msg = "NVal Socket-FD in poll_data!";

	if(!msg)
		msg = "unknown problem";

	{
		char addr_buf[INET6_ADDRSTRLEN];
		logg_posd(LOGF_DEBUG, "%s Ip: %s\tPort: %hu\tFDNum: %i\n",
			msg,
			inet_ntop((*w_entry)->af_type, &(*w_entry)->remote_host.sin_addr, addr_buf, sizeof(addr_buf)),
			//inet_ntoa((*w_entry)->remote_host.sin_addr),
			ntohs((*w_entry)->remote_host.sin_port),
			(*w_entry)->com_socket);
/*
 * Under Linux get errno out off ERRQUEUE and print/log it
 *			#define ERR_BUFF_SIZE 4096
 *			char err_buff[ERR_BUFF_SIZE];
 *			ssize_t ret_val = recv(poll_data[i].fd,
 *			&err_buff[0],
 *			sizeof(err_buff),
 *			MSG_ERRQUEUE);
 */
 	}
	return w_entry;
}

inline bool do_read(struct epoll_event *p_entry)
{
	g2_connection_t *w_entry = *((g2_connection_t **)p_entry->data.ptr);
	ssize_t result = 0;
	bool ret_val = true;
#ifdef DEBUG_LEVEL
	if(!w_entry->recv)
	{
		char addr_buf[INET6_ADDRSTRLEN];
		logg_posd(LOGF_DEBUG, "%s Ip: %s\tPort: %hu\tFDNum: %i\n",
			"no recv buffer!",
			inet_ntop(w_entry->af_type, &w_entry->remote_host.sin_addr, addr_buf, sizeof(addr_buf)),
			//inet_ntoa(w_entry->remote_host.sin_addr),
			ntohs(w_entry->remote_host.sin_port),
			w_entry->com_socket);
		w_entry->flags.dismissed = true;
		return false;
	}
#endif

	do
	{
		errno = 0;
		result = recv(w_entry->com_socket, buffer_start(*w_entry->recv), buffer_remaining(*w_entry->recv), 0);
	} while(-1 == result && EINTR == errno);
	//(w_entry->recv.data + w_entry->recv.pos),

	switch(result)
	{
	default:
		w_entry->recv->pos += result;
		break;
	case  0:
		if(buffer_remaining(*w_entry->recv))
		{
			if(EAGAIN != errno)
			{
				char addr_buf[INET6_ADDRSTRLEN];
				logg_posd(LOGF_DEBUG, "%s ERRNO=%i Ip: %s\tPort: %hu\tFDNum: %i\n",
					"EOF reached!", errno,
					inet_ntop(w_entry->af_type, &w_entry->remote_host.sin_addr, addr_buf, sizeof(addr_buf)),
					//inet_ntoa(w_entry->remote_host.sin_addr),
					ntohs(w_entry->remote_host.sin_port),
					w_entry->com_socket);
				w_entry->flags.dismissed = true;
				ret_val = false;
			}
			else
				logg_devel("Nothing to read!\n");
		}
		break;
	case -1:
		if(EAGAIN != errno)
		{
			logg_errno(LOGF_DEBUG, "write");
			w_entry->flags.dismissed = true;
			ret_val = false;
		}
		break;
	}
	logg_develd_old("ret_val: %i\tpos: %u\tlim: %u\n", ret_val, w_entry->recv->pos, w_entry->recv->limit);
	
	return ret_val;
}

inline bool do_write(struct epoll_event *p_entry, int epoll_fd)
{
	g2_connection_t *w_entry = *((g2_connection_t **)p_entry->data.ptr);
	bool ret_val = true;
	ssize_t result = 0;
#ifdef DEBUG_LEVEL
	if(!w_entry->send)
	{
		char addr_buf[INET6_ADDRSTRLEN];
		logg_posd(LOGF_DEBUG, "%s Ip: %s\tPort: %hu\tFDNum: %i\n",
			"no send buffer!",
			inet_ntop(w_entry->af_type, &w_entry->remote_host.sin_addr, addr_buf, sizeof(addr_buf)),
			//inet_ntoa(w_entry->remote_host.sin_addr),
			ntohs(w_entry->remote_host.sin_port),
			w_entry->com_socket);
		w_entry->flags.dismmised = true;
		ret_val = false;
	}
#endif

	buffer_flip(*w_entry->send);
	do
	{
		errno = 0;
		result = send(w_entry->com_socket, w_entry->send->data, w_entry->send->limit, 0);
	} while(-1 == result && EINTR == errno);

	switch(result)
	{
	default:
		w_entry->send->pos += result;
		//p_entry->events |= POLLIN;
		if(!buffer_remaining(*w_entry->send))
		{
			p_entry->events = w_entry->poll_interrests &= ~((uint32_t)EPOLLOUT);
			if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, w_entry->com_socket, p_entry))
			{
				logg_errno(LOGF_DEBUG, "changing sockets Epoll-interrests");
				w_entry->flags.dismissed = true;
				ret_val = false;
			}
			else if(w_entry->flags.dismissed)
			{
				char addr_buf[INET6_ADDRSTRLEN];
				logg_posd(LOGF_DEBUG, "%s Ip: %s\tPort: %hu\tFDNum: %i\n",
					"Dismissed!",
					inet_ntop(w_entry->af_type, &w_entry->remote_host.sin_addr, addr_buf, sizeof(addr_buf)),
					//inet_ntoa(w_entry->remote_host.sin_addr),
					ntohs(w_entry->remote_host.sin_port),
					w_entry->com_socket);
				ret_val = false;
			}
		}
		break;
	case  0:
		if(buffer_remaining(*w_entry->send))
		{
			if(EAGAIN != errno)
			{
				char addr_buf[INET6_ADDRSTRLEN];
				logg_posd(LOGF_DEBUG, "%s Ip: %s\tPort: %hu\tFDNum: %i\n",
					"Dismissed!",
					inet_ntop(w_entry->af_type, &w_entry->remote_host.sin_addr, addr_buf, sizeof(addr_buf)),
					//inet_ntoa(w_entry->remote_host.sin_addr),
					ntohs(w_entry->remote_host.sin_port),
					w_entry->com_socket);
				w_entry->flags.dismissed = true;
				ret_val = false;
			}
			else
				logg_devel("not ready to write\n");
		}
		else
		{
			//putchar('-');
			p_entry->events = w_entry->poll_interrests &= ~((uint32_t)EPOLLOUT);
			if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, w_entry->com_socket, p_entry))
			{
				logg_errno(LOGF_DEBUG, "changing sockets Epoll-interrests");
				w_entry->flags.dismissed = true;
				ret_val = false;
			}
			else if(w_entry->flags.dismissed)
			{
				char addr_buf[INET6_ADDRSTRLEN];
				logg_posd(LOGF_DEBUG, "%s ERRNO=%i Ip: %s\tPort: %hu\tFDNum: %i\n",
					"EOF reached!", errno,
					inet_ntop(w_entry->af_type, &w_entry->remote_host.sin_addr, addr_buf, sizeof(addr_buf)),
					//inet_ntoa(w_entry->remote_host.sin_addr),
					ntohs(w_entry->remote_host.sin_port),
					w_entry->com_socket);
				ret_val = false;
			}
		}
		break;
	case -1:
		if(EAGAIN != errno)
		{
			logg_errno(LOGF_DEBUG, "write");
			ret_val = false;
		}
		break; 
	}
	logg_develd_old("ret_val: %i\tpos: %u\tlim: %u\n", ret_val, work_entry->send->pos, work_entry->send->limit);
	buffer_compact(*w_entry->send);
	logg_develd_old("ret_val: %i\tpos: %u\tlim: %u\n", ret_val, work_entry->send->pos, work_entry->send->limit);
	return ret_val;
}

inline bool recycle_con(
	g2_connection_t **w_entry,
	struct g2_con_info *work_cons,
	int epoll_fd,
	int keep_it
	)
{
	struct epoll_event tmp_eevent;
	g2_connection_t **w_last = &work_cons->data[work_cons->limit-1];
	g2_connection_t *tmp_con = *w_entry;

	// remove from WorkCons
	if(w_entry < w_last)
		memmove(w_entry, (w_entry + 1), (w_last - w_entry) * sizeof(*w_entry));

	*w_last = NULL;
	work_cons->limit--;

	tmp_eevent.events = 0;
	tmp_eevent.data.u64 = 0;
	// remove from EPoll
	if(my_epoll_ctl(epoll_fd, EPOLL_CTL_DEL, tmp_con->com_socket, &tmp_eevent))
		logg_errno(LOGF_ERR, "removing bad socket from EPoll");

	if(!keep_it)
	{
		int ret_val;
		// free the fd
		do
		{
			ret_val = close(tmp_con->com_socket);
		} while(ret_val && EINTR == errno);
		if(ret_val)
			logg_errno(LOGF_DEBUG, "closing bad socket");

		atomic_dec(&server.status.act_connection_sum);

		// return datastructure to FreeCons
		g2_con_clear(tmp_con);
		g2_con_ret_free(tmp_con);
	}

	logg_develd("%s\n", (keep_it) ? "connection removed" : "connection recyled");

	return true;
}

inline bool manage_buffer_before(struct norm_buff **con_buff, struct norm_buff **our_buff)
{
	if(!*con_buff)
	{
		logg_devel_old("no buffer\n");
		if(!*our_buff)
		{
			logg_devel_old("allocating\n");
			*our_buff = recv_buff_alloc();
			if(!*our_buff)
			{
				logg_pos(LOGF_WARN, "allocating recv buff failed\n");
				return false;
			}
		}
		*con_buff = *our_buff;
	}
	return true;
}

inline void manage_buffer_after(struct norm_buff **con_buff, struct norm_buff **our_buff)
{
	if(*con_buff == *our_buff)
	{
		logg_devel_old("local buffer\n");
		if(buffer_cempty(**con_buff)) /* buffer is empty */
		{
			logg_devel_old("empty\n");
			*con_buff = NULL;
			buffer_clear(**our_buff);
		}
		else
			*our_buff = NULL;
	}
	else
	{
		if(buffer_cempty(**con_buff))
		{
			logg_devel_old("foreign buffer empty\n");
			recv_buff_free(*con_buff);
			*con_buff = NULL;
		}
	}
}

static char const rcsid_ch[] GCC_ATTR_USED_VAR = "$Id: G2ConHelper.c,v 1.12 2005/07/29 09:24:04 redbully Exp redbully $";
// EOF
