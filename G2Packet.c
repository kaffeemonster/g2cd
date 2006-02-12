/*
 * G2Packet.c
 * helper-functions for G2-packets
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
 * $Id: G2Packet.c,v 1.12 2004/12/18 18:06:13 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
// System includes
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <alloca.h>
#include <time.h>
#include <zlib.h>
// other
#include "other.h"
// Own includes
#define _G2PACKET_C
#define _NEED_G2_P_TYPE
#include "G2Packet.h"
#include "G2MainServer.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/my_bitops.h"

// Internal Prototypes
static bool empty_action(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_KHL(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_KHL_TS(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_LNI(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_LNI_NA(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_LNI_GU(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_LNI_V(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_PI(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_QHT(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_UPROC(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_UPROD(g2_connection_t *, g2_packet_t *, struct norm_buff *);

static inline bool g2_packet_decide_spec(g2_connection_t *, struct norm_buff *, const g2_p_type_t *, g2_packet_t *);

// sixth type-char-layer
// UPROD
static const g2_p_type_t packet_dict_UPROD0 = { NULL, {.action = &handle_UPROD}, '\0', true };
// UPROC
static const g2_p_type_t packet_dict_UPROC0 = { NULL, {.action = &handle_UPROC}, '\0', true };

// fith type-char-layer
// UPROx
static const g2_p_type_t packet_dict_UPROD = { NULL, {.found = &packet_dict_UPROD0}, 'D', false };
static const g2_p_type_t packet_dict_UPROC = { &packet_dict_UPROD, {.found = &packet_dict_UPROC0}, 'C', false };

// fourth type-char-layer
// KHL
static const g2_p_type_t packet_dict_KHL0 = { NULL, {.action = &handle_KHL}, '\0', true };
// LNI
static const g2_p_type_t packet_dict_LNI0 = { NULL, {.action = &handle_LNI}, '\0', true };
// QHT
static const g2_p_type_t packet_dict_QHT0 = { NULL, {.action = &handle_QHT}, '\0', true };
// QKR
static const g2_p_type_t packet_dict_QKR0 = { NULL, {.action = NULL}, '\0', true };
// QKA
static const g2_p_type_t packet_dict_QKA0 = { NULL, {.action = NULL}, '\0', true };
// UPRx
static const g2_p_type_t packet_dict_UPRO = { NULL, {.found = &packet_dict_UPROC}, 'O', false };

// third type-char-layer
// KHx
static const g2_p_type_t packet_dict_KHL = { NULL, {.found = &packet_dict_KHL0}, 'L', false };
// LNx
static const g2_p_type_t packet_dict_LNI = { NULL, {.found = &packet_dict_LNI0}, 'I', false };
// PI
static const g2_p_type_t packet_dict_PI0 = { NULL, {.action = &handle_PI}, '\0', true };
// PO
static const g2_p_type_t packet_dict_PO0 = { NULL, {.action = NULL}, '\0', true };
// Q2
static const g2_p_type_t packet_dict_Q20 = { NULL, {.action = NULL}, '\0', true };
// QHx
static const g2_p_type_t packet_dict_QHT = { NULL, {.found = &packet_dict_QHT0}, 'T', false };
// QKx
static const g2_p_type_t packet_dict_QKA = { NULL, {.found = &packet_dict_QKA0}, 'A', false };
static const g2_p_type_t packet_dict_QKR = { &packet_dict_QKA, {.found = &packet_dict_QKR0}, 'R', false };
// UPx
static const g2_p_type_t packet_dict_UPR = { NULL, {.found = &packet_dict_UPRO}, 'R', false };

// second type-char-layer
// Kx
static const g2_p_type_t packet_dict_KH = { NULL, {.found = &packet_dict_KHL}, 'H', false };
// Lx
static const g2_p_type_t packet_dict_LN = { NULL, {.found = &packet_dict_LNI}, 'N', false };
// Px
static const g2_p_type_t packet_dict_PO = { NULL, {.found = &packet_dict_PO0}, 'O', false };
static const g2_p_type_t packet_dict_PI = { &packet_dict_PO, {.found = &packet_dict_PI0}, 'I', false };
// Qx
static const g2_p_type_t packet_dict_QH = { NULL, {.found = &packet_dict_QHT}, 'H', false };
static const g2_p_type_t packet_dict_QK = { &packet_dict_QH, {.found = &packet_dict_QKR}, 'K', false };
static const g2_p_type_t packet_dict_Q2 = { &packet_dict_QK, {.found = &packet_dict_Q20}, '2', false };
// Ux
static const g2_p_type_t packet_dict_UP = { NULL, {.found = &packet_dict_UPR}, 'P', false };

// first type-char-layer
static const g2_p_type_t packet_dict_U = { NULL, {.found = &packet_dict_UP}, 'U', false };
static const g2_p_type_t packet_dict_Q = { &packet_dict_U, {.found = &packet_dict_Q2}, 'Q', false };
static const g2_p_type_t packet_dict_P = { &packet_dict_Q, {.found = &packet_dict_PI}, 'P', false };
static const g2_p_type_t packet_dict_L = { &packet_dict_P, {.found = &packet_dict_LN}, 'L', false };
const g2_p_type_t g2_packet_dict = { &packet_dict_L, {.found = &packet_dict_KH}, 'K', false }; //packet_dict_K

// LNI-childs
// third
// /LNI/HS
static const g2_p_type_t LNI_packet_dict_HS0 = { NULL, {.action = &empty_action}, '\0', true };
// /LNI/GU
static const g2_p_type_t LNI_packet_dict_GU0 = { NULL, {.action = &handle_LNI_GU}, '\0', true };
// /LNI/LS
static const g2_p_type_t LNI_packet_dict_LS0 = { NULL, {.action = &empty_action}, '\0', true };
// /LNI/NA
static const g2_p_type_t LNI_packet_dict_NA0 = { NULL, {.action = &handle_LNI_NA}, '\0', true };
// /LNI/QK
static const g2_p_type_t LNI_packet_dict_QK0 = { NULL, {.action = &empty_action}, '\0', true };

// second
// /LNI/Hx
static const g2_p_type_t LNI_packet_dict_HS = { NULL, {.found = &LNI_packet_dict_HS0}, 'S', false };
// /LNI/Gx
static const g2_p_type_t LNI_packet_dict_GU = { NULL, {.found = &LNI_packet_dict_GU0}, 'U', false };
// /LNI/Lx
static const g2_p_type_t LNI_packet_dict_LS = { NULL, {.found = &LNI_packet_dict_LS0}, 'S', false };
// /LNI/Nx
static const g2_p_type_t LNI_packet_dict_NA = { NULL, {.found = &LNI_packet_dict_NA0}, 'A', false };
// /LNI/Qx
static const g2_p_type_t LNI_packet_dict_QK = { NULL, {.found = &LNI_packet_dict_QK0}, 'K', false };
// /LNI/V
static const g2_p_type_t LNI_packet_dict_V0 = { NULL, {.action = &handle_LNI_V}, '\0', true };

// first
static const g2_p_type_t LNI_packet_dict_V = { NULL, {.found = &LNI_packet_dict_V0}, 'V', false };
static const g2_p_type_t LNI_packet_dict_Q = { &LNI_packet_dict_V, {.found = &LNI_packet_dict_QK}, 'Q', false };
static const g2_p_type_t LNI_packet_dict_N = { &LNI_packet_dict_Q, {.found = &LNI_packet_dict_NA}, 'N', false };
static const g2_p_type_t LNI_packet_dict_L = { &LNI_packet_dict_N, {.found = &LNI_packet_dict_LS}, 'L', false };
static const g2_p_type_t LNI_packet_dict_H = { &LNI_packet_dict_L, {.found = &LNI_packet_dict_HS}, 'H', false };
static const g2_p_type_t LNI_packet_dict = { &LNI_packet_dict_H, {.found = &LNI_packet_dict_GU}, 'G', false }; // LNI_packet_dict_G


// KHL-childs
// third
// /KHL/TS
static const g2_p_type_t KHL_packet_dict_TS0 = { NULL, {.action = &handle_KHL_TS}, '\0', true };
// /KHL/NH
static const g2_p_type_t KHL_packet_dict_NH0 = { NULL, {.action = NULL}, '\0', true };
// /KHL/CH
static const g2_p_type_t KHL_packet_dict_CH0 = { NULL, {.action = NULL}, '\0', true };

// second
// /KHL/Tx
static const g2_p_type_t KHL_packet_dict_TS = { NULL, {.found = &KHL_packet_dict_TS0}, 'S', false };
// /KHL/Nx
static const g2_p_type_t KHL_packet_dict_NH = { NULL, {.found = &KHL_packet_dict_NH0}, 'H', false };
// /KHL/Cx
static const g2_p_type_t KHL_packet_dict_CH = { NULL, {.found = &KHL_packet_dict_CH0}, 'H', false };

// first
static const g2_p_type_t KHL_packet_dict_T = { NULL, {.found = &KHL_packet_dict_TS}, 'T', false };
static const g2_p_type_t KHL_packet_dict_N = { &KHL_packet_dict_T, {.found = &KHL_packet_dict_NH}, 'N', false };
static const g2_p_type_t KHL_packet_dict = { &KHL_packet_dict_N, {.found = &KHL_packet_dict_CH}, 'C', false }; // KHL_packet_dict_C

// prebuild packets
static const char packet_po[]		= { 0x08, 'P', 'O', };
static const char packet_uproc[]	= { 0x20, 'U', 'P', 'R', 'O', 'C' };

#define logg_packet(x,...) logg_develd("\t"x, __VA_ARGS__)
#define logg_packet_old(x,...) logg_develd_old("\t"x, __VA_ARGS__)
#define STDSF	"%s\n"
#define STDLF	"%s -> /%s\n"

// funktions
static bool empty_action(GCC_ATTR_UNUSED_PARAM(g2_connection_t *, connec), GCC_ATTR_UNUSED_PARAM(g2_packet_t *, source), GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	return false;
}

static bool handle_KHL(g2_connection_t *connec, g2_packet_t *source, struct norm_buff *target)
{
	g2_packet_t *packs;
	size_t num;
	bool ret_val = false;

	logg_develd("num: %u\tptr: 0x%p\n", source->num_child, (void *) source->children);
	
	for(num = source->num_child, packs = source->children; num; num--, packs++)
	{
		ret_val |= g2_packet_decide_spec(connec, target, &KHL_packet_dict, packs);

		//logg_packet_old("/KHL/%s -> No action\n", packs->type);
		//logg_packet_old("/KHL/%s -> Unknown, undecoded: %s\n", packs->type, to_match);
	}

	return false;
}

static bool handle_KHL_TS(g2_connection_t *connec, g2_packet_t *source, GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	time_t foreign_time;
	time_t local_time;
	foreign_time = local_time = time(NULL);

	if(4 == buffer_remaining(source->data_trunk) || 8 == buffer_remaining(source->data_trunk))
	{
		/* fill the upper bit of time_t if we need more than the recvd 32 bit */
		if(8 == sizeof(time_t) && 4 == buffer_remaining(source->data_trunk))
			foreign_time &= (time_t)0xFFFFFFFF00000000;
		else
			foreign_time = 0;

		/* the most commen case 32-bit time_t and little endian (all the Win-Clients) */
		if(4 == buffer_remaining(source->data_trunk) && !source->big_endian)
			foreign_time |= ((time_t)*buffer_start(source->data_trunk) & 0xFFu) |
				((((time_t)*(buffer_start(source->data_trunk)+1)) & 0xFFu) << 8) |
				((((time_t)*(buffer_start(source->data_trunk)+2)) & 0xFFu) << 16) |
				((((time_t)*(buffer_start(source->data_trunk)+3)) & 0xFFu) << 24);
		else
		{
			size_t shift = 0;

			if(source->big_endian)
			{
				if(4 == buffer_remaining(source->data_trunk))
					shift = 24;
				else
				{
					logg_packet(STDLF, "/KHL", "TS with 8 byte! Ola, a 64-bit OS?");
					/* Lets try too cludge it together, if we also have a 64-bit OS,
					 * everything will be fine, if not, we hopefully get the lower 32 bit,
					 * and if we don't test at the moment of overflow in 2013 (or when ever)
					 * it should work */
					shift = 56;
				}
			}
	
			for(; buffer_remaining(source->data_trunk); source->data_trunk.pos++)
			{
				foreign_time |= ((((time_t)*buffer_start(source->data_trunk)) & 0xFFu) << shift);

				if(!source->big_endian)
					shift += 8;
				else
					shift -= 8;
			}
		}
		logg_packet("/KHL/TS -> %lu\t%lu\n", foreign_time, local_time);
	}
	else
		logg_packet(STDLF, "/KHL/TS", "TS not 4 or 8 byte");

	connec->time_diff = difftime(foreign_time, local_time);
	return false;
}

static bool handle_LNI(g2_connection_t *connec, g2_packet_t *source, struct norm_buff *target)
{
	g2_packet_t *packs;
	size_t num;
	bool ret_val = false;
	

	for(num = source->num_child, packs = source->children; num; num--, packs++)
	{
		ret_val |= g2_packet_decide_spec(connec, target, &LNI_packet_dict, packs);
//		logg_packet_old("/LNI/%s -> No action\n", packs->type);
//		logg_packet_old("/LNI/%s -> Unknown, undecoded: %s\n", packs->type, to_match);
	}

				
	logg_packet_old("/LNI/%s -> data_length: %u data: \"%*s\"\n", packs->type, packs->data_length, packs->data_length, packs->data);
	return ret_val;
}

static bool handle_LNI_GU(g2_connection_t *connec, g2_packet_t *source, GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	logg_packet(STDSF, "/LNI/GU");

	if(sizeof(connec->guid) == buffer_remaining(source->data_trunk))
		memcpy(connec->guid, buffer_start(source->data_trunk), sizeof(connec->guid));
	else
		logg_packet(STDLF, "/LNI/GU", "GU not a valid GUID");
		
	return false;
}

static bool handle_LNI_NA(g2_connection_t *connec, g2_packet_t *source, GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	if(6 == buffer_remaining(source->data_trunk))
	{
		uint16_t	tmp_port;
		uint32_t tmp_addr =
			((((uint32_t)*buffer_start(source->data_trunk)) & 0xFFu) << 24) |
			((((uint32_t)*(buffer_start(source->data_trunk)+1)) & 0xFFu) << 16) |
			((((uint32_t)*(buffer_start(source->data_trunk)+2)) & 0xFFu) << 8) |
			(((uint32_t)*(buffer_start(source->data_trunk)+3)) & 0xFFu);
		
		connec->sent_addr.sin_addr.s_addr = htonl(tmp_addr);

		if(!source->big_endian)
			tmp_port =
			(((uint16_t)*(buffer_start(source->data_trunk)+4)) & 0xFFu) | ((((uint16_t)*(buffer_start(source->data_trunk)+5)) & 0xFFu) << 8) ;
		else
			tmp_port =
			((((uint16_t)*(buffer_start(source->data_trunk)+4)) & 0xFFu) << 8) | (((uint16_t)*(buffer_start(source->data_trunk)+5)) & 0xFFu) ;

		connec->sent_addr.sin_port = htons(tmp_port);

		logg_packet(STDSF, "/LNI/NA");

		{
			logg_packet_old("/LNI/NA:\t%s:%hu\n", inet_ntop(connec->af_type, &connec->sent_addr.sin_addr, char addr_buf[INET6_ADDRSTRLEN], sizeof(addr_buf)), ntohs(connec->sent_addr.sin_port));
		}
	}
	else if(18 == buffer_remaining(source->data_trunk))
		logg_packet(STDLF, "/LNI/NA", "NA could be an IPv6 address -> TODO");
	else
		logg_packet(STDLF, "/LNI/NA", "NA not an IPv4 address");
	
	return false;
}

static bool handle_LNI_V(g2_connection_t *connec, g2_packet_t *source, GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	size_t min_length = (buffer_remaining(source->data_trunk) < (sizeof(connec->vendor_code)-1) ?
		buffer_remaining(source->data_trunk) :
		sizeof(connec->vendor_code)-1);

	memcpy(connec->vendor_code, buffer_start(source->data_trunk), min_length);
	connec->vendor_code[min_length] = '\0';

	logg_packet(STDSF, "/LNI/V");
	
	return false;
}

static bool handle_PI(GCC_ATTR_UNUSED_PARAM(g2_connection_t *, connec), g2_packet_t *source, struct norm_buff *target)
{
	// simple /PI-packet
	if(!source->is_compound)
	{
		if(sizeof(packet_po) <= buffer_remaining(*target))
		{
			memcpy(buffer_start(*target), packet_po, sizeof(packet_po));
			target->pos += sizeof(packet_po);
			logg_packet(STDSF, "\t/PI");
		}
		else
			logg_packet(STDLF, "\t/PI", "sendbuffer full");

		return true;
	}

	logg_packet(STDLF, "\t/PI/x", "some child");
	return false;
}

// struct norm_buff *target GCC_ATTR_UNUSED
static inline bool handle_QHT_patch(g2_connection_t *connec, g2_packet_t *source, struct qhtable *ttable)
{
	size_t patch_length;
	uint8_t *tmp_ptr;
	const char *patch_txt;

	if(!ttable)
	{
		logg_packet(STDLF, "/QHT-patch", "initial patch without initial reset");
		connec->flags.dismissed = true;
		return false;
	}

	if(1 != *(buffer_start(source->data_trunk)+3))
	{
		logg_packet(STDLF, "/QHT-patch", "illegal bit-number");
		connec->flags.dismissed = true;
		goto qht_patch_end;
	}

	if(1 != *(buffer_start(source->data_trunk)+2) && 0 != *(buffer_start(source->data_trunk)+2))
	{
		logg_packet(STDLF, "/QHT-patch", "unknown compression");
		connec->flags.dismissed = true;
		goto qht_patch_end;
	}

	if(ttable->fragments)
	{
		if(((unsigned)*(buffer_start(source->data_trunk)+1) & 0xFFu) != ttable->last_frag_count)
		{
			logg_packet(STDLF, "/QHT-patch", "fragment-count changed!");
			connec->flags.dismissed = true;
			goto qht_patch_end;
		}

		if((*(buffer_start(source->data_trunk)+2) && !ttable->frag_compressed) || (!*(buffer_start(source->data_trunk)+2) && ttable->frag_compressed))
		{
			logg_packet(STDLF, "/QHT-patch", "compression changed!");
			connec->flags.dismissed = true;
			goto qht_patch_end;
		}

		if(((unsigned)*buffer_start(source->data_trunk) & 0xFFu) != (ttable->last_frag_no + 1))
		{
			logg_packet(STDLF, "/QHT-patch", "fragment missed");
			connec->flags.dismissed = true;
			goto qht_patch_end;
		}
	}
	else if(1 == *buffer_start(source->data_trunk) && !connec->flags.dismissed)
	{
		ttable->last_frag_count = *(buffer_start(source->data_trunk)+1) & 0xFFu; 
		ttable->frag_compressed = *(buffer_start(source->data_trunk)+2);
	}
	else
	{
		logg_packet(STDLF, "/QHT-patch", (!connec->flags.dismissed) ? "not starting at fragment 1" : "connection dismissed");
		connec->flags.dismissed = true;
		goto qht_patch_end;
	}

	patch_length = buffer_remaining(source->data_trunk) - 4;

	{
		size_t legal_length = ttable->data_length;

// TODO: If the patch is compressed, we are just guessing a max length
		if(ttable->frag_compressed)
			legal_length = ttable->data_length / 2;

		if((ttable->fragments_length + patch_length) > legal_length)
		{
			logg_packet(STDLF, "/QHT-patch", "patch to big");
			connec->flags.dismissed = true;
			goto qht_patch_end;
		}
	}

	ttable->last_frag_no = *buffer_start(source->data_trunk) & 0xFFu;
	
	tmp_ptr = realloc(ttable->fragments, ttable->fragments_length + patch_length);
	if(!tmp_ptr)
	{
		logg_errno(LOGF_DEBUG, "reallocating qht-fragments");
		connec->flags.dismissed = true;
		goto qht_patch_end;
	}
	ttable->fragments = tmp_ptr;

	memcpy(ttable->fragments + ttable->fragments_length, buffer_start(source->data_trunk)+4, patch_length);
	ttable->fragments_length += patch_length;

	if(ttable->last_frag_no != ttable->last_frag_count)
	{
		logg_packet(STDLF, "/QHT-patch", "patch recieved");
		return false;
	}

	// apply Patch
	if(ttable->frag_compressed)
	{
		z_stream patch_decoder;

		memset(&patch_decoder, 0, sizeof(patch_decoder));
// TODO: do something about this alloca, our BSD friends have little default task stacks (64k),
// allocating 128k will crash, so either:
// - raise the task stack size before creating the tasks
// - use a static buffer with a mutex
// - dynamically allocate it (from a buffer-cache?)
		tmp_ptr = alloca(ttable->data_length);

		patch_decoder.next_in = ttable->fragments;
		patch_decoder.avail_in = ttable->fragments_length;
		patch_decoder.next_out = tmp_ptr;
		patch_decoder.avail_out = ttable->data_length;

		if(Z_OK != inflateInit(&patch_decoder) || Z_STREAM_END != inflate(&patch_decoder, Z_SYNC_FLUSH))
		{
			logg_packet(STDLF, "/QHT-patch", "failure in compressed fragments");
			goto qht_patch_end;
		}

		patch_txt = "compressed patch applied";
	}
	else
	{
		tmp_ptr = ttable->fragments;
		patch_txt = "patch applied";
	}

	memxor(ttable->data, tmp_ptr, ttable->data_length);

	logg_packet(STDLF, "/QHT-patch", patch_txt);
	
qht_patch_end:
	if(ttable->fragments)
		free(ttable->fragments);

	ttable->fragments = NULL;
	ttable->fragments_length = 0;
	ttable->last_frag_no = 0;
	ttable->last_frag_count = 0;
	ttable->frag_compressed = false;
// TODO: Global QHT-needs-update flag (wich would need per connection locking)
	return false;
}

// struct norm_buff *target GCC_ATTR_UNUSED
static inline bool handle_QHT_reset(g2_connection_t *connec, g2_packet_t *source, struct qhtable *ttable)
{
	uint32_t qht_ent;
	size_t w_size;

	if(5 != buffer_remaining(source->data_trunk))
	{
		logg_packet(STDLF, "/QHT-reset", "to short");
		connec->flags.dismissed = true;
		return false;
	}
	
	if(1 != *(buffer_start(source->data_trunk)+4))
	{
		logg_packet(STDLF, "/QHT-reset", "illegal infinity");
		connec->flags.dismissed = true;
		return false;
	}

	if(!source->big_endian)
		qht_ent =	((uint32_t)*buffer_start(source->data_trunk) & 0xFFu) |
		((((uint32_t)*(buffer_start(source->data_trunk)+1)) & 0xFFu) << 8) |
		((((uint32_t)*(buffer_start(source->data_trunk)+2))  & 0xFFu) << 16) |
		((((uint32_t)*(buffer_start(source->data_trunk)+3))  & 0xFFu) << 24);
	else
		qht_ent =	((((uint32_t)*buffer_start(source->data_trunk)) & 0xFFu) << 24) |
			((((uint32_t)*(buffer_start(source->data_trunk)+1)) & 0xFFu) << 16) |
			((((uint32_t)*(buffer_start(source->data_trunk)+2)) & 0xFFu) << 8) |
			((uint32_t)*(buffer_start(source->data_trunk)+3) & 0xFFu);

	w_size = (size_t)(qht_ent / BITS_PER_CHAR);
	w_size += (qht_ent % BITS_PER_CHAR) ? 1u : 0u;
	if(!qht_ent || w_size > MAX_BYTES_QHT)
	{
		logg_packet(STDLF, "/QHT-reset", "illegal number of elements");
		connec->flags.dismissed = true;
		return false;
	}

	if(ttable)
	{
		if(ttable->data_length < w_size)
		{
			ttable = realloc(ttable, sizeof(*ttable) + w_size);
			if(!ttable)
			{
				logg_errno(LOGF_DEBUG, "reallocating qh-table");
				goto after_realloc_error;
			}
			connec->qht = ttable;
			ttable->data_length = w_size;
		}
	}
	else
	{
		ttable = malloc(sizeof(*ttable) + w_size);
		if(!ttable)
		{
			logg_errno(LOGF_DEBUG, "allocating qh-table");
			return false;
		}
		connec->qht = ttable;
		ttable->compressed = false;
		ttable->fragments = NULL;
		ttable->data_length = w_size;
	}

	ttable->entries = qht_ent;
	/* qht_ent is zero-checked, we need the power, not the index, so -1 */
	ttable->bits = flsst(qht_ent) - 1;

after_realloc_error:
	ttable = connec->qht;
	if(ttable->fragments)
	{
		free(ttable->fragments);
		ttable->fragments = NULL;
	}
	ttable->fragments_length = 0;
	ttable->last_frag_no = 0;
	ttable->last_frag_count = 0;
	ttable->frag_compressed = false;

	memset(ttable->data, -1, ttable->data_length);
// TODO: Global QHT-needs-update flag (wich would need per connection locking)
	logg_packet(STDSF, "/QHT-reset");

	return false;
}

static bool handle_QHT(g2_connection_t *connec, g2_packet_t *source, GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	struct qhtable *ttable = connec->qht;
	char tmp;

	if(!buffer_remaining(source->data_trunk))
	{
		logg_packet(STDLF, "/QHT", "without data?");
		return false;
	}

	tmp = *buffer_start(source->data_trunk);
	source->data_trunk.pos++;
	
	if(1 == tmp)
		return handle_QHT_patch(connec, source, ttable);
	else if(0 == tmp)
		return handle_QHT_reset(connec, source, ttable);
	else
		logg_packet(STDLF, "/QHT", "with unknown command");
	
	return false;
}

static bool handle_UPROC(GCC_ATTR_UNUSED_PARAM(g2_connection_t *, connec), GCC_ATTR_UNUSED_PARAM(g2_packet_t *, source), struct norm_buff *target)
{
	// /UPROC-packet, user-profile-request, if we want to and have an
	// answer, do it.
	logg_packet(STDSF, "/UPROC");
	if(server.settings.want_2_send_profile && packet_uprod)
	{
		if(packet_uprod_length <= buffer_remaining(*target))
		{
			memcpy(buffer_start(*target), packet_uprod, packet_uprod_length);
			target->pos += packet_uprod_length;
		}
		return true;
	}

	return false;
}

static bool handle_UPROD(GCC_ATTR_UNUSED_PARAM(g2_connection_t *, connec), GCC_ATTR_UNUSED_PARAM(g2_packet_t *, source), GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	if(source->is_compound)
	{
//		g2_packet_t sub_packet;

// TODO: write UPROD subdecoder
//		logg_packet("/UPROD/xxx -> TODO: subdecoder\n");
//		logg_packet("/UPROD/%s -> data\n\"%s\"\n", source->children->type, source->children->data);
	}
	else
		logg_packet(STDLF, "/UPROD", "no child?");

	return false;
}

// helper-funktions
inline void _g2_packet_free(g2_packet_t *to_free, int is_freeable)
{
	if(!to_free)
		return;

	if(to_free->child_is_freeable && to_free->num_child)
	{
		size_t i = to_free->num_child;
		g2_packet_t *tmp_ptr = to_free->children;

		for(; i; i--, tmp_ptr++)
			_g2_packet_free(tmp_ptr, false);

		free(to_free->children);
	}

	if(to_free->data_trunk_is_freeable)
		free(to_free->data_trunk.data);

	if(is_freeable)
		free(to_free);
}

inline void g2_packet_clean(g2_packet_t *to_clean)
{
	bool tmp_info = to_clean->data_trunk_is_freeable;

	if(to_clean->children && to_clean->child_is_freeable && to_clean->num_child)
	{
		size_t i = to_clean->num_child;
		g2_packet_t *tmp_ptr = to_clean->children;

		for(; i; i--, tmp_ptr++)
			_g2_packet_free(tmp_ptr, false);

		free(to_clean->children);
	}
	
	memset(to_clean, 0, offsetof(g2_packet_t, data_trunk));
	to_clean->children = NULL;
	buffer_clear(to_clean->data_trunk);
	to_clean->data_trunk_is_freeable = tmp_info;
}

static inline bool g2_packet_decide_spec(g2_connection_t *connec, struct norm_buff *target, const g2_p_type_t *work_type, g2_packet_t *packs)
{
	char *to_match = packs->type;
	bool ret_val = false;
	bool done = false;

	do
	{
		if(work_type->match == *to_match)
		{
			if(work_type->last)
			{
				done = true;
				if(work_type->work.action)
				{
					if(empty_action == work_type->work.action)
						logg_packet("*/%s\tC: %s -> ignored\n", packs->type, packs->is_compound ? "true" : "false");
					else
						logg_packet("*/%s\tC: %s\n", packs->type, packs->is_compound ? "true" : "false");
					ret_val |= work_type->work.action(connec, packs, target);
				}
				else
					logg_packet("/%s\tC: %s -> No action\n", packs->type, packs->is_compound ? "true" : "false");

				break;
			}
			else
				work_type = work_type->work.found;

			to_match++;
		}
		else
			work_type = work_type->next;
	} while(work_type);

	if(!done)
		logg_packet("/%s\tC: %s -> Unknown, undecoded: %s\n", packs->type, packs->is_compound ? "true" : "false", to_match);

	return ret_val;
}

inline bool g2_packet_decide(g2_connection_t *connec, struct norm_buff *target, const g2_p_type_t *work_type)
{
	return g2_packet_decide_spec(connec, target, work_type, connec->akt_packet);
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id: G2Packet.c,v 1.12 2004/12/18 18:06:13 redbully Exp redbully $";
// EOF
