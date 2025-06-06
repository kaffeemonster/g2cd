/*
 * G2Handler.c
 * code to handle G2-Protocol
 *
 * Copyright (c) 2004-2019 Jan Seiffert
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
/* System net-includes */
#include <sys/types.h>
#include "lib/my_epoll.h"
#include "lib/combo_addr.h"
#include <fcntl.h>
/* other */
#include "lib/other.h"
/* Own includes */
#define _G2HANDLER_C
#define _NEED_G2_P_TYPE
#include "G2Packet.h"
#include "G2Handler.h"
#include "gup.h"
#include "G2MainServer.h"
#include "G2Connection.h"
#include "G2ConHelper.h"
#include "G2ConRegistry.h"
#include "G2PacketSerializer.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/recv_buff.h"
#include "lib/my_epoll.h"
#include "lib/hzp.h"

/* internal prototypes */
static inline g2_connection_t *handle_socket_io_h(struct epoll_event *, int epoll_fd, struct norm_buff **, struct norm_buff **);

void handle_con(struct epoll_event *e_wptr, struct norm_buff *lbuff[MULTI_RECV_NUM], int epoll_fd)
{
	g2_connection_t *w_entry = e_wptr->data.ptr;
	int lock_res;

	if((lock_res = mutex_trylock(&w_entry->lock))) {
		/* somethings wrong */
		if(EBUSY == lock_res)
			return; /* if already locked, do nothing */
// TODO: what to do on locking error?
		return;
	}

	logg_develd_old("-------- Events: 0x%0X, PTR: %p\n", e_wptr->events, e_wptr->data.ptr);
	/*
	 * Any problems? 'Where did you go my lovely...'
	 * Any invalid FD's remained in PollData?
	 */
	if(e_wptr->events & ~((uint32_t)(EPOLLIN|EPOLLOUT|EPOLLONESHOT)))
	{
		g2_connection_t *tmp_con_holder = handle_socket_abnorm(e_wptr);
		/* take the poor dear behind the shed... */
		if(tmp_con_holder)
			recycle_con(tmp_con_holder, epoll_fd, false);
		else { /* this should not happen... */
			logg_pos(LOGF_ERR, "Somethings wrong with our polled FD's, couldn't solve it\n");
			mutex_unlock(&w_entry->lock);
		}
		return;
	}

	/* Some FD's ready to be filled? Some data ready to be read in? */
	g2_connection_t *tmp_con_holder = handle_socket_io_h(e_wptr, epoll_fd, &lbuff[0], &lbuff[1]);
	if(tmp_con_holder)
	{
		if(lbuff[0] && tmp_con_holder->recv == lbuff[0]) {
			tmp_con_holder->recv = NULL;
			buffer_clear(*lbuff[0]);
		}
		if(lbuff[1] && tmp_con_holder->send == lbuff[1]) {
			tmp_con_holder->send = NULL;
			buffer_clear(*lbuff[1]);
		}
		recycle_con(tmp_con_holder, epoll_fd, false);
	}
}

static g2_connection_t *handle_socket_io_h(struct epoll_event *p_entry, int epoll_fd, struct norm_buff **lrecv_buff, struct norm_buff **lsend_buff)
{
	g2_connection_t *w_entry = p_entry->data.ptr;

	/* get buffer */
	if(!manage_buffer_before(&w_entry->recv, lrecv_buff)) {
		w_entry->flags.dismissed = true;
		return w_entry;
	}
	if(!manage_buffer_before(&w_entry->send, lsend_buff)) {
		manage_buffer_after(&w_entry->recv, lrecv_buff);
		w_entry->flags.dismissed = true;
		return w_entry;
	}

	/* write */
	w_entry->flags.has_written = false;
	if(p_entry->events & (uint32_t)EPOLLOUT)
	{
		struct norm_buff *d_target = NULL;
		bool compact_ubuff = false, old_flush = false, genuie_buf_full = false;

		if(ENC_DEFLATE == w_entry->encoding_out)
			d_target = w_entry->send_u;
		else
			d_target = w_entry->send;

		if(buffer_remaining(*d_target) < 10)
			goto no_pfill_before_write;

		/*
		 * MB:
		 * This spinlock protects the list, but also
		 * barriers the packet creation from our reading
		 */
		shortlock_lock(&w_entry->pts_lock);
		if(!list_empty(&w_entry->packets_to_send))
		{
			struct list_head head;
			struct list_head *e, *n;

more_packet_encode:
			INIT_LIST_HEAD(&head);
			list_splice_init(&w_entry->packets_to_send, &head);
			shortlock_unlock(&w_entry->pts_lock);

			list_for_each_safe(e, n, &head)
			{
				g2_packet_t *entry = list_entry(e, g2_packet_t, list);
				if(!g2_packet_serialize_to_buff(entry, d_target))
				{
					logg_posd(LOGF_DEBUG, "%s Ip: %p#I\tFDNum: %i\n",
					          "failed to encode packet-stream", &w_entry->remote_host,
					          w_entry->com_socket);
					w_entry->flags.dismissed = true;
					break;
				}
				if(ENCODE_FINISHED != entry->packet_encode)
					break;
				logg_develd_old("removing one: %s\n", g2_ptype_names[entry->type]);
				list_del(e);
				g2_packet_free(entry);
			}
			shortlock_lock(&w_entry->pts_lock);
			if(!list_empty(&head))
				list_splice(&head, &w_entry->packets_to_send);
			else if(!list_empty(&w_entry->packets_to_send))
				goto more_packet_encode;
			shortlock_unlock(&w_entry->pts_lock);
			if(w_entry->flags.dismissed)
				return w_entry;
		}
		else
			shortlock_unlock(&w_entry->pts_lock);

no_pfill_before_write:
		if(ENC_DEFLATE == w_entry->encoding_out)
		{
			if(buffer_remaining(*w_entry->send) > 6)
			{
				buffer_flip(*w_entry->send_u);
retry_pack:
				logg_develd_old("++++ cbytes:\t%zu\n", buffer_remaining(*w_entry->send));
				logg_develd_old("**** space:\t%zu\n", buffer_remaining(*w_entry->send_u));
				w_entry->z_encoder->next_in = (Bytef *)buffer_start(*w_entry->send_u);
				w_entry->z_encoder->avail_in = buffer_remaining(*w_entry->send_u);
				w_entry->z_encoder->next_out = (Bytef *)buffer_start(*w_entry->send);
				w_entry->z_encoder->avail_out = buffer_remaining(*w_entry->send);

				old_flush = w_entry->u.handler.z_flush;
				w_entry->u.handler.z_flush = false;
				mb();
				logg_develd_old("z_flush:\t%c\n", old_flush ? 't' : 'f');
				switch(deflate(w_entry->z_encoder, old_flush ?  Z_SYNC_FLUSH : Z_NO_FLUSH))
				{
				case Z_BUF_ERROR:
					logg_devel_old("Z_BUF_ERROR\n");
					GCC_FALL_THROUGH
				case Z_OK:
					w_entry->send_u->pos += (buffer_remaining(*w_entry->send_u) - w_entry->z_encoder->avail_in);
					w_entry->send->pos += (buffer_remaining(*w_entry->send) - w_entry->z_encoder->avail_out);
					if(0 == w_entry->z_encoder->avail_out) {
						genuie_buf_full = true;
						 /* if we filled the buffer, make sure we reuse the same flush setting */
						w_entry->u.handler.z_flush = old_flush;
					}
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
				default:
					logg_devel("deflate was not Z_OK\n");
					return w_entry;
				}
				compact_ubuff = true;
			}
			logg_develd_old("++++ cpos:\t%zu\n", w_entry->send->pos);
			logg_develd_old("**** space:\t%zu\n", buffer_remaining(*w_entry->send_u));
		}

		if(!do_write(p_entry, epoll_fd))
			return w_entry;

		if(ENC_NONE != w_entry->encoding_out)
		{
// TODO: loop till zlib says more data
			logg_develd_old("++++ cbytes:\t%zu\n", buffer_remaining(*w_entry->send));
			if(buffer_remaining(*w_entry->send) > 6) {
				if(compact_ubuff && buffer_remaining(*w_entry->send_u))
					goto retry_pack;
				if(!compact_ubuff && !buffer_cempty(*w_entry->send_u))
					goto no_pfill_before_write;
			}
			if(compact_ubuff)
				buffer_compact(*w_entry->send_u);
			if(buffer_remaining(*w_entry->send_u) > 10)
			{
				shortlock_lock(&w_entry->pts_lock);
				if(!list_empty(&w_entry->packets_to_send))
						goto more_packet_encode;
				shortlock_unlock(&w_entry->pts_lock);
			}
			/*
			 * when the o-buffer is full (avail_out == 0) we have to
			 * re-call deflate with the same flush value
			 */
			if(!old_flush) {
				w_entry->aux_to.data = w_entry;
				w_entry->aux_to.fun  = handler_z_flush_timeout;
				timeout_add(&w_entry->aux_to, 20);
			} else if(!genuie_buf_full)
				w_entry->u.handler.z_flush = false;
		}
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
		bool retry, compact_cbuff = false, save_build_packet = false;

		w_entry->flags.last_data_active = false;
		if(!do_read(p_entry, epoll_fd))
			return w_entry;
		if(buffer_cempty(*w_entry->recv))
			goto nothing_to_read;

		if(ENC_NONE == w_entry->encoding_in)
			d_source = w_entry->recv;
		else if(ENC_DEFLATE == w_entry->encoding_in)
		{
			buffer_flip(*w_entry->recv);
retry_unpack:
			logg_devel_old("--------- compressed\n");
			d_source = w_entry->recv_u;
			if(buffer_remaining(*w_entry->recv))
			{
				w_entry->z_decoder->next_in = (Bytef *)buffer_start(*w_entry->recv);
				w_entry->z_decoder->avail_in = buffer_remaining(*w_entry->recv);
				w_entry->z_decoder->next_out = (Bytef *)buffer_start(*d_source);
				w_entry->z_decoder->avail_out = buffer_remaining(*d_source);

				logg_develd_old("++++ cbytes: %u\n", buffer_remaining(*w_entry->recv));
				logg_develd_old("**** space: %u\n", buffer_remaining(*d_source));

				switch(inflate(w_entry->z_decoder, Z_SYNC_FLUSH))
				{
				case Z_BUF_ERROR:
					logg_devel("Z_BUF_ERROR\n");
					GCC_FALL_THROUGH
				case Z_OK:
					w_entry->recv->pos += (buffer_remaining(*w_entry->recv) - w_entry->z_decoder->avail_in);
					d_source->pos += (buffer_remaining(*d_source) - w_entry->z_decoder->avail_out);
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
				default:
					logg_devel("inflate was not Z_OK\n");
					return w_entry;
				}
			}
			compact_cbuff = true;
		}
#ifdef HAVE_LZO
		else if(ENC_LZO == w_entry->encoding_in)
		{
			/*
			 * LZO compression is a pipe dream...
			 *
			 * The reason why lzo looked so good besides beeing fast is:
			 * - no mem needed on decompress
			 * - only a user supplied wmem needed for compression, which
			 *   is "dead" after return (can use TLS)
			 * But these advantages have a reason: it has no builtin
			 * support to compress "running" streams. One or several
			 * Chunks -> OK, "transparent" streams -> NO.
			 *
			 * This may be ok with most use cases, but sucks in our case.
			 * Compression:
			 * - no mem window, so every "stop" (we see at most some packets
			 *   at once) is like a full finish sync flush
			 *   -> bad compression, or hacking lzo while loosing the "no
			 *      persistent state" advantage (give it a memory window)
			 * Decompression:
			 * - does not tell how many bytes got consumed, it even errs out
			 *   if not all could be consumed.
			 *   -> could be fixed with a wrapping format or hacking lzo...
			 * - would also need a mem window...
			 *
			 * We either have to chunk our continous stream or hack a sliding
			 * window into the guts of lzo (+ API adaption).
			 *
			 * Maybe for QHTs.
			 */
			return w_entry;
		}
#endif
		else
			return w_entry; /* this should not happen */

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
				logg_develd_old("taking %p\n", build_packet);

			logg_develd_old("**** bytes: %u\n", buffer_remaining(*d_source));

			if(!g2_packet_extract_from_stream(d_source, build_packet, server.settings.max_g2_packet_length, false))
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

					parg.connec      = w_entry;
					parg.src_addr    = NULL;
					parg.dst_addr    = NULL;
					parg.father      = NULL;
					parg.source      = build_packet;
					parg.target_lock = &w_entry->pts_lock;
					parg.target      = &w_entry->packets_to_send;
					parg.opaque      = NULL;
					if(g2_packet_decide_spec(&parg, g2_packet_dict)) {
						shortlock_lock(&w_entry->pts_lock);
						w_entry->poll_interrests |= (uint32_t)EPOLLOUT;
						shortlock_unlock(&w_entry->pts_lock);
					}

					save_build_packet = false;
					/* !!! packet is seen as finished here !!! */
					if(build_packet->is_freeable)
						logg_devel_old("freeing durable packet\n");
					else if(build_packet->data_trunk_is_freeable)
						logg_devel_old("datatrunk freed\n");

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
			if(buffer_remaining(*w_entry->recv)) {
				if(save_build_packet && build_packet == &tmp_packet)
					w_entry->build_packet = build_packet; /* reinject packet */
				goto retry_unpack;
			}
			if(compact_cbuff)
				buffer_compact(*w_entry->recv);
			logg_develd_old("---- cbytes: %u\n", buffer_remaining(*w_entry->recv));
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
	shortlock_lock(&w_entry->pts_lock);
	mutex_unlock(&w_entry->lock);
	p_entry->events = w_entry->poll_interrests;
	shortlock_unlock(&w_entry->pts_lock);
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, w_entry->com_socket, p_entry)) {
		logg_errno(LOGF_DEBUG, "changing EPoll interrests");
		return w_entry;
	}
	return NULL;
}

static char const rcsid_h[] GCC_ATTR_USED_VAR = "$Id: G2Handler.c,v 1.21 2005/07/29 09:24:04 redbully Exp redbully $";
/* EOF */
