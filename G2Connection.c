/*
 * G2Connection.c
 * helper-function for G2-Connections
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
 * $Id: G2Connection.c,v 1.14 2004/12/18 18:06:13 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <strings.h>
#include <ctype.h>
#include <zlib.h>
/* other */
#include "lib/other.h"
/* Own includes */
#include "G2MainServer.h"
#define _G2CONNECTION_C
#include "G2Connection.h"
#include "G2ConRegistry.h"
#include "G2KHL.h"
#include "lib/log_facility.h"
#include "lib/recv_buff.h"
#include "lib/atomic.h"
#include "lib/hzp.h"
#include "lib/my_bitops.h"

#define PACKET_SPACE_START_CAP 128

/* Protos */
#ifndef VALGRIND_ME_CON
	/* You better not kill this proto, our it wount work ;) */
static void g2_con_init(void) GCC_ATTR_CONSTRUCT;
static void g2_con_deinit(void) GCC_ATTR_DESTRUCT;
#endif
	/* internal action-prototypes */
static bool empty_action_c(g2_connection_t *, size_t);
static bool accept_what(g2_connection_t *, size_t);
static bool remote_ip_what(g2_connection_t *, size_t);
static bool a_encoding_what(g2_connection_t *, size_t);
static bool c_encoding_what(g2_connection_t *, size_t);
static bool uagent_what(g2_connection_t *, size_t);
static bool ulpeer_what(g2_connection_t *, size_t);
static bool content_what(g2_connection_t *, size_t);
static bool listen_what(g2_connection_t *, size_t);
static bool try_hub_what(g2_connection_t *, size_t);
static bool crawler_what(g2_connection_t *, size_t);

/* Vars */
#ifndef VALGRIND_ME_CON
	/* con buffer */
static atomicptra_t free_cons[FC_CAP_START];
#endif
	/* encoding */
static const action_string enc_as00 = {NULL, str_size(ENC_NONE_S),    ENC_NONE_S};
static const action_string enc_as01 = {NULL, str_size(ENC_DEFLATE_S), ENC_DEFLATE_S};
static const action_string enc_as02 = {NULL, str_size(ENC_LZO_S),     ENC_LZO_S};

const action_string *KNOWN_ENCODINGS[] GCC_ATTR_VIS("hidden") =
{
	&enc_as00,
	&enc_as01,
	&enc_as02,
};

	/* headerfields */
/*
 * These header fields are now sorted, so we can use bsearch.
 */
#include "G2HeaderFields.h"

/*
 * Funcs
 */
#ifndef VALGRIND_ME_CON
	/* constructor */
static void g2_con_init(void)
{
	size_t i;
	/* memory for free_cons */
	for(i = 0; i < anum(free_cons); i++)
	{
		g2_connection_t *tmp = g2_con_alloc(1);
		if(tmp)
		{
			if((tmp = atomic_pxa(tmp, &free_cons[i]))) {
				logg_pos(LOGF_CRIT, "another thread working while init???\n");
				g2_con_free(tmp);
			}
		}
		else
		{
			logg_errno(LOGF_CRIT, "free_cons memory");
			if(FC_TRESHOLD < i)
				break;

			for(; i > 0; --i) {
				tmp = NULL;
				g2_con_free(atomic_pxa(tmp, &free_cons[i]));
			}
			exit(EXIT_FAILURE);
		}
	}
}

static void g2_con_deinit(void)
{
	size_t i;
	/* memory for free_cons */
	for(i = 0; i < anum(free_cons); i++)
	{
		g2_connection_t *tmp = NULL;
		g2_con_free(atomic_pxa(tmp, &free_cons[i]));
	}
}
#endif

g2_connection_t *g2_con_alloc(size_t num)
{
	g2_connection_t *ret_val = NULL;
	
	if(num)
		ret_val = malloc(num * sizeof(g2_connection_t));

	if(ret_val) {
		size_t i;
		for(i = 0; i < num; i++)
			_g2_con_clear(&ret_val[i], true);
	}
	
	return ret_val;
}

#if 0
static void *my_zmalloc(void *opaque GCC_ATTR_UNUSED_PARAM, unsigned int items, unsigned int size)
{
	void *tmp_ptr = malloc(items * size);

	if(tmp_ptr)
		logg_develd_old("zlib alloc o: 0x%p\tp: 0x%p\ts: %u * %u = %lu\n", opaque, tmp_ptr, items, size, (unsigned long) items * size);

	return tmp_ptr;
}

static void my_zfree(void *opaque GCC_ATTR_UNUSED_PARAM, void *to_free)
{
	logg_develd_old("zlib free o: 0x%p\tp: 0x%p\n", opaque, to_free);
	free(to_free);
}
#endif

void GCC_ATTR_FASTCALL _g2_con_clear(g2_connection_t *work_entry, int new)
{
	/* cleanup stuff we don't wan't to memset */
	if(!new)
	{
		timeout_cancel(&work_entry->active_to);
		DESTROY_TIMEOUT(&work_entry->active_to);

		if(G2CONNECTED != work_entry->connect_state) {
			timeout_cancel(&work_entry->u.accept.header_complete_to);
			DESTROY_TIMEOUT(&work_entry->u.accept.header_complete_to);
		} else {
			timeout_cancel(&work_entry->u.handler.z_flush_to);
			DESTROY_TIMEOUT(&work_entry->u.handler.z_flush_to);
		}

		g2_conreg_remove(work_entry);
		if(work_entry->z_decoder)
		{
			int res = inflateEnd(work_entry->z_decoder);
			if(Z_OK != res && Z_DATA_ERROR != res) {
				if(work_entry->z_decoder->msg)
					logg_posd(LOGF_DEBUG, "%s\n", work_entry->z_decoder->msg);
			}
			free(work_entry->z_decoder);
		}
		if(work_entry->z_encoder)
		{
			int res = deflateEnd(work_entry->z_encoder);
			if(Z_OK != res && Z_DATA_ERROR != res) {
				if(work_entry->z_encoder->msg)
					logg_posd(LOGF_DEBUG, "%s\n", work_entry->z_encoder->msg);
			}
			free(work_entry->z_encoder);
		}
	}
	/*
	 * wipe everything which is small, has many fields, of which only
	 * some need to be initialised
	 */
	memset(work_entry, 0, offsetof(g2_connection_t, recv));
#if 0
// TODO: OUCH! these memset are expensive, and for what did i add these fields?
	memset(work_entry->tmp1, 0, sizeof(work_entry->tmp1));
	memset(work_entry->tmp2, 0, sizeof(work_entry->tmp2));
#endif

	/*
	 * reinitialise stuff
	 */
	/* networking */
	work_entry->gup = GUP_G2CONNEC_HANDSHAKE;
	work_entry->com_socket = -1;

	/* Internal States */
	work_entry->sent_addr.s_fam = AF_INET;
#if 0
	/* they are zero by coincidence, but lets shove some cycles */
	work_entry->connect_state = UNCONNECTED;
	work_entry->encoding_in = ENC_NONE;
	work_entry->encoding_out = ENC_NONE;
	work_entry->time_diff = 0.0;
#endif

	/* Buffer */
/*	work_entry->recv.pos = 0;
	work_entry->recv.capacity = sizeof(work_entry->recv.data);
	work_entry->recv.limit = work_entry->recv.capacity;
	work_entry->send.pos = 0;
	work_entry->send.capacity = sizeof(work_entry->send.data);
	work_entry->send.limit = work_entry->send.capacity;*/
	if(!new)
	{
		struct list_head *e, *n;

		if(work_entry->recv)
			recv_buff_free(work_entry->recv);
		if(work_entry->send)
			recv_buff_free(work_entry->send);
		if(work_entry->recv_u)
			recv_buff_free(work_entry->recv_u);
		if(work_entry->send_u)
			recv_buff_free(work_entry->send_u);
		if(work_entry->build_packet)
			g2_packet_free(work_entry->build_packet);

		g2_qht_put(work_entry->qht);
		g2_qht_put(work_entry->sent_qht);

		list_for_each_safe(e, n, &work_entry->packets_to_send) {
			g2_packet_t *entry = list_entry(e, g2_packet_t, list);
			list_del_init(e);
			g2_packet_free(entry);
		}
	}
	else {
		INIT_HLIST_NODE(&work_entry->registry);
		shortlock_t_init(&work_entry->pts_lock);
		pthread_mutex_init(&work_entry->lock, NULL);
		work_entry->qht = NULL;
	}

	work_entry->recv = NULL;
	work_entry->send = NULL;
	work_entry->recv_u = NULL;
	work_entry->send_u = NULL;

	work_entry->qht = NULL;
	work_entry->sent_qht = NULL;
	work_entry->build_packet = NULL;
	INIT_LIST_HEAD(&work_entry->packets_to_send);
	INIT_LIST_HEAD(&work_entry->hub_list);
	INIT_TIMEOUT(&work_entry->active_to);
	INIT_TIMEOUT(&work_entry->u.accept.header_complete_to);
	pthread_mutex_unlock(&work_entry->lock);
}

static void g2_con_free_internal(g2_connection_t *to_free)
{
	/* timeouts */
	timeout_cancel(&to_free->active_to);

	if(G2CONNECTED != to_free->connect_state) {
		timeout_cancel(&to_free->u.accept.header_complete_to);
		DESTROY_TIMEOUT(&to_free->u.accept.header_complete_to);
	} else {
		timeout_cancel(&to_free->u.handler.z_flush_to);
		DESTROY_TIMEOUT(&to_free->u.handler.z_flush_to);
	}

	/* zlib */
	if(to_free->z_decoder)
	{
		if(Z_OK != inflateEnd(to_free->z_decoder)) {
			if(to_free->z_decoder->msg)
				logg_posd(LOGF_DEBUG, "%s\n", to_free->z_decoder->msg);
		}
		free(to_free->z_decoder);
	}
	if(to_free->z_encoder)
	{
		if(Z_OK != deflateEnd(to_free->z_encoder)) {
			if(to_free->z_encoder->msg)
				logg_posd(LOGF_DEBUG, "%s\n", to_free->z_encoder->msg);
		}
		free(to_free->z_encoder);
	}

	/* buffer */
	if(to_free->recv)
		recv_buff_free(to_free->recv);
	if(to_free->send)
		recv_buff_free(to_free->send);
	if(to_free->recv_u)
		recv_buff_free(to_free->recv_u);
	if(to_free->send_u)
		recv_buff_free(to_free->send_u);

	/* qht */
	g2_qht_put(to_free->qht);
	g2_qht_put(to_free->sent_qht);

	shortlock_t_destroy(&to_free->pts_lock);
	pthread_mutex_destroy(&to_free->lock);
}

void g2_con_free_glob(g2_connection_t *to_free)
{
	struct list_head *e, *n;

	if(!to_free)
		return;

	g2_con_free_internal(to_free);
	/* packets */
	if(to_free->build_packet)
		g2_packet_free_glob(to_free->build_packet);

	list_for_each_safe(e, n, &to_free->packets_to_send) {
		g2_packet_t *entry = list_entry(e, g2_packet_t, list);
		list_del_init(e);
		g2_packet_free_glob(entry);
	}

	free(to_free);
}

void g2_con_free(g2_connection_t *to_free)
{
	struct list_head *e, *n;

	if(!to_free)
		return;

	g2_con_free_internal(to_free);
	/* packets */
	if(to_free->build_packet)
		g2_packet_free(to_free->build_packet);

	list_for_each_safe(e, n, &to_free->packets_to_send) {
		g2_packet_t *entry = list_entry(e, g2_packet_t, list);
		list_del_init(e);
		g2_packet_free(entry);
	}

	free(to_free);
}

#ifdef HELGRIND_ME
# include <valgrind/helgrind.h>
void GCC_ATTR_FASTCALL g2_con_helgrind_transfer(g2_connection_t *connec)
{
	VALGRIND_HG_CLEAN_MEMORY(connec, sizeof(*connec));
}
#endif

#ifdef DEBUG_CON_ALLOC
g2_connection_t *_g2_con_get_free(const char *from_file, const char *from_func, const unsigned int from_line)
#else
g2_connection_t *g2_con_get_free(void)
#endif
{
#ifndef VALGRIND_ME_CON
	int failcount = 0;
	g2_connection_t *ret_val = NULL;

#ifdef DEBUG_CON_ALLOC
	logg_develd_old("called from %s:%s()@%u\n", from_file, from_func, from_line);
#endif

	do
	{
		size_t i = 0;
		for(i = 0; i < anum(free_cons); i++)
		{
			if(atomic_pread(&free_cons[i])) {
				if((ret_val = atomic_pxa(ret_val, &free_cons[i])))
					return ret_val;
			}
		}
	} while(++failcount < 2);
#endif

	return g2_con_alloc(1);
}

#ifdef DEBUG_CON_ALLOC
void _g2_con_ret_free(g2_connection_t *to_return, const char *from_file, const char *from_func, const unsigned int from_line)
#else
void g2_con_ret_free(g2_connection_t *to_return)
#endif
{
#ifndef VALGRIND_ME_CON
	int failcount = 0;

#ifdef DEBUG_CON_ALLOC
	logg_develd_old("called from %s:%s()@%u\n", from_file, from_func, from_line);
#endif

	do
	{
		size_t i = 0;
		for(i = 0; i < anum(free_cons); i++)
		{
			if(!atomic_pread(&free_cons[i])) {
				if(!(to_return = atomic_pxa(to_return, &free_cons[i])))
					return;
			}
		}
	} while(++failcount < 2);
#endif

	hzp_deferfree(&to_return->hzp, to_return, (void (*)(void *))g2_con_free_glob);
}


/*
 * actionstring-functions
 */
static bool content_what(g2_connection_t *to_con, size_t distance)
{
	to_con->u.accept.flags.content_ok = true;

	if(likely(str_size(ACCEPT_G2) <= distance))
	{
		if(!strncasecmp_a(buffer_start(*to_con->recv), ACCEPT_G2, str_size(ACCEPT_G2)))
		{
			/* found!! */
			logg_develd_old("found for Content:\t\"%.*s\"\n", str_size(ACCEPT_G2), buffer_start(*to_con->recv));
			to_con->u.accept.flags.content_g2 = true;
			return false;
		}
	}

	to_con->u.accept.flags.content_g2 = false;
	return true;
}

static bool ulpeer_what(g2_connection_t *to_con, size_t distance GCC_ATTR_UNUSED_PARAM)
{
	if(!strncasecmp_a(buffer_start(*to_con->recv), G2_FALSE, str_size(G2_FALSE))) {
		to_con->flags.upeer = false;
		to_con->u.accept.flags.upeer_ok = true;
		return false;
	}

	if(!strncasecmp_a(buffer_start(*to_con->recv), G2_TRUE, str_size(G2_TRUE))) {
		to_con->flags.upeer = true;
		to_con->u.accept.flags.upeer_ok = true;
		return false;
	}

	to_con->flags.upeer = false;
	to_con->u.accept.flags.upeer_ok = false;
	return true;
}

static bool uagent_what(g2_connection_t *to_con, size_t distance)
{
	size_t n = (distance < (sizeof(to_con->uagent) - 1))? distance : sizeof(to_con->uagent) - 1;
	strncpy(to_con->uagent, buffer_start(*to_con->recv), n);
	to_con->uagent[n] = '\0';
	to_con->u.accept.flags.uagent_ok = true;
	logg_develd_old("found for User-Agent:\t\"%s\"\n", to_con->uagent);
	return false;
}

static bool a_encoding_what(g2_connection_t *to_con, size_t distance)
{
	size_t i;

	if(to_con->u.accept.flags.second_header)
		return false;

	for(i = 0; i < sizeof(KNOWN_ENCODINGS)  / sizeof(action_string *); i++)
	{
		if(unlikely(KNOWN_ENCODINGS[i]->length > distance))
			continue;

		if(!strncasecmp_a(buffer_start(*to_con->recv), KNOWN_ENCODINGS[i]->txt, KNOWN_ENCODINGS[i]->length))
		{
		/* found!! */
			to_con->encoding_out = i;
			to_con->u.accept.flags.enc_out_ok = true;
			logg_develd_old("found for Encoding out:\t\"%.*s\" %i\n", KNOWN_ENCODINGS[i]->length, buffer_start(*to_con->recv), to_con->encoding_out);
			return false;
		}
	}

	to_con->u.accept.flags.enc_out_ok = false;
	return true;
}

static bool c_encoding_what(g2_connection_t *to_con, size_t distance)
{
	size_t i;

	if(!to_con->u.accept.flags.second_header)
		return false;

	for(i = 0; i < sizeof(KNOWN_ENCODINGS)  / sizeof(action_string *); i++)
	{
		if(KNOWN_ENCODINGS[i]->length > distance)
			continue;

		if(!strncasecmp_a(buffer_start(*to_con->recv), KNOWN_ENCODINGS[i]->txt, KNOWN_ENCODINGS[i]->length))
		{
		/* found!! */
			to_con->encoding_in = i;
			to_con->u.accept.flags.enc_in_ok = true;
			logg_develd_old("found for Encoding in:\t\"%.*s\" %i\n", KNOWN_ENCODINGS[i]->length, buffer_start(*to_con->recv), to_con->encoding_in);
			return false;
		}
	}

	to_con->u.accept.flags.enc_in_ok = false;
	return true;
}

static bool remote_ip_what(g2_connection_t *to_con, size_t distance)
{
	char *buffer;
	union combo_addr l_addr;
	char *z;
	int ret_val = 0;
	uint16_t port = 0;

	buffer = buffer_start(*to_con->recv);
	/* brutally NUL terminate, there is a \r\n behind us */
	buffer[distance] = '\0';

	/* port */
	z = strrchr(buffer, ':');
	if(z) {
		*z++ = '\0';
		port = atoi(z);
	}
	/* IPv6? */
	z = buffer;
	if(*z == '[')
		z++;
	ret_val = combo_addr_read(z, &l_addr);

	if(0 < ret_val) {
		combo_addr_set_port(&l_addr, port);
		logg_develd_old("they say is our IP:\t%p#I\n", &l_addr);
		to_con->u.accept.flags.addr_ok = true;
		return false;
	}
	else
	{
		to_con->u.accept.flags.addr_ok = false;
		if(0 == ret_val)
			logg_posd(LOGF_DEBUG, "%s %p#I\tFDNum: %i \"%.*s\"\n",
			          "got illegal remote ip from", &to_con->remote_host,
			           to_con->com_socket, (int)distance, buffer);
		else
			logg_errno(LOGF_DEBUG, "reading remote ip");

		return true;
	}
	return false;
}


static bool accept_what(g2_connection_t *to_con, size_t distance)
{
	char *w_ptr = buffer_start(*to_con->recv);

	if(to_con->u.accept.flags.second_header)
		return false;

	to_con->u.accept.flags.accept_ok = true;

	while(distance)
	{
		if(str_size(ACCEPT_G2) > distance)
			break;

		if(!strncasecmp_a(w_ptr, ACCEPT_G2, str_size(ACCEPT_G2)))
		{
		/* found!! */
			logg_develd_old("found for Accept:\t\"%.*s\"\n", str_size(ACCEPT_G2), buffer_start(*to_con->recv));
			to_con->u.accept.flags.accept_g2 = true;
			return false;
		}
		else
		{
// TODO: Does this work? No, needs to be written
		/* search a ',' */
			while(distance)
			{
				w_ptr++;
				distance--;
				if(',' == *(w_ptr-1))
					break;
			}
		}
	}

	to_con->u.accept.flags.accept_g2 = false;
	return true;
}

static bool listen_what(g2_connection_t *to_con, size_t distance)
{
	char *buffer;
	char *z;
	int ret_val = 0;
	uint16_t port = 0;

	buffer = buffer_start(*to_con->recv);
	/* brutally NUL terminate, there is a \r\n behind us */
	buffer[distance] = '\0';

	/* port */
// TODO: memrchr
	z = strrchr(buffer, ':');
	if(z) {
		*z++ = '\0';
		port = atoi(z);
	}
	/* IPv6? */
	z = buffer;
	if(*z == '[')
		z++;
	ret_val = combo_addr_read(z, &to_con->sent_addr);

	if(0 < ret_val) {
		combo_addr_set_port(&to_con->sent_addr, htons(port));
		logg_develd_old("they think of their-IP:\t%p#I\n", &to_con->sent_addr);
		return false;
	}
	else
	{
		if(0 == ret_val)
			logg_posd(LOGF_DEBUG, "%s %p#I\tFDNum: %i \"%.*s\"\n",
			          "got illegal listen ip from", &to_con->remote_host,
			           to_con->com_socket, (int)distance, buffer);
		else
			logg_errno(LOGF_DEBUG, "reading listen ip");

		return true;
	}
}

static bool try_hub_what(g2_connection_t *to_con, size_t distance)
{
	union combo_addr where;
	struct tm when_tm;
	time_t when;
	char *w_ptr = buffer_start(*to_con->recv);
	char *x;
	char c;

	/* we only accept foreign hub info after the second header */
	if(!to_con->u.accept.flags.second_header)
		return true;

	/* brutally NUL terminate, there is a \r\n behind us */
	w_ptr[distance] = '\0';

	/* init stuff */
	gmtime_r(&local_time_now, &when_tm);
	memset(&where, 0, sizeof(where));

	for(c = 0xff; c; w_ptr = x)
	{
		char *t_str, *z;
		uint16_t port;

		x = strchrnul(w_ptr, ',');
		/* remove leading white-spaces in field-data */
		for(c = *x, *x++ = '\0'; c && (c = *x) && isspace((int)*x);)
			*x++ = '\0';

		t_str = strchrnul(w_ptr, ' ');
		if(!*t_str)
			continue;
		*t_str++ = '\0';

		/* port */
		z = strrchr(w_ptr, ':');
		if(!z)
			continue;
		*z++ = '\0';
		port = atoi(z);
		if(!port)
			continue;
		combo_addr_set_port(&where, port);

		/* IPv6? */
		if(*w_ptr == '[')
			w_ptr++;
		if(!combo_addr_read(w_ptr, &where))
			continue;

		/* time stamp */
		z = strptime(t_str, "%Y-%m-%dT%H:%M", &when_tm);
		if(!z) {
			logg_develd("couldn't read time stamp \"%s\"\n", z);
			continue;
		}
		if('\0' != *z || 'Z' != *z)
			logg_develd("funny stuff behind time stamp \"%s\"\n", z);
		when = mktime(&when_tm);

		g2_khl_add(&where, when, false);
	}

	return true;
}

static bool crawler_what(g2_connection_t *to_con, size_t distance GCC_ATTR_UNUSED_PARAM)
{
	to_con->u.accept.flags.had_crawler = true;
	return false;
}

static bool empty_action_c(g2_connection_t *to_con GCC_ATTR_UNUSED_PARAM, size_t distance GCC_ATTR_UNUSED_PARAM)
{
	return false;
}

static char const rcsid_c[] GCC_ATTR_USED_VAR = "$Id: G2Connection.c,v 1.14 2004/12/18 18:06:13 redbully Exp redbully $";
/* EOF */
