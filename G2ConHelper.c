/*
 * G2ConHelper.c
 * G2-specific network-helper functions
 *
 * Copyright (c) 2004-2010, Jan Seiffert
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
 * $Id: G2ConHelper.c,v 1.12 2005/07/29 09:24:04 redbully Exp redbully $
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
#include <sys/types.h>
#if 0 && defined(HAVE_SYS_UIO_H)
# include <sys/uio.h>
#endif
/* other */
#include "lib/other.h"
/* Own includes */
#define _G2CONHELPER_C
#include "G2ConHelper.h"
#include "G2MainServer.h"
#include "G2Connection.h"
#include "G2ConRegistry.h"
#include "lib/combo_addr.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/recv_buff.h"
#include "lib/my_epoll.h"
#include "lib/atomic.h"

g2_connection_t *handle_socket_abnorm(struct epoll_event *p_entry)
{
	g2_connection_t *w_entry = p_entry->data.ptr;
	const char *msg = NULL;

	if(p_entry->events & (uint32_t)EPOLLERR)
		msg = "error in connection!";
	if(p_entry->events & (uint32_t)EPOLLHUP)
		msg = "HUP in connection";
/*
 * grmpf... EPoll removes NVal-fd automagicly...
 * if(p_entry->events & (uint32_t)POLLNVAL)
 * 	msg = "NVal Socket-FD in poll_data!";
 */

	if(!msg)
		msg = "unknown problem";

	logg_posd(LOGF_DEVEL_OLD, "%s Ip: %p#I\tFDNum: %i\n",
	          msg, &w_entry->remote_host, w_entry->com_socket);
/*
 * Under Linux get errno out off ERRQUEUE and print/log it
 *			#define ERR_BUFF_SIZE 4096
 *			char err_buff[ERR_BUFF_SIZE];
 *			ssize_t ret_val = recv(poll_data[i].fd,
 *			&err_buff[0],
 *			sizeof(err_buff),
 *			MSG_ERRQUEUE);
 */
	return w_entry;
}

bool do_read(struct epoll_event *p_entry, int epoll_fd)
{
	g2_connection_t *w_entry = (g2_connection_t *)p_entry->data.ptr;
	ssize_t result = 0;
	bool ret_val = true;
#ifdef DEBUG_DEVEL
	if(!w_entry->recv) {
		logg_posd(LOGF_DEBUG, "%s Ip: %p#I\tFDNum: %i\n",
		          "no recv buffer!", &w_entry->remote_host, w_entry->com_socket);
		w_entry->flags.dismissed = true;
		return false;
	}
#endif

	do	{
		set_s_errno(0);
		result = my_epoll_recv(epoll_fd, w_entry->com_socket, buffer_start(*w_entry->recv),
		              buffer_remaining(*w_entry->recv), 0);
	} while(-1 == result && EINTR == s_errno);

	switch(result)
	{
	default:
		w_entry->recv->pos += result;
		break;
	case  0:
		if(buffer_remaining(*w_entry->recv))
		{
			if(EAGAIN != s_errno) {
				logg_posd(LOGF_DEVEL_OLD, "%s ERRNO=%i Ip: %p#I\tFDNum: %i\n",
				          "EOF reached!", s_errno, &w_entry->remote_host, w_entry->com_socket);
				w_entry->flags.dismissed = true;
				ret_val = false;
			} else
				logg_devel("Nothing to read!\n");
		}
		break;
	case -1:
		if(!(EAGAIN == s_errno || EWOULDBLOCK == s_errno)) {
			logg_serrno(LOGF_DEBUG, "write");
			w_entry->flags.dismissed = true;
			ret_val = false;
		}
		break;
	}
	logg_develd_old("ret_val: %i\tpos: %u\tlim: %u\n", ret_val, w_entry->recv->pos, w_entry->recv->limit);
	
	return ret_val;
}

#if 0
static size_t iovec_len(const struct iovec vec[], size_t cnt)
{
	size_t i, len;
	for(i = 0, len = 0; i < cnt; i++)
		len += vec[i].iov_len;
	return len;
}

ssize_t do_writev(struct epoll_event *p_entry, int epoll_fd, const struct iovec vec[], size_t cnt, bool more)
{
	g2_connection_t *w_entry = (g2_connection_t *)p_entry->data.ptr;
	ssize_t result = 0;

	if(!cnt)
		return 0;

	do	{
		set_s_errno(0);
		result = writev(w_entry->com_socket, vec, cnt);
	} while(-1 == result && EINTR == s_errno);

	switch(result)
	{
	default:
		if(!more && iovec_len(vec, cnt) <= (size_t)result)
		{
			shortlock_t_lock(&w_entry->pts_lock);
			w_entry->poll_interrests &= ~((uint32_t)EPOLLOUT);
			if(!(w_entry->poll_interrests & EPOLLONESHOT))
			{
				struct epoll_event t_entry;
				t_entry.data.u64 = p_entry->data.u64;
				t_entry.events = w_entry->poll_interrests;
				shortlock_t_unlock(&w_entry->pts_lock);

				if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, w_entry->com_socket, &t_entry)) {
					logg_serrno(LOGF_DEBUG, "changing sockets Epoll-interrests");
					w_entry->flags.dismissed = true;
					result = -1;
				}
			}
			else
			{
				shortlock_t_unlock(&w_entry->pts_lock);

				if(w_entry->flags.dismissed) {
					logg_posd(LOGF_DEVEL_OLD, "%s Ip: %p#I\tFDNum: %i\n",
					          "Dismissed!", &w_entry->remote_host, w_entry->com_socket);
					result = -1;
				}
			}
		}
		break;
	case  0:
		if(iovec_len(vec, cnt))
		{
			if(EAGAIN != s_errno) {
				logg_posd(LOGF_DEVEL_OLD, "%s Ip: %p#I\tFDNum: %i\n",
				          "Dismissed!", &w_entry->remote_host, w_entry->com_socket);
				w_entry->flags.dismissed = true;
				result = -1;
			} else
				logg_devel("not ready to write\n");
		}
		else if(!more)
		{
			shortlock_t_lock(&w_entry->pts_lock);
			w_entry->poll_interrests &= ~((uint32_t)EPOLLOUT);
			if(!(w_entry->poll_interrests & EPOLLONESHOT))
			{
				struct epoll_event t_entry;
				t_entry.data.u64 = p_entry->data.u64;
				t_entry.events = w_entry->poll_interrests;
				shortlock_t_unlock(&w_entry->pts_lock);

				if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, w_entry->com_socket, &t_entry)) {
					logg_serrno(LOGF_DEBUG, "changing sockets Epoll-interrests");
					w_entry->flags.dismissed = true;
					result = -1;
				}
			}
			else
			{
				shortlock_t_unlock(&w_entry->pts_lock);

				if(w_entry->flags.dismissed) {
					logg_posd(LOGF_DEBUG, "%s ERRNO=%i Ip: %p#I\tFDNum: %i\n",
					          "EOF reached!", s_errno, &w_entry->remote_host, w_entry->com_socket);
					result = -1;
				}
			}
		}
		break;
	case -1:
		if(EAGAIN != s_errno)
			logg_serrno(LOGF_DEBUG, "write");
		break;
	}
	return result;
}
#endif

bool do_write(struct epoll_event *p_entry, int epoll_fd)
{
	g2_connection_t *w_entry = (g2_connection_t *)p_entry->data.ptr;
	bool ret_val = true, more_write;
	ssize_t result = 0;
#ifdef DEBUG_DEVEL
	if(!w_entry->send) {
		logg_posd(LOGF_DEBUG, "%s Ip: %p#I\tFDNum: %i\n",
		          "no send buffer!", &w_entry->remote_host, w_entry->com_socket);
		w_entry->flags.dismissed = true;
		ret_val = false;
	}
#endif

	buffer_flip(*w_entry->send);
	do	{
		set_s_errno(0);
		result = my_epoll_send(epoll_fd, w_entry->com_socket, w_entry->send->data, w_entry->send->limit, 0);
	} while(-1 == result && EINTR == s_errno);

	switch(result)
	{
	default:
		w_entry->send->pos += result;
		w_entry->flags.has_written = true;
		w_entry->last_send = local_time_now;
		if(buffer_remaining(*w_entry->send))
			break;

		shortlock_t_lock(&w_entry->pts_lock);
		if(list_empty(&w_entry->packets_to_send)) {
			w_entry->poll_interrests &= ~((uint32_t)EPOLLOUT);
			more_write = true;
		} else
			more_write = false;

		if(more_write)
		{
			if(!(w_entry->poll_interrests & EPOLLONESHOT))
			{
				struct epoll_event t_entry;
				t_entry.data.u64 = p_entry->data.u64;
				t_entry.events = w_entry->poll_interrests;
				shortlock_t_unlock(&w_entry->pts_lock);

				if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, w_entry->com_socket, &t_entry)) {
					logg_serrno(LOGF_DEBUG, "changing sockets Epoll-interrests");
					w_entry->flags.dismissed = true;
					ret_val = false;
				}
			}
			else
			{
				shortlock_t_unlock(&w_entry->pts_lock);

				if(w_entry->flags.dismissed) {
					logg_posd(LOGF_DEVEL_OLD, "%s\tIP: %p#I\tFDNum: %i\n",
					          "Dismissed!", &w_entry->remote_host, w_entry->com_socket);
					ret_val = false;
				}
			}
		}
		else
			shortlock_t_unlock(&w_entry->pts_lock);
		break;
	case  0:
		if(buffer_remaining(*w_entry->send))
		{
			if(EAGAIN != s_errno) {
				logg_posd(LOGF_DEVEL_OLD, "%s Ip: %p#I\tFDNum: %i\n",
				          "Dismissed!", &w_entry->remote_host, w_entry->com_socket);
				w_entry->flags.dismissed = true;
				ret_val = false;
			} else
				logg_devel("not ready to write\n");
		}
		else
		{
			shortlock_t_lock(&w_entry->pts_lock);
			w_entry->poll_interrests &= ~((uint32_t)EPOLLOUT);
			if(!(w_entry->poll_interrests & EPOLLONESHOT))
			{
				struct epoll_event t_entry;
				t_entry.data.u64 = p_entry->data.u64;
				t_entry.events = w_entry->poll_interrests;
				shortlock_t_unlock(&w_entry->pts_lock);

				if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, w_entry->com_socket, &t_entry)) {
					logg_serrno(LOGF_DEBUG, "changing sockets Epoll-interrests");
					w_entry->flags.dismissed = true;
					ret_val = false;
				}
			}
			else
			{
				shortlock_t_unlock(&w_entry->pts_lock);

				if(w_entry->flags.dismissed) {
					logg_posd(LOGF_DEVEL_OLD, "%s ERRNO=%i Ip: %p#I\tFDNum: %i\n",
					          "EOF reached!", s_errno, &w_entry->remote_host, w_entry->com_socket);
					ret_val = false;
				}
			}
		}
		break;
	case -1:
		if(!(EAGAIN == errno || EWOULDBLOCK == s_errno)) {
			logg_serrno(LOGF_DEBUG, "write");
			ret_val = false;
		}
		break;
	}
	logg_develd_old("ret_val: %i\tpos: %u\tlim: %u\n", ret_val, w_entry->send->pos, w_entry->send->limit);
	buffer_compact(*w_entry->send);
	logg_develd_old("ret_val: %i\tpos: %u\tlim: %u\n", ret_val, w_entry->send->pos, w_entry->send->limit);
	return ret_val;
}

bool recycle_con(g2_connection_t *w_entry, int epoll_fd, int keep_it)
{
	struct epoll_event tmp_eevent;

	if(!keep_it)
	{
		/*
		 * make sure the timeouts can not fire again,
		 * so they do not undo the epoll_ctl
		 */
		timeout_cancel(&w_entry->active_to);
		timeout_cancel(&w_entry->aux_to);
	}

	tmp_eevent.events = 0;
	tmp_eevent.data.u64 = 0;
	/* remove from EPoll */
	if(my_epoll_ctl(epoll_fd, EPOLL_CTL_DEL, w_entry->com_socket, &tmp_eevent)) {
		if(ENOENT != s_errno)
			logg_serrno(LOGF_ERR, "removing bad socket from EPoll");
	}

	if(!keep_it)
	{
		int ret_val;
		/* free the fd */
		do {
			ret_val = closesocket(w_entry->com_socket);
		} while(ret_val && EINTR == s_errno);
		if(ret_val)
			logg_serrno(LOGF_DEBUG, "closing bad socket");
		w_entry->com_socket = -1;

		/* free associated rescources and remove from conreg */
		g2_con_clear(w_entry);
		atomic_dec(&server.status.act_connection_sum);

		/* return datastructure to FreeCons */
		g2_con_ret_free(w_entry);
	}
	else
		pthread_mutex_unlock(&w_entry->lock);

	logg_develd_old("%s\n", (keep_it) ? "connection removed" : "connection recyled");

	return true;
}

void teardown_con(g2_connection_t *w_entry, int epoll_fd)
{
	struct epoll_event tmp_eevent;
	int ret_val;

	/* remove from conreg */
	g2_conreg_remove(w_entry);

	/*
	 * make sure the timeouts can not fire again,
	 * so they do not undo the epoll_ctl
	 */
	/*
	 * we would need to cancel both timeouts, but we may hold the timeout lock
	 * we simply assume that it is not the aux_to
	 */
	timeout_cancel(&w_entry->aux_to);

	tmp_eevent.events = 0;
	tmp_eevent.data.u64 = 0;
	/* remove from EPoll */
	if(my_epoll_ctl(epoll_fd, EPOLL_CTL_DEL, w_entry->com_socket, &tmp_eevent)) {
		if(ENOENT != s_errno)
			logg_serrno(LOGF_ERR, "removing bad socket from EPoll");
	}

	/* free the fd */
	do {
		ret_val = closesocket(w_entry->com_socket);
	} while(ret_val && EINTR == s_errno);
	if(ret_val)
		logg_serrno(LOGF_DEBUG, "closing bad socket");
	w_entry->com_socket = -1;

	/* after the fs is free we can take another one */
	atomic_dec(&server.status.act_connection_sum);

	/* to avoid deadlocks with timer, defer final free */
	hzp_deferfree(&w_entry->hzp, w_entry, (void (*)(void *))g2_con_free_glob);
}

bool manage_buffer_before(struct norm_buff **con_buff, struct norm_buff **our_buff)
{
	if(unlikely(*con_buff))
		return true;

	logg_devel_old("no buffer\n");
	if(!*our_buff)
	{
		logg_devel_old("allocating\n");
		*our_buff = recv_buff_local_get();
		if(!*our_buff) {
			logg_pos(LOGF_WARN, "allocating recv buff failed\n");
			return false;
		}
	}
	*con_buff = *our_buff;
	return true;
}

void manage_buffer_after(struct norm_buff **con_buff, struct norm_buff **our_buff)
{
	if(unlikely(!*con_buff))
		return;
	if(likely(*con_buff == *our_buff))
	{
		logg_devel_old("local buffer\n");
		if(likely(buffer_cempty(**con_buff))) { /* buffer is empty */
			logg_devel_old("empty\n");
			*con_buff = NULL;
			buffer_clear(**our_buff);
		}
		else
			*our_buff = NULL;
	}
	else
	{
		if(buffer_cempty(**con_buff)) {
			logg_devel_old("foreign buffer empty\n");
			recv_buff_local_ret(*con_buff);
			*con_buff = NULL;
		}
	}
}

static char const rcsid_ch[] GCC_ATTR_USED_VAR = "$Id: G2ConHelper.c,v 1.12 2005/07/29 09:24:04 redbully Exp redbully $";
/* EOF */
