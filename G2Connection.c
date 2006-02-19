/*
 * G2Connection.c
 * helper-function for G2-Connections
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
 * $Id: G2Connection.c,v 1.14 2004/12/18 18:06:13 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
// System includes
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <strings.h>
#include <zlib.h>
// other
#include "other.h"
// Own includes
#include "G2MainServer.h"
#define _G2CONNECTION_C
#include "G2Connection.h"
#include "lib/log_facility.h"

#define PACKET_SPACE_START_CAP 128

// encoding
static const action_string enc_as00	= {NULL,		str_size(ENC_NONE_S),		ENC_NONE_S};
static const action_string enc_as01	= {NULL,		str_size(ENC_DEFLATE_S),	ENC_DEFLATE_S};

const action_string *KNOWN_ENCODINGS[] GCC_ATTR_VIS("hidden") =
{
	&enc_as00,
	&enc_as01,
};

// internal action-prototypes
static bool empty_action(g2_connection_t *, size_t);
static bool accept_what(g2_connection_t *, size_t);
static bool remote_ip_what(g2_connection_t *, size_t);
static bool a_encoding_what(g2_connection_t *, size_t);
static bool c_encoding_what(g2_connection_t *, size_t);
static bool uagent_what(g2_connection_t *, size_t);
static bool ulpeer_what(g2_connection_t *, size_t);
static bool content_what(g2_connection_t *, size_t);
static bool listen_what(g2_connection_t *, size_t);


// headerfields
static const action_string h_as00 = {&accept_what,		str_size(ACCEPT_KEY),		ACCEPT_KEY};
static const action_string h_as01 = {&uagent_what,		str_size(UAGENT_KEY),		UAGENT_KEY};
static const action_string h_as02 = {&a_encoding_what,str_size(ACCEPT_ENC_KEY),	ACCEPT_ENC_KEY};
static const action_string h_as03 = {&remote_ip_what,	str_size(REMOTE_ADR_KEY),	REMOTE_ADR_KEY};
static const action_string h_as04 = {&listen_what,		str_size(LISTEN_ADR_KEY),	LISTEN_ADR_KEY};
static const action_string h_as05 = {&content_what,	str_size(CONTENT_KEY),		CONTENT_KEY};
static const action_string h_as06 = {&c_encoding_what,str_size(CONTENT_ENC_KEY),	CONTENT_ENC_KEY};
static const action_string h_as07 = {&ulpeer_what,		str_size(UPEER_KEY),			UPEER_KEY};
static const action_string h_as08 = {NULL,				str_size(UPEER_NEEDED_KEY),UPEER_NEEDED_KEY};
static const action_string h_as09 = {NULL,				str_size(X_TRY_UPEER_KEY),	X_TRY_UPEER_KEY};
static const action_string h_as10 = {&empty_action,	str_size(GGEP_KEY),			GGEP_KEY};
static const action_string h_as11 = {&empty_action,	str_size(PONG_C_KEY),		PONG_C_KEY};
static const action_string h_as12 = {&empty_action,	str_size(QUERY_ROUTE_KEY),	QUERY_ROUTE_KEY};
static const action_string h_as13 = {&empty_action,	str_size(VEND_MSG_KEY),		VEND_MSG_KEY};


const action_string *KNOWN_HEADER_FIELDS[KNOWN_HEADER_FIELDS_SUM] GCC_ATTR_VIS("hidden") = 
{
	&h_as00,
	&h_as01,
	&h_as02,
	&h_as03,
	&h_as04,
	&h_as05,
	&h_as06,
	&h_as07,
	&h_as08,
	&h_as09,
	&h_as10,
	&h_as11,
	&h_as12,
	&h_as13,
};

inline g2_connection_t *g2_con_alloc(size_t num)
{
	char *packet_spaces[2*num];
	g2_connection_t *ret_val = NULL;
	
	if(num)
		ret_val = malloc(num * sizeof(g2_connection_t));

	if(ret_val)
	{
		size_t i, j;
		for(i = 0; i < (2 * num); i++)
		{
			packet_spaces[i] = malloc(PACKET_SPACE_START_CAP);
			if(!packet_spaces[i])
			{
				for(; i; i--)
					free(packet_spaces[i-1]);

				free(ret_val);
				return NULL;
			}
		}

		for(i=0, j = 0; i < num; i++, j += 2)
		{
			ret_val[i].packet_1.data_trunk.data = packet_spaces[j];
			ret_val[i].packet_1.data_trunk_is_freeable = true;
			ret_val[i].packet_1.data_trunk.capacity = PACKET_SPACE_START_CAP;
			ret_val[i].packet_1.children = NULL;
			ret_val[i].packet_2.data_trunk.data = packet_spaces[j+1];
			ret_val[i].packet_2.data_trunk_is_freeable = true;
			ret_val[i].packet_2.data_trunk.capacity = PACKET_SPACE_START_CAP;
			ret_val[i].packet_2.children = NULL;
			_g2_con_clear(&ret_val[i], true);
		}
	}
	
	return ret_val;
}

static void *my_zmalloc(void *opaque, unsigned int items, unsigned int size)
{
	void *tmp_ptr = malloc(items * size);

	if(tmp_ptr)
		logg_develd("zlib alloc o: 0x%p\tp: 0x%p\ts: %u * %u = %lu\n", opaque, tmp_ptr, items, size, (unsigned long) items * size);
	
	return tmp_ptr;
}

static void my_zfree(void *opaque, void *to_free)
{
	logg_develd("zlib free o: 0x%p\tp: 0x%p\n", opaque, to_free);
	free(to_free);
}

inline void _g2_con_clear(g2_connection_t *work_entry, int new)
{

	memset(work_entry, 0, offsetof(g2_connection_t, recv));
	memset(work_entry->tmp1, 0, sizeof(work_entry->tmp1));
	memset(work_entry->tmp2, 0, sizeof(work_entry->tmp2));
	work_entry->sin_size = sizeof (work_entry->remote_host);
	work_entry->af_type = AF_INET;
	work_entry->com_socket = -1;

	// Internal States
	work_entry->sent_addr.sin_family = AF_INET;
/*	work_entry->connect_state = UNCONNECTED;
	work_entry->encoding_in = ENC_NONE;
	work_entry->encoding_out = ENC_NONE;
	work_entry->time_diff = 0.0;*/

	// Buffer
	work_entry->recv.pos = 0;
	work_entry->recv.capacity = sizeof(work_entry->recv.data);
	work_entry->recv.limit = work_entry->recv.capacity;
	work_entry->send.pos = 0;
	work_entry->send.capacity = sizeof(work_entry->send.data);
	work_entry->send.limit = work_entry->send.capacity;
	if(!new)
	{
		if(Z_OK != inflateEnd(&work_entry->z_decoder))
		{
			if(work_entry->z_decoder.msg)
				logg_posd(LOGF_DEBUG, "%s\n", work_entry->z_decoder.msg);
		}
		if(Z_OK != deflateEnd(&work_entry->z_encoder))
		{
			if(work_entry->z_encoder.msg)
				logg_posd(LOGF_DEBUG, "%s\n", work_entry->z_encoder.msg);
		}
		if(work_entry->recv_u)
		{
//			memset(work_entry->recv_u, 0, sizeof(*(work_entry->recv_u)));
			work_entry->recv_u->pos = 0;
			work_entry->recv_u->limit = work_entry->recv_u->capacity = sizeof(work_entry->recv_u->data);
		}
		if(work_entry->send_u)
		{
//			memset(work_entry->send_u, 0, sizeof(*(work_entry->send_u)));
			work_entry->send_u->pos = 0;
			work_entry->send_u->limit = work_entry->send_u->capacity = sizeof(work_entry->send_u->data);
		}

		if(work_entry->qht)
		{
			size_t tmp_length = work_entry->qht->data_length;
			if(work_entry->qht->fragments)
				free(work_entry->qht->fragments);
			memset(work_entry->qht, 0, sizeof(*(work_entry->qht)) + tmp_length);
			work_entry->qht->data_length = tmp_length;
		}

		work_entry->akt_packet = &(work_entry->packet_1);
		work_entry->build_packet = &(work_entry->packet_2);
/*		if(work_entry->akt_packet)
		{
			g2_packet_free(work_entry->akt_packet);
			work_entry->akt_packet = NULL;
		}
		if(work_entry->build_packet)
		{
			g2_packet_free(work_entry->build_packet);
			work_entry->build_packet = NULL;
		}
		if(work_entry->last_packet)
		{
			g2_packet_free(work_entry->last_packet);
			work_entry->last_packet = NULL;
		}*/
	}
	else
	{
		work_entry->recv_u = NULL;
		work_entry->send_u = NULL;
		work_entry->qht = NULL;
		work_entry->akt_packet = &(work_entry->packet_1);
		work_entry->build_packet = &(work_entry->packet_2);
//		work_entry->last_packet = &(work_entry->packet_1);
	}

	g2_packet_clean(work_entry->build_packet);
	g2_packet_clean(work_entry->akt_packet);
	
	memset(&work_entry->z_decoder, 0, sizeof(work_entry->z_decoder));
	memset(&work_entry->z_encoder, 0, sizeof(work_entry->z_encoder));
	
	work_entry->z_decoder.zalloc = work_entry->z_encoder.zalloc = my_zmalloc;
	work_entry->z_decoder.zfree = work_entry->z_encoder.zfree =  my_zfree;
	work_entry->z_decoder.opaque = work_entry->z_encoder.opaque =  work_entry;
}

inline void g2_con_free(g2_connection_t *to_free)
{
	if(!to_free)
		return;

	if(Z_OK != inflateEnd(&to_free->z_decoder))
	{
		if(to_free->z_decoder.msg)
			logg_posd(LOGF_DEBUG, "%s\n", to_free->z_decoder.msg);
	}

	if(Z_OK != deflateEnd(&to_free->z_encoder))
	{
		if(to_free->z_encoder.msg)
			logg_posd(LOGF_DEBUG, "%s\n", to_free->z_encoder.msg);
	}
	
	if(to_free->recv_u)
		free(to_free->recv_u);
	
	if(to_free->send_u)
		free(to_free->send_u);

	if(to_free->qht)
	{
		if(to_free->qht->fragments)
				free(to_free->qht->fragments);
		free(to_free->qht);
	}

/*	if(to_free->last_packet)
		g2_packet_free(to_free->last_packet);*/
	if(to_free->akt_packet)
		_g2_packet_free(to_free->akt_packet, 0);
	if(to_free->build_packet)
		_g2_packet_free(to_free->build_packet, 0);

	free(to_free);
}

// actionstring-functions
static bool content_what(g2_connection_t *to_con, size_t distance)
{
	to_con->flags.content_ok = true;

	if(str_size(ACCEPT_G2) <= distance)
	{
		if(!strncasecmp(buffer_start(to_con->recv), ACCEPT_G2, str_size(ACCEPT_G2)))
		{
		//found!!
			logg_develd_old("found for Content:\t\"%.*s\"\n", str_size(ACCEPT_G2), buffer_start(to_con->recv));
			to_con->flags.content_g2 = true;
			return false;
		}
	}

	to_con->flags.content_g2 = false;
	return true;
}

static bool ulpeer_what(g2_connection_t *to_con, GCC_ATTR_UNUSED_PARAM(size_t, distance))
{
	if(!strncasecmp(buffer_start(to_con->recv), G2_FALSE, str_size(G2_FALSE)))
	{
		to_con->flags.upeer = false;
		to_con->flags.upeer_ok = true;
		return false;
	}

	if(!strncasecmp(buffer_start(to_con->recv), G2_TRUE, str_size(G2_TRUE)))
	{
		to_con->flags.upeer = true;
		to_con->flags.upeer_ok = true;
		return false;
	}

	to_con->flags.upeer = false;
	to_con->flags.upeer_ok = false;
	return true;
}

static bool uagent_what(g2_connection_t *to_con, size_t distance)
{
	size_t n = (distance < (sizeof(to_con->uagent) - 1))? distance : sizeof(to_con->uagent) - 1;
	strncpy(to_con->uagent, buffer_start(to_con->recv), n);
	to_con->uagent[n] = '\0';
	to_con->flags.uagent_ok = true;
	logg_develd_old("found for User-Agent:\t\"%s\"\n", to_con->uagent);
	return false;
}

static bool a_encoding_what(g2_connection_t *to_con, size_t distance)
{
	size_t i;

	if(to_con->flags.second_header)
		return false;

	for(i = 0; i < sizeof(KNOWN_ENCODINGS)  / sizeof(action_string *); i++)
	{
		if(KNOWN_ENCODINGS[i]->length > distance)
			continue;

		if(!strncasecmp(buffer_start(to_con->recv), KNOWN_ENCODINGS[i]->txt, KNOWN_ENCODINGS[i]->length))
		{
		//found!!
			to_con->encoding_out = i;
			to_con->flags.enc_out_ok = true;
			logg_develd_old("found for Encoding:\t\"%.*s\" %i\n", KNOWN_ENCODINGS[i]->length, buffer_start(to_con->recv), to_con->encoding_in);
			return false;
		}
	}

	to_con->flags.enc_out_ok = false;
	return true;
}

static bool c_encoding_what(g2_connection_t *to_con, size_t distance)
{
	size_t i;

	if(!to_con->flags.second_header)
		return false;

	for(i = 0; i < sizeof(KNOWN_ENCODINGS)  / sizeof(action_string *); i++)
	{
		if(KNOWN_ENCODINGS[i]->length > distance)
			continue;

		if(!strncasecmp(buffer_start(to_con->recv), KNOWN_ENCODINGS[i]->txt, KNOWN_ENCODINGS[i]->length))
		{
		//found!!
			to_con->encoding_in = i;
			to_con->flags.enc_in_ok = true;
			logg_develd_old("found for Encoding:\t\"%.*s\" %i\n", KNOWN_ENCODINGS[i]->length, buffer_start(to_con->recv), to_con->encoding_in);
			return false;
		}
	}

	to_con->flags.enc_in_ok = false;
	return true;
}

static bool remote_ip_what(g2_connection_t *to_con, size_t distance)
{
	char buffer[distance+1];
	int ret_val = 0;

	strncpy(buffer, buffer_start(to_con->recv), distance);
	buffer[distance] = '\0';

	ret_val = inet_pton(to_con->af_type, buffer, &(to_con->sent_addr.sin_addr));
	
	if(0 < ret_val)
	{
		logg_develd_old("found for Remote-IP:\t%s\n", inet_ntop(to_con->af_type, &to_con->sent_addr.sin_addr, char addr_buf[INET6_ADDRSTRLEN], sizeof(addr_buf)));
		to_con->flags.addr_ok = true;
		return false;
	}
	else
	{
		to_con->flags.addr_ok = false;
		if(0 == ret_val)
		{
			char addr_buf[INET6_ADDRSTRLEN];
			logg_posd(LOGF_DEBUG, "%s Ip: %s\tPort: %hu\tFDNum: %i\n",
				"got illegal remote-ip",
				inet_ntop(to_con->af_type, &to_con->remote_host.sin_addr, addr_buf, sizeof(addr_buf)),
				//inet_ntoa(to_con->remote_host.sin_addr),
				ntohs(to_con->remote_host.sin_port),
				to_con->com_socket);
		}
		else
			logg_errno(LOGF_DEBUG, "reading remote-ip");

		return true;
	}
}


static bool accept_what(g2_connection_t *to_con, size_t distance)
{
	char *w_ptr = buffer_start(to_con->recv);

	if(to_con->flags.second_header)
		return false;

	to_con->flags.accept_ok = true;

	while(distance)
	{
		if(str_size(ACCEPT_G2) > distance)
			break;

		if(!strncasecmp(w_ptr, ACCEPT_G2, str_size(ACCEPT_G2)))
		{
		//found!!
			logg_develd_old("found for Accept:\t\"%.*s\"\n", str_size(ACCEPT_G2), buffer_start(to_con->recv));
			to_con->flags.accept_g2 = true;
			return false;
		}
		else
		{
// TODO: Does this work?
		//search a ','
			while(distance)
			{
				w_ptr++;
				distance--;
				if(',' == *(w_ptr-1))
					break;
			}
		}
	}

	to_con->flags.accept_g2 = false;
	return true;
}

static bool listen_what(g2_connection_t * to_con, size_t distance)
{
	/* stringdisplacements are int... */
	logg_develd(LISTEN_ADR_KEY " needs to be handeld: %.*s\n", (int)distance, buffer_start(to_con->recv));
	return false;
}

bool empty_action(GCC_ATTR_UNUSED_PARAM(g2_connection_t *, to_con), GCC_ATTR_UNUSED_PARAM(size_t, distance))
{
	return false;
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id: G2Connection.c,v 1.14 2004/12/18 18:06:13 redbully Exp redbully $";
//EOF