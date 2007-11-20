/*
 * G2Packet.c
 * helper-functions for G2-packets
 *
 * Copyright (c) 2004,2007 Jan Seiffert
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
#include "G2PacketSerializer.h"
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

#define PT_ENT(m, n)	{.match = (m), .last = false, .term = false, .found = {.next = (n)}}
#define PT_ACT(a)	{.match = '\0', .last = true, .term = false, .found = {.action = (a)}}
#define PT_TERM	{.match = 0, .last = 0, .term = true, .found = {.next = NULL}}

/*
 * seventh type-char-layer
 */
// CRAWLA - anwser? not my buisines
static const g2_p_type_t packet_dict_CRAWLA0[] = { PT_ACT(empty_action_p), PT_TERM };
// CRAWLR
static const g2_p_type_t packet_dict_CRAWLR0[] = { PT_ACT(NULL), PT_TERM };

/*
 * sixth type-char-layer
 */
// UPROD
static const g2_p_type_t packet_dict_UPROD0[] = { PT_ACT(handle_UPROD), PT_TERM };
// UPROC
static const g2_p_type_t packet_dict_UPROC0[] = { PT_ACT(handle_UPROC), PT_TERM };
// CRAWLx
static const g2_p_type_t packet_dict_CRAWLx[] = {
	PT_ENT('A', packet_dict_CRAWLA0),
	PT_ENT('R', packet_dict_CRAWLR0),
	PT_TERM};
// G2CDC
static const g2_p_type_t packet_dict_G2CDC0[] = { PT_ACT(handle_G2CDC), PT_TERM };

/*
 * fith type-char-layer
 */
// UPROx
static const g2_p_type_t packet_dict_UPROx[] = {
	PT_ENT('D', packet_dict_UPROD0),
	PT_ENT('C', packet_dict_UPROC0),
	PT_TERM};
//CRAWx
static const g2_p_type_t packet_dict_CRAWL[] = { PT_ENT('L', packet_dict_CRAWLx), PT_TERM };
// G2CDx
static const g2_p_type_t packet_dict_G2CDC[] = { PT_ENT('C', packet_dict_G2CDC0), PT_TERM };

/*
 * fourth type-char-layer
 */
// KHL
static const g2_p_type_t packet_dict_KHL0[] = { PT_ACT(handle_KHL), PT_TERM };
// LNI
static const g2_p_type_t packet_dict_LNI0[] = { PT_ACT(handle_LNI), PT_TERM };
// QHT
static const g2_p_type_t packet_dict_QHT0[] = { PT_ACT(handle_QHT), PT_TERM };
// QKR
static const g2_p_type_t packet_dict_QKR0[] = { PT_ACT(NULL), PT_TERM };
// QKA
static const g2_p_type_t packet_dict_QKA0[] = { PT_ACT(NULL), PT_TERM };
// UPRx
static const g2_p_type_t packet_dict_UPRO[] = { PT_ENT('O', packet_dict_UPROx), PT_TERM };
// CRAx
static const g2_p_type_t packet_dict_CRAW[] = { PT_ENT('W', packet_dict_CRAWL), PT_TERM };
// G2Cx
static const g2_p_type_t packet_dict_G2CD[] = { PT_ENT('D', packet_dict_G2CDC), PT_TERM };

/*
 * third type-char-layer
 */
// KHx
static const g2_p_type_t packet_dict_KHL[] = { PT_ENT('L', packet_dict_KHL0), PT_TERM };
// LNx
static const g2_p_type_t packet_dict_LNI[] = { PT_ENT('I', packet_dict_LNI0), PT_TERM };
// PI
static const g2_p_type_t packet_dict_PI0[] = { PT_ACT(handle_PI), PT_TERM };
// PO
static const g2_p_type_t packet_dict_PO0[] = { PT_ACT(NULL), PT_TERM };
// Q2
static const g2_p_type_t packet_dict_Q20[] = { PT_ACT(NULL), PT_TERM };
// QHx
static const g2_p_type_t packet_dict_QHT[] = { PT_ENT('T', packet_dict_QHT0), PT_TERM };
// QKx
static const g2_p_type_t packet_dict_QKx[] = {
	PT_ENT('A', packet_dict_QKA0),
	PT_ENT('R', packet_dict_QKR0),
	PT_TERM};
// UPx
static const g2_p_type_t packet_dict_UPR[] = { PT_ENT('R', packet_dict_UPRO), PT_TERM };
// CRx
static const g2_p_type_t packet_dict_CRA[] = { PT_ENT('A', packet_dict_CRAW), PT_TERM };
// G2x
static const g2_p_type_t packet_dict_G2C[] = { PT_ENT('C', packet_dict_G2CD), PT_TERM };

/* 
 * second type-char-layer
 */
// Kx
static const g2_p_type_t packet_dict_KH[] = { PT_ENT('H', packet_dict_KHL), PT_TERM };
// Lx
static const g2_p_type_t packet_dict_LN[] = { PT_ENT('N', packet_dict_LNI), PT_TERM };
// Px
static const g2_p_type_t packet_dict_Px[] = {
		PT_ENT('I', packet_dict_PI0),
		PT_ENT('O', packet_dict_PO0),
		PT_TERM};
// Qx
static const g2_p_type_t packet_dict_Qx[] = {
		PT_ENT('H', packet_dict_QHT),
		PT_ENT('K', packet_dict_QKx),
		PT_ENT('2', packet_dict_Q20),
		PT_TERM};
// Ux
static const g2_p_type_t packet_dict_UP[] = { PT_ENT('P', packet_dict_UPR), PT_TERM };
// Cx
static const g2_p_type_t packet_dict_CR[] = { PT_ENT('R', packet_dict_CRA), PT_TERM };
// Gx
static const g2_p_type_t packet_dict_G2[] = { PT_ENT('2', packet_dict_G2C), PT_TERM };

/*
 * first type-char-layer
 */
const g2_p_type_t g2_packet_dict[] = {
		PT_ENT('K', packet_dict_KH),
		PT_ENT('L', packet_dict_LN),
		PT_ENT('P', packet_dict_Px),
		PT_ENT('Q', packet_dict_Qx),
		PT_ENT('U', packet_dict_UP),
		PT_ENT('C', packet_dict_CR),
		PT_ENT('G', packet_dict_G2),
		PT_TERM
};

/*
 * LNI-childs
 */
// third
// /LNI/HS
static const g2_p_type_t LNI_packet_dict_HS0[] = { PT_ACT(empty_action_p), PT_TERM };
// /LNI/GU
static const g2_p_type_t LNI_packet_dict_GU0[] = { PT_ACT(handle_LNI_GU), PT_TERM };
// /LNI/LS
static const g2_p_type_t LNI_packet_dict_LS0[] = { PT_ACT(empty_action_p), PT_TERM };
// /LNI/NA
static const g2_p_type_t LNI_packet_dict_NA0[] = { PT_ACT(handle_LNI_NA), PT_TERM };
// /LNI/QK
static const g2_p_type_t LNI_packet_dict_QK0[] = { PT_ACT(NULL), PT_TERM };

// second
// /LNI/Hx
static const g2_p_type_t LNI_packet_dict_HS[] = { PT_ENT('S', LNI_packet_dict_HS0), PT_TERM };
// /LNI/Gx
static const g2_p_type_t LNI_packet_dict_GU[] = { PT_ENT('U', LNI_packet_dict_GU0), PT_TERM };
// /LNI/Lx
static const g2_p_type_t LNI_packet_dict_LS[] = { PT_ENT('S', LNI_packet_dict_LS0), PT_TERM };
// /LNI/Nx
static const g2_p_type_t LNI_packet_dict_NA[] = { PT_ENT('A', LNI_packet_dict_NA0), PT_TERM };
// /LNI/Qx
static const g2_p_type_t LNI_packet_dict_QK[] = { PT_ENT('K', LNI_packet_dict_QK0), PT_TERM };
// /LNI/V
static const g2_p_type_t LNI_packet_dict_V0[] = { PT_ACT(handle_LNI_V), PT_TERM };

// first
static const g2_p_type_t LNI_packet_dict[] = {
	PT_ENT('G', LNI_packet_dict_GU),
	PT_ENT('H', LNI_packet_dict_HS),
	PT_ENT('L', LNI_packet_dict_LS),
	PT_ENT('N', LNI_packet_dict_NA),
	PT_ENT('Q', LNI_packet_dict_QK),
	PT_ENT('V', LNI_packet_dict_V0),
	PT_TERM
};


/*
 * KHL-childs
 */
// third
// /KHL/TS
static const g2_p_type_t KHL_packet_dict_TS0[] = { PT_ACT(handle_KHL_TS), PT_TERM };
// /KHL/NH
static const g2_p_type_t KHL_packet_dict_NH0[] = { PT_ACT(NULL), PT_TERM };
// /KHL/CH
static const g2_p_type_t KHL_packet_dict_CH0[] = { PT_ACT(NULL), PT_TERM };

// second
// /KHL/Tx
static const g2_p_type_t KHL_packet_dict_TS[] = { PT_ENT('S', KHL_packet_dict_TS0), PT_TERM };
// /KHL/Nx
static const g2_p_type_t KHL_packet_dict_NH[] = { PT_ENT('H', KHL_packet_dict_NH0), PT_TERM };
// /KHL/Cx
static const g2_p_type_t KHL_packet_dict_CH[] = { PT_ENT('H', KHL_packet_dict_CH0), PT_TERM };

// first
static const g2_p_type_t KHL_packet_dict[] = { 
	PT_ENT('C', KHL_packet_dict_CH),
	PT_ENT('N', KHL_packet_dict_NH),
	PT_ENT('T', KHL_packet_dict_TS),
	PT_TERM
};


/*
 * CRAWLR-childs
 */
// seventh
// /CRAWLR/RLEAF
static const g2_p_type_t CRAWLR_packet_dict_RLEAF0[] = { PT_ACT(NULL), PT_TERM };
// /CRAWLR/RNAME
static const g2_p_type_t CRAWLR_packet_dict_RNAME0[] = { PT_ACT(NULL), PT_TERM };

// sixth
// /CRAWLR/RLEAx
static const g2_p_type_t CRAWLR_packet_dict_RLEAF[] = { PT_ENT('F', CRAWLR_packet_dict_RLEAF0), PT_TERM };
// /CRAWLR/RNAMx
static const g2_p_type_t CRAWLR_packet_dict_RNAME[] = { PT_ENT('E', CRAWLR_packet_dict_RNAME0), PT_TERM };
// /CRAWLR/RGPS
static const g2_p_type_t CRAWLR_packet_dict_RGPS0[] = { PT_ACT(NULL), PT_TERM };
// /CRAWLR/REXT
static const g2_p_type_t CRAWLR_packet_dict_REXT0[] = { PT_ACT(NULL), PT_TERM };

// fourth
// /CRAWLR/RLEx
static const g2_p_type_t CRAWLR_packet_dict_RLEA[] = { PT_ENT('A', CRAWLR_packet_dict_RLEAF), PT_TERM };
// /CRAWLR/RNAx
static const g2_p_type_t CRAWLR_packet_dict_RNAM[] = { PT_ENT('M', CRAWLR_packet_dict_RNAME), PT_TERM };
// /CRAWLR/RGPx
static const g2_p_type_t CRAWLR_packet_dict_RGPS[] = { PT_ENT('S', CRAWLR_packet_dict_RGPS0), PT_TERM };
// /CRAWLR/REXx
static const g2_p_type_t CRAWLR_packet_dict_REXT[] = { PT_ENT('T', CRAWLR_packet_dict_REXT0), PT_TERM };

// third
// /CRAWLR/RLx
static const g2_p_type_t CRAWLR_packet_dict_RLE[] = { PT_ENT('E', CRAWLR_packet_dict_RLEA), PT_TERM };
// /CRAWLR/RNx
static const g2_p_type_t CRAWLR_packet_dict_RNA[] = { PT_ENT('A', CRAWLR_packet_dict_RNAM), PT_TERM };
// /CRAWLR/RGx
static const g2_p_type_t CRAWLR_packet_dict_RGP[] = { PT_ENT('P', CRAWLR_packet_dict_RGPS), PT_TERM };
// /CRAWLR/REx
static const g2_p_type_t CRAWLR_packet_dict_REX[] = { PT_ENT('X', CRAWLR_packet_dict_REXT), PT_TERM };

// second
// /CRAWLR/Rx
static const g2_p_type_t CRAWLR_packet_dict_Rx[] = {
	PT_ENT('E', CRAWLR_packet_dict_REX),
	PT_ENT('G', CRAWLR_packet_dict_RGP),
	PT_ENT('N', CRAWLR_packet_dict_RNA),
	PT_ENT('L', CRAWLR_packet_dict_RLE),
	PT_TERM
};

// first
static const g2_p_type_t CRAWLR_packet_dict[] = { PT_ENT('R', CRAWLR_packet_dict_Rx), PT_TERM };



/*
 * prebuild packets
 */
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
	bool ret_val = false, keep_decoding;

	do
	{
		g2_packet_t child_p;
		child_p.more_bytes_needed = false;
		child_p.packet_decode = CHECK_CONTROLL_BYTE;
		keep_decoding = g2_packet_decode_from_packet(source, &child_p, 0);
		if(!keep_decoding)
		{
			logg_packet(STDLF, "KHL", "broken child");
			connec->flags.dismissed = true;
			break;
		}
		if(child_p.packet_decode == DECODE_FINISHED)
			ret_val |= g2_packet_decide_spec(connec, target, KHL_packet_dict, &child_p);
//		source->num_child++; // put within if
	} while(keep_decoding && source->packet_decode != DECODE_FINISHED);

	return ret_val;
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
		logg_packet("/KHL/TS -> %lu\t%lu\n", (unsigned long)foreign_time, (unsigned long)local_time);
	}
	else
		logg_packet(STDLF, "/KHL/TS", "TS not 4 or 8 byte");

	connec->time_diff = difftime(foreign_time, local_time);
	return false;
}

static bool handle_LNI(g2_connection_t *connec, g2_packet_t *source, struct norm_buff *target)
{
	bool ret_val = false, keep_decoding;

	do
	{
		g2_packet_t child_p;
		child_p.more_bytes_needed = false;
		child_p.packet_decode = CHECK_CONTROLL_BYTE;
		keep_decoding = g2_packet_decode_from_packet(source, &child_p, 0);
		if(!keep_decoding)
		{
			logg_packet(STDLF, "LNI", "broken child");
			connec->flags.dismissed = true;
			break;
		}
		if(child_p.packet_decode == DECODE_FINISHED)
			ret_val |= g2_packet_decide_spec(connec, target, LNI_packet_dict, &child_p);
//		source->num_child++; // put within if
	} while(keep_decoding && source->packet_decode != DECODE_FINISHED);

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
		/* 
		 * the addr comes in network byteorder over the wire, when we only load and store it,
		 * result should be in network byteorder again.
		 */
		get_unaligned(connec->sent_addr.sin_addr.s_addr, (uint32_t *) buffer_start(source->data_trunk));

		/* load port and fix it for those, who sent it wrong way round */
		get_unaligned_endian(tmp_port, (uint16_t *) (buffer_start(source->data_trunk)+4), source->big_endian);
		connec->sent_addr.sin_port = tmp_port;

//		logg_packet(STDSF, "/LNI/NA");

		{
			char addr_buf[INET6_ADDRSTRLEN];
			logg_packet("/LNI/NA:\t%s:%hu\n", inet_ntop(connec->af_type, &connec->sent_addr.sin_addr, addr_buf, sizeof(addr_buf)), ntohs(connec->sent_addr.sin_port));
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

	logg_packet(STDLF, "/LNI/V", connec->vendor_code);
	
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
			logg_packet_old(STDSF, "\t/PI");
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

	get_unaligned_endian(qht_ent, (uint32_t *)buffer_start(source->data_trunk), source->big_endian);
	
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
	logg_packet_old(STDSF, "/UPROC");
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
				if(work_type->found.action)
				{
					if(empty_action_p == work_type->found.action)
						logg_packet("*/%s\tC: %s -> ignored\n", packs->type, packs->is_compound ? "true" : "false");
					else
						logg_packet("*/%s\tC: %s\n", packs->type, packs->is_compound ? "true" : "false");
					ret_val |= work_type->found.action(connec, packs, target);
				}
				else
					logg_packet("*/%s\tC: %s -> No action\n", packs->type, packs->is_compound ? "true" : "false");

				break;
			}
			else
				work_type = work_type->found.next;

			to_match++;
		}
		else
			work_type++;
	} while(!work_type->term);

	if(!done)
		logg_packet("*/%s\tC: %s -> Unknown, undecoded: %s\n", packs->type, packs->is_compound ? "true" : "false", to_match);

	return ret_val;
}

inline bool g2_packet_decide(g2_connection_t *connec, struct norm_buff *target, const g2_p_type_t *work_type)
{
	return g2_packet_decide_spec(connec, target, work_type, connec->akt_packet);
}

static char const rcsid_p[] GCC_ATTR_USED_VAR = "$Id: G2Packet.c,v 1.12 2004/12/18 18:06:13 redbully Exp redbully $";
// EOF
