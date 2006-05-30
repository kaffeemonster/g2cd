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
#include <time.h>
#include <dlfcn.h>
// other
#include "other.h"
// Own includes
#define _G2PACKET_C
#define _NEED_G2_P_TYPE
#include "G2Packet.h"
#include "G2QHT.h"
#include "G2MainServer.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"

// Internal Prototypes
static bool empty_action_p(g2_connection_t *, g2_packet_t *, struct norm_buff *);
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
static bool handle_G2CDC(g2_connection_t *, g2_packet_t *, struct norm_buff *);

static inline bool g2_packet_decide_spec(g2_connection_t *, struct norm_buff *, const g2_p_type_t *, g2_packet_t *);

/*
 * seventh type-char-layer
 */
// CRAWLA - anwser? not my buisines
static const g2_p_type_t packet_dict_CRAWLA0 = { NULL, {.action = &empty_action_p}, '\0', true};
// CRAWLR
static const g2_p_type_t packet_dict_CRAWLR0 = { NULL, {.action = NULL}, '\0', true};

/*
 * sixth type-char-layer
 */
// UPROD
static const g2_p_type_t packet_dict_UPROD0 = { NULL, {.action = &handle_UPROD}, '\0', true };
// UPROC
static const g2_p_type_t packet_dict_UPROC0 = { NULL, {.action = &handle_UPROC}, '\0', true };
// CRAWLx
static const g2_p_type_t packet_dict_CRAWLA = { NULL, {.found = &packet_dict_CRAWLA0}, 'A', false};
static const g2_p_type_t packet_dict_CRAWLR = { &packet_dict_CRAWLA, {.found = &packet_dict_CRAWLR0}, 'R', false};
// G2CDC
static const g2_p_type_t packet_dict_G2CDC0 = { NULL, {.action = &handle_G2CDC}, '\0', true };

/*
 * fith type-char-layer
 */
// UPROx
static const g2_p_type_t packet_dict_UPROD = { NULL, {.found = &packet_dict_UPROD0}, 'D', false };
static const g2_p_type_t packet_dict_UPROC = { &packet_dict_UPROD, {.found = &packet_dict_UPROC0}, 'C', false };
//CRAWx
static const g2_p_type_t packet_dict_CRAWL = { NULL, {.found = &packet_dict_CRAWLR}, 'L', false};
// G2CDx
static const g2_p_type_t packet_dict_G2CDC = { NULL, {.found = &packet_dict_G2CDC0}, 'C', false };

/*
 * fourth type-char-layer
 */
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
// CRAx
static const g2_p_type_t packet_dict_CRAW = { NULL, {.found = &packet_dict_CRAWL}, 'W', false };
// G2Cx
static const g2_p_type_t packet_dict_G2CD = { NULL, {.found = &packet_dict_G2CDC}, 'D', false };

/*
 * third type-char-layer
 */
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
// CRx
static const g2_p_type_t packet_dict_CRA = { NULL, {.found = &packet_dict_CRAW}, 'A', false };
// G2x
static const g2_p_type_t packet_dict_G2C = { NULL, {.found = &packet_dict_G2CD}, 'C', false };

/* 
 * second type-char-layer
 */
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
// Cx
static const g2_p_type_t packet_dict_CR = { NULL, {.found = &packet_dict_CRA}, 'R', false};
// Gx
static const g2_p_type_t packet_dict_G2 = { NULL, {.found = &packet_dict_G2C}, '2', false };

// first type-char-layer
static const g2_p_type_t packet_dict_G = { NULL, {.found = &packet_dict_G2}, 'G', false };
static const g2_p_type_t packet_dict_C = { &packet_dict_G, {.found = &packet_dict_CR}, 'C', false };
static const g2_p_type_t packet_dict_U = { &packet_dict_C, {.found = &packet_dict_UP}, 'U', false };
static const g2_p_type_t packet_dict_Q = { &packet_dict_U, {.found = &packet_dict_Q2}, 'Q', false };
static const g2_p_type_t packet_dict_P = { &packet_dict_Q, {.found = &packet_dict_PI}, 'P', false };
static const g2_p_type_t packet_dict_L = { &packet_dict_P, {.found = &packet_dict_LN}, 'L', false };
const g2_p_type_t g2_packet_dict = { &packet_dict_L, {.found = &packet_dict_KH}, 'K', false }; //packet_dict_K

// LNI-childs
// third
// /LNI/HS
static const g2_p_type_t LNI_packet_dict_HS0 = { NULL, {.action = &empty_action_p}, '\0', true };
// /LNI/GU
static const g2_p_type_t LNI_packet_dict_GU0 = { NULL, {.action = &handle_LNI_GU}, '\0', true };
// /LNI/LS
static const g2_p_type_t LNI_packet_dict_LS0 = { NULL, {.action = &empty_action_p}, '\0', true };
// /LNI/NA
static const g2_p_type_t LNI_packet_dict_NA0 = { NULL, {.action = &handle_LNI_NA}, '\0', true };
// /LNI/QK
static const g2_p_type_t LNI_packet_dict_QK0 = { NULL, {.action = &empty_action_p}, '\0', true };

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


// CRAWLR-childs
// seventh
// /CRAWLR/RLEAF
static const g2_p_type_t CRAWLR_packet_dict_RLEAF0 = { NULL, {.action = NULL}, '\0', true };
// /CRAWLR/RNAME
static const g2_p_type_t CRAWLR_packet_dict_RNAME0 = { NULL, {.action = NULL}, '\0', true };

// sixth
// /CRAWLR/RLEAx
static const g2_p_type_t CRAWLR_packet_dict_RLEAF = { NULL, {.found = &CRAWLR_packet_dict_RLEAF0}, 'F', false };
// /CRAWLR/RNAMx
static const g2_p_type_t CRAWLR_packet_dict_RNAME = { NULL, {.found = &CRAWLR_packet_dict_RNAME0}, 'E', false };
// /CRAWLR/RGPS
static const g2_p_type_t CRAWLR_packet_dict_RGPS0 = { NULL, {.action = NULL}, '\0', true };
// /CRAWLR/REXT
static const g2_p_type_t CRAWLR_packet_dict_REXT0 = { NULL, {.action = NULL}, '\0', true };

// fourth
// /CRAWLR/RLEx
static const g2_p_type_t CRAWLR_packet_dict_RLEA = { NULL, {.found = &CRAWLR_packet_dict_RLEAF}, 'A', false };
// /CRAWLR/RNAx
static const g2_p_type_t CRAWLR_packet_dict_RNAM = { NULL, {.found = &CRAWLR_packet_dict_RNAME}, 'M', false };
// /CRAWLR/RGPx
static const g2_p_type_t CRAWLR_packet_dict_RGPS = { NULL, {.found = &CRAWLR_packet_dict_RGPS0}, 'S', false };
// /CRAWLR/REXx
static const g2_p_type_t CRAWLR_packet_dict_REXT = { NULL, {.found = &CRAWLR_packet_dict_REXT0}, 'T', false };

// third
// /CRAWLR/RLx
static const g2_p_type_t CRAWLR_packet_dict_RLE = { NULL, {.found = &CRAWLR_packet_dict_RLEA}, 'E', false };
// /CRAWLR/RNx
static const g2_p_type_t CRAWLR_packet_dict_RNA = { NULL, {.found = &CRAWLR_packet_dict_RNAM}, 'A', false };
// /CRAWLR/RGx
static const g2_p_type_t CRAWLR_packet_dict_RGP = { NULL, {.found = &CRAWLR_packet_dict_RGPS}, 'P', false };
// /CRAWLR/REx
static const g2_p_type_t CRAWLR_packet_dict_REX = { NULL, {.found = &CRAWLR_packet_dict_REXT}, 'X', false };

// second
// /CRAWLR/Rx
static const g2_p_type_t CRAWLR_packet_dict_RL = { NULL, {.found = &CRAWLR_packet_dict_RLE}, 'L', false };
static const g2_p_type_t CRAWLR_packet_dict_RN = { &CRAWLR_packet_dict_RL, {.found = &CRAWLR_packet_dict_RNA}, 'N', false };
static const g2_p_type_t CRAWLR_packet_dict_RG = { &CRAWLR_packet_dict_RN, {.found = &CRAWLR_packet_dict_RGP}, 'G', false };
static const g2_p_type_t CRAWLR_packet_dict_RE = { &CRAWLR_packet_dict_RG, {.found = &CRAWLR_packet_dict_REX}, 'E', false };

// first
static const g2_p_type_t CRAWLR_packet_dict = { NULL, {.found = &CRAWLR_packet_dict_RE}, 'R', false };


// prebuild packets
static const char packet_po[]		= { 0x08, 'P', 'O', };
static const char packet_uproc[]	= { 0x20, 'U', 'P', 'R', 'O', 'C' };

#define logg_packet(x,...) logg_develd("\t"x, __VA_ARGS__)
#define logg_packet_old(x,...) logg_develd_old("\t"x, __VA_ARGS__)
#define STDSF	"%s\n"
#define STDLF	"%s -> /%s\n"

// funktions
static bool empty_action_p(GCC_ATTR_UNUSED_PARAM(g2_connection_t *, connec), GCC_ATTR_UNUSED_PARAM(g2_packet_t *, source), GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
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

	logg_develd("num: %lu\tchild: %p\n", (unsigned long) source->num_child, (void *) source->children);

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
static inline bool handle_QHT_patch(g2_connection_t *connec, g2_packet_t *source)
{
	struct qht_fragment frag;
	int ret_val;
	const char *patch_txt;

	if(!connec->qht)
	{
		logg_packet(STDLF, "/QHT-patch", "initial patch without initial reset");
		connec->flags.dismissed = true;
		return false;
	}

	if(5 > buffer_remaining(source->data_trunk))
	{
		logg_packet(STDLF, "/QHT-patch", "to short");
		connec->flags.dismissed = true;
		goto qht_patch_end;
	}

	if(1 != *(buffer_start(source->data_trunk)+3))
	{
		logg_packet(STDLF, "/QHT-patch", "illegal bit-number");
		connec->flags.dismissed = true;
		goto qht_patch_end;
	}

	frag.nr         = ((unsigned)*buffer_start(source->data_trunk)) & 0x00FF;
	frag.count      = ((unsigned)*(buffer_start(source->data_trunk)+1)) & 0x00FF;
	frag.compressed = ((unsigned)*(buffer_start(source->data_trunk)+2)) & 0x00FF;
	frag.data       = (uint8_t *)buffer_start(source->data_trunk)+4;
	frag.length     = buffer_remaining(source->data_trunk) - 4;

	if(!connec->flags.dismissed)
		ret_val = g2_qht_add_frag(connec->qht, &frag);
	else
	{
		logg_packet(STDLF, "/QHT-patch", "connection dissmissed");
		ret_val = -1;
	}

	if(0 == ret_val) /* patch io, but need more*/
	{
		logg_packet(STDLF, "/QHT-patch", "patch recieved");
		return false;	
	}
	else if(0 > ret_val) /* patch nio */
	{
		connec->flags.dismissed = true;
		goto qht_patch_end;
	}
	/*patch io and complete*/
	patch_txt = g2_qht_patch(&connec->qht, &connec->qht->fragments);
	logg_packet(STDLF, "/QHT-patch", patch_txt ? patch_txt : "some error while appling");
qht_patch_end:
	g2_qht_frag_clean(&connec->qht->fragments);
// TODO: Global QHT-needs-update flag (wich would need per connection locking)
	return false;
}

// struct norm_buff *target GCC_ATTR_UNUSED
static inline bool handle_QHT_reset(g2_connection_t *connec, g2_packet_t *source)
{
	uint32_t qht_ent;

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

	if(g2_qht_reset(&connec->qht, qht_ent))
		connec->flags.dismissed = true;

	logg_packet(STDSF, "/QHT-reset");
	return false;
}

static bool handle_QHT(g2_connection_t *connec, g2_packet_t *source, GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	char tmp;

	if(!buffer_remaining(source->data_trunk))
	{
		logg_packet(STDLF, "/QHT", "without data?");
		return false;
	}

	tmp = *buffer_start(source->data_trunk);
	source->data_trunk.pos++;
	
	if(1 == tmp)
		return handle_QHT_patch(connec, source);
	else if(0 == tmp)
		return handle_QHT_reset(connec, source);
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

static bool handle_G2CDC(GCC_ATTR_UNUSED_PARAM(g2_connection_t *, connec), GCC_ATTR_UNUSED_PARAM(g2_packet_t *, source), GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	static void *handle;
	const struct s_data
	{
		const unsigned long len;
		const char *data;
	} *s_data;
	
	if(!handle)
	{
		if(!(handle = dlopen(NULL, RTLD_LAZY)))
			return false;
	}

	(void) dlerror();
	s_data = dlsym(handle, "s_data");
	if(dlerror())
		return false;
	

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
					if(empty_action_p == work_type->work.action)
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

static char const rcsid_p[] GCC_ATTR_USED_VAR = "$Id: G2Packet.c,v 1.12 2004/12/18 18:06:13 redbully Exp redbully $";
// EOF
