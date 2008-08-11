/*
 * G2Packet.c
 * helper-functions for G2-packets
 *
 * Copyright (c) 2004 - 2008 Jan Seiffert
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
# include "config.h"
#endif
/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#ifdef HAVE_DLOPEN
# include <dlfcn.h>
#endif
/* other */
#include "other.h"
/* own includes */
#define _G2PACKET_C
#define _NEED_G2_P_TYPE
#include "G2Packet.h"
#include "G2PacketSerializer.h"
#include "G2QHT.h"
#include "G2MainServer.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"

/*
 * Internal Prototypes
 */
static bool empty_action_p(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool unimpl_action_p(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_KHL(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_KHL_TS(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_LNI(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_LNI_HS(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_LNI_NA(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_LNI_GU(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_LNI_V(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_PI(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_QHT(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_HAW(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_UPROC(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_UPROD(g2_connection_t *, g2_packet_t *, struct norm_buff *);
static bool handle_G2CDC(g2_connection_t *, g2_packet_t *, struct norm_buff *);

/*
 * Packet dicts
 */
/* main dict */
const g2_ptype_action_func g2_packet_dict[PT_MAXIMUM] =
{
	[PT_CRAWLA] = empty_action_p,
	[PT_CRAWLR] = unimpl_action_p,
	[PT_G2CDC ] = handle_G2CDC,
	[PT_KHL   ] = handle_KHL,
	[PT_LNI   ] = handle_LNI,
	[PT_HAW   ] = handle_HAW,
	[PT_PI    ] = handle_PI,
	[PT_PO    ] = unimpl_action_p,
	[PT_Q2    ] = unimpl_action_p,
	[PT_QHT   ] = handle_QHT,
	[PT_QKR   ] = unimpl_action_p,
	[PT_QKA   ] = unimpl_action_p,
	[PT_UPROC ] = handle_UPROC,
	[PT_UPROD ] = handle_UPROD,
};

/* LNI-childs */
static const g2_ptype_action_func LNI_packet_dict[PT_MAXIMUM] =
{
	[PT_FW] = unimpl_action_p,
	[PT_GU] = handle_LNI_GU,
	[PT_HS] = handle_LNI_HS,
	[PT_LS] = empty_action_p,
	[PT_NA] = handle_LNI_NA,
	[PT_QK] = unimpl_action_p,
	[PT_V ] = handle_LNI_V,
};

/* KHL-childs */
static const g2_ptype_action_func KHL_packet_dict[PT_MAXIMUM] =
{
	[PT_TS] = handle_KHL_TS,
	[PT_NH] = unimpl_action_p,
	[PT_CH] = unimpl_action_p,
};

/* CRAWLR-childs */
static const g2_ptype_action_func CRAWLR_packet_dict[PT_MAXIMUM] =
{
	[PT_REXT ] = unimpl_action_p,
	[PT_RGPS ] = unimpl_action_p,
	[PT_RLEAF] = unimpl_action_p,
	[PT_RNAME] = unimpl_action_p,
};

/* HAW-childs */
static const g2_ptype_action_func HAW_packet_dict[PT_MAXIMUM] =
{
	[PT_HS] = unimpl_action_p,
	[PT_NA] = unimpl_action_p,
	[PT_V ] = unimpl_action_p,
};

#define ENUM_CMD(x) str_it(x)
const char const g2_ptype_names[][8] = 
{
	G2_PACKET_TYPES
};
#undef ENUM_CMD

/*
 * packet typer
 *
 */
#define PT_TERM \
	{ .c = 0, .u = { .t = PT_UNKNOWN }}
#define PT_END(x) \
	{ .c = '\0', .u = { .t = x }}
#define PT_NEXT(x, y) \
	{ .c = x, .u = { .d = y}}
static const struct 
{
	const char c;
	union
	{
		const enum g2_ptype t;
		const unsigned char d;
	} u;
} g2_ptype_state_table[] =
{
	/* Align entry points on even numbers */
/*	      CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC */
	[  0] =	PT_NEXT('H', 2), /* CH */
	[  1] =	PT_TERM,
	[  2] =		PT_END(PT_CH), /* CH0 */
/*	   3  FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF */
	[  4] =	PT_NEXT('W', 2), /* FW */
	[  5] =	PT_TERM,
	[  6] =		PT_END(PT_FW), /* FW0 */
/*	   7  GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG */
	[  8] =	PT_NEXT('U', 3), /* GU */
	[  9] =	PT_NEXT('2', 4), /* G2 */
	[ 10] =	PT_TERM,
	[ 11] =		PT_END(PT_GU), /* GU0 */
	[ 12] =		PT_TERM,
	[ 13] =		PT_NEXT('C', 2), /* G2C */
	[ 14] =		PT_TERM,
	[ 15] =			PT_NEXT('D', 2), /* G2CD */
	[ 16] =			PT_TERM,
	[ 17] =				PT_NEXT('C', 2), /* G2CDC */
	[ 18] =				PT_TERM,
	[ 19] =					PT_END(PT_G2CDC), /* G2CDC0 */
	[ 20] =					PT_TERM,
/*	  21  HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH */
	[ 22] =	PT_NEXT('S', 3), /* HS */
	[ 23] =	PT_NEXT('A', 4), /* HA */
	[ 24] =	PT_TERM,
	[ 25] =		PT_END(PT_HS), /* HS0 */
	[ 26] =		PT_TERM,
	[ 27] =		PT_NEXT('W', 2), /* HAW */
	[ 28] =		PT_TERM,
	[ 29] =			PT_END(PT_HAW), /* HAW0 */
	[ 30] =			PT_TERM,
/*	  31  KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK */
	[ 32] =	PT_NEXT('H', 2), /* KH */
	[ 33] =	PT_TERM,
	[ 34] =		PT_NEXT('L', 2), /* KHL */
	[ 35] =		PT_TERM,
	[ 36] =			PT_END(PT_KHL), /* KHL0 */
/*	  37  LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL */
	[ 38] =	PT_NEXT('N', 3), /* LN */
	[ 39] =	PT_NEXT('S', 6), /* LS */
	[ 40] =	PT_TERM,
	[ 41] =		PT_NEXT('I', 2), /* LNI */
	[ 42] =		PT_TERM,
	[ 43] =			PT_END(PT_LNI), /* LNI0 */
	[ 44] =			PT_TERM,
	[ 45] =		PT_END(PT_LS), /* LS0 */
	[ 46] =		PT_TERM,
/*	  47  NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN */
	[ 48] =	PT_NEXT('A', 3), /* NA */
	[ 49] =	PT_NEXT('H', 4), /* NH */
	[ 50] =	PT_TERM,
	[ 51] =		PT_END(PT_NA), /* NA0 */
	[ 52] =		PT_TERM,
	[ 53] =		PT_END(PT_NH), /* NH0 */
	[ 54] =		PT_TERM,
/*	  55  PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP */
	[ 56] =	PT_NEXT('I', 3), /* PI */
	[ 57] =	PT_NEXT('O', 4), /* PO */
	[ 58] =	PT_TERM,
	[ 59] =		PT_END(PT_PI), /* PI0 */
	[ 60] =		PT_TERM,
	[ 61] =		PT_END(PT_PO), /* PO0 */
	[ 62] =		PT_TERM,
/*	  63  QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ */
	[ 64] =	PT_NEXT('2', 4), /* Q2 */
	[ 65] =	PT_NEXT('H', 5), /* QH */
	[ 66] =	PT_NEXT('K', 8), /* QK */
	[ 67] =	PT_TERM,
	[ 68] =		PT_END(PT_Q2), /* Q20 */
	[ 69] =		PT_TERM,
	[ 70] =		PT_NEXT('T', 2), /* QHT */
	[ 71] =		PT_TERM,
	[ 72] =			PT_END(PT_QHT), /* QHT0 */
	[ 73] =			PT_TERM,
	[ 74] =		PT_END(PT_QK), /* QK0 */
	[ 75]	=		PT_NEXT('A', 3), /* QKA */
	[ 76]	=		PT_NEXT('R', 4), /* QKR */
	[ 77] =		PT_TERM,
	[ 78] =			PT_END(PT_QKA), /* QKA0 */
	[ 79] =			PT_TERM,
	[ 80] =			PT_END(PT_QKR), /* QKR0 */
/*	  81  RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR */
	[ 82] =	PT_NEXT('E', 5), /* RE */
	[ 83] =	PT_NEXT('G', 6), /* RG */
	[ 84]	=	PT_NEXT('L', 7), /* RL */
	[ 85]	=	PT_NEXT('N', 8), /* RN */
	[ 86] =	PT_TERM,
	[ 87] =		PT_NEXT('X', 8), /* REX */
	[ 88] =		PT_TERM,
	[ 89] =		PT_NEXT('P', 8), /* RGP */
	[ 90] =		PT_TERM,
	[ 91] =		PT_NEXT('E', 8), /* RLE */
	[ 92] =		PT_TERM,
	[ 93] =		PT_NEXT('A', 8), /* RNA */
	[ 94]	=		PT_TERM,
	[ 95]	=			PT_NEXT('T', 8), /* REXT */
	[ 96] =			PT_TERM,
	[ 97] =			PT_NEXT('S', 8), /* RGPS */
	[ 98] =			PT_TERM,
	[ 99] =			PT_NEXT('A', 8), /* RLEA */
	[100] =			PT_TERM,
	[101] =			PT_NEXT('M', 8), /* RNAM */
	[102] =			PT_TERM,
	[103] =				PT_END(PT_REXT), /* REXT0 */
	[104]	=				PT_TERM,
	[105]	=				PT_END(PT_RGPS), /* RGPS0 */
	[106] =				PT_TERM,
	[107] =				PT_NEXT('F', 4), /* RLEAF */
	[108] =				PT_TERM,
	[109]	=				PT_NEXT('E', 4), /* RNAME */
	[110]	=				PT_TERM,
	[111] =					PT_END(PT_RLEAF), /* RLEAF0 */
	[112] =					PT_TERM,
	[113] =					PT_END(PT_RNAME), /* RNAME0 */
	[114] =					PT_TERM,
/*	 115  TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT */
	[116] =	PT_NEXT('S', 2), /* TS */
	[117] =	PT_TERM,
	[118] =		PT_END(PT_TS), /* TS0 */
/*	 119  UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU */
	[120] =	PT_NEXT('P', 2), /* UP */
	[121] =	PT_TERM,
	[122] =		PT_NEXT('R', 2), /* UPR */
	[123] =		PT_TERM,
	[124] =				PT_NEXT('O', 2), /* UPRO */
	[125] =				PT_TERM,
	[126] =					PT_NEXT('C', 3), /* UPROC */
	[127] = 					PT_NEXT('D', 4), /* UPROD */
	[128]	=					PT_TERM,
	[129]	=						PT_END(PT_UPROC), /* UPROC0 */
	[130] =						PT_TERM,
	[131] =						PT_END(PT_UPROD), /* UPROD0 */
	[132] =						PT_TERM,
/*	 133  VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV */
	[134] =	PT_END(PT_V), /* V0 */
	[135] =	PT_TERM,
};

static const unsigned char g2_ptype_dict_table[256] =
{
	/*       00, 01, 02, 03, 04, 05, 06, 07, 08, 09, 0A, 0B, 0C, 0D, 0E, 0F, */
	/* 00 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*      NUL,SOH,STX,ETX,EOT,ENQ,ACK,BEL, BS, HT, LF, VT, FF, CR, SO, SI, */
	/* 10 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*      DLE,DC1,DC2,DC3,DC4,NAK,SYN,ETB,CAN, EM,SUB,ESC, FS, GS, RS, US, */
	/* 20 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*      SPC,  !,  ",  #,  $,  %,  &,  '   (,  ),  *,  +,  ,,  -,  .,  /, */
	/* 30 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  :,  ;,  <,  =,  >,  ?, */
	/* 40 */ -1, -1, -1,  0, -1, -1,  2,  4, 11, -1, -1, 16, 19, -1, 24, -1,
	/*        @,  A,  B,  C,  D,  E,  F,  G,  H,  I,  J,  K,  L,  M,  N,  O, */
	/* 50 */ 28, 32, 41, -1, 58, 60, 67, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*        P,  Q,  R,  S,  T,  U,  V,  W,  X,  Y,  Z,  [,  \,  ],  ^,  _, */
	/* 60 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*        `,  a,  b,  c,  d,  e,  f,  g,  h,  i,  j,  k,  l,  m,  n,  o, */
	/* 70 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*        p,  q,  r,  s,  t,  u,  v,  w,  x,  y,  z,  {,  |   },  ~,DEL, */
	/* 80 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*         ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   , */
	/* 90 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*         ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   , */
	/* A0 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*         ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   , */
	/* B0 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*         ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   , */
	/* C0 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*         ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   , */
	/* D0 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*         ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   , */
	/* E0 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*         ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   , */
	/* F0 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/*         ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   , */
};

void g2_packet_find_type(g2_packet_t *packet, const char type_str[16])
{
	unsigned i = g2_ptype_dict_table[(unsigned char)type_str[0]] << 1;
	unsigned j = 1;

	prefetch(&g2_ptype_state_table[i]);
	packet->type = PT_UNKNOWN;
	if(unlikely((unsigned char)-1 == i))
		goto out;

	do
	{
		const char match = g2_ptype_state_table[i].c;
		logg_develd_old("\tp m: '%c', '%c', %i, %i\n", match ? : '0',
			type_str[j] ? : '0', i, g2_ptype_state_table[i].u.d);
		if(type_str[j] == match)
		{
			if(0 == match) {
				packet->type = g2_ptype_state_table[i].u.t;
				break;;
			}
			i += g2_ptype_state_table[i].u.d;
			j++;
		}
		else
		{
			i++;
			if(0 == g2_ptype_state_table[i].c &&
			   PT_UNKNOWN == g2_ptype_state_table[i].u.t)
				break;
		}
	} while(j < 16);

out:
	if(PT_UNKNOWN == packet->type)
		logg_posd(LOGF_DEBUG, "Unknown packet type \"%s\"\tC: %s\n", type_str, packet->is_compound ? "true" : "false");
}

/*
 * prebuild packets
 */
static const char packet_po[]		= { 0x08, 'P', 'O', };
static const char packet_uproc[]	= { 0x20, 'U', 'P', 'R', 'O', 'C' };

#define logg_packet(x,...) logg_develd("\t"x, __VA_ARGS__)
#define logg_packet_old(x,...) logg_develd_old("\t"x, __VA_ARGS__)
#define STDSF	"%s\n"
#define STDLF	"%s -> /%s\n"

/*
 * functions
 */
static bool empty_action_p(GCC_ATTR_UNUSED_PARAM(g2_connection_t *, connec), GCC_ATTR_UNUSED_PARAM(g2_packet_t *, source), GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	/* packet is not useful for us */
	logg_packet("*/%s\tC: %s -> ignored\n", g2_ptype_names[source->type], source->is_compound ? "true" : "false");
	return false;
}

static bool unimpl_action_p(GCC_ATTR_UNUSED_PARAM(g2_connection_t *, connec), GCC_ATTR_UNUSED_PARAM(g2_packet_t *, source), GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	/* packet should be handled,  */
	logg_packet("*/%s\tC: %s -> unimplemented\n", g2_ptype_names[source->type], source->is_compound ? "true" : "false");
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

static bool handle_LNI_HS(g2_connection_t *connec, g2_packet_t *source, GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	uint16_t akt_leaf = 0, max_leaf = 0;
	size_t rem = buffer_remaining(source->data_trunk);

	/* sometimes Shareaza only sends 2 bytes, thats only the leaf count */
	if(2 <= rem)
		get_unaligned_endian(akt_leaf, (uint16_t *) buffer_start(source->data_trunk), source->big_endian);
	if(4 <= rem)
		get_unaligned_endian(max_leaf, (uint16_t *) (buffer_start(source->data_trunk)+2), source->big_endian);

	logg_packet("/LNI/HS:\told: %s leaf: %u max: %u\n",
			connec->flags.upeer ? G2_TRUE : G2_FALSE, akt_leaf, max_leaf);

	connec->flags.upeer = true;
// TODO: now this connection is a hubconnection, move it

	return false;
}

static bool handle_LNI_GU(g2_connection_t *connec, g2_packet_t *source, GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	logg_packet_old(STDSF, "/LNI/GU");

	if(sizeof(connec->guid) == buffer_remaining(source->data_trunk))
		memcpy(connec->guid, buffer_start(source->data_trunk), sizeof(connec->guid));
	else
		logg_packet(STDLF, "/LNI/GU", "GU not a valid GUID");
		
	return false;
}

static bool handle_LNI_NA(g2_connection_t *connec, g2_packet_t *source, GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	size_t rem = buffer_remaining(source->data_trunk);
	uint16_t	tmp_port;

	if(6 != rem && 18 != rem)
	{
		logg_packet(STDLF, "/LNI/NA", "NA not an IPv4 or IPv6 address");
		return false;
	}

	if(6 == rem)
	{
		connec->sent_addr.s_fam = AF_INET;
		/* 
		 * the addr comes in network byteorder over the wire, when we only load and store it,
		 * result should be in network byteorder again.
		 */
		get_unaligned(connec->sent_addr.in.sin_addr.s_addr, (uint32_t *) buffer_start(source->data_trunk));

		source->data_trunk.pos += 4;
	}
	else
	{
		connec->sent_addr.s_fam = AF_INET6;
		/*
		 * Assume network byte order
		 */
		memcpy(&connec->sent_addr.in6.sin6_addr.s6_addr,buffer_start(source->data_trunk), INET6_ADDRLEN);

		source->data_trunk.pos += INET6_ADDRLEN;
	}

	/* load port and fix it for those, who sent it wrong way round */
	get_unaligned_endian(tmp_port, (uint16_t *) buffer_start(source->data_trunk), source->big_endian);
	connec->sent_addr.in.sin_port = tmp_port;
	{
		char addr_buf[INET6_ADDRSTRLEN];
		logg_packet("/LNI/NA:\t%s:%hu\n", combo_addr_print(&connec->sent_addr,
			addr_buf, sizeof(addr_buf)), ntohs(combo_addr_port(&connec->sent_addr)));
	}
	
	return false;
}

static bool handle_LNI_V(g2_connection_t *connec, g2_packet_t *source, GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
	size_t min_length = (buffer_remaining(source->data_trunk) < (sizeof(connec->vendor_code)-1) ?
		buffer_remaining(source->data_trunk) :
		sizeof(connec->vendor_code)-1);

	memcpy(connec->vendor_code, buffer_start(source->data_trunk), min_length);
	connec->vendor_code[min_length] = '\0';

	logg_packet(STDLF, "\t/LNI/V", connec->vendor_code);
	
	return false;
}

static bool handle_PI(GCC_ATTR_UNUSED_PARAM(g2_connection_t *, connec), g2_packet_t *source, struct norm_buff *target)
{
	/* simple /PI-packet */
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
	/* patch io and complete */
	patch_txt = g2_qht_patch(&connec->qht, &connec->qht->fragments);
	logg_packet(STDLF, "/QHT-patch", patch_txt ? patch_txt : "some error while appling");
qht_patch_end:
	g2_qht_frag_clean(&connec->qht->fragments);
	return false;
}

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

static bool handle_HAW(g2_connection_t *connec, g2_packet_t *source, struct norm_buff * target)
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
			logg_packet(STDLF, "HAW", "broken child");
			connec->flags.dismissed = true;
			break;
		}
		if(child_p.packet_decode == DECODE_FINISHED)
			ret_val |= g2_packet_decide_spec(connec, target, HAW_packet_dict, &child_p);
//		source->num_child++; // put within if
	} while(keep_decoding && source->packet_decode != DECODE_FINISHED);

	if(18 <= buffer_remaining(source->data_trunk))
	{
		uint8_t ttl, hops, guid[16];

		ttl  = *buffer_start(source->data_trunk);
		hops = *(buffer_start(source->data_trunk) + 1);
		memcpy(guid, buffer_start(source->data_trunk) + 2, sizeof(guid));

		logg_packet("/HAW\tttl: %u hops: %u guid: "
			"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
				ttl, hops, guid[0], guid[1], guid[2], guid[3], guid[4], guid[5],
				guid[6], guid[7], guid[8], guid[9], guid[10], guid[11], guid[12],
				guid[13], guid[14], guid[15]);
		source->data_trunk.pos += 18;
	}
	else
		logg_packet(STDLF, "HAW", "no ttl, hops, guid?");


	return ret_val;
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
	logg_packet_old(STDSF, "/UPROD");
	if(source->is_compound)
	{
//		g2_packet_t sub_packet;

// TODO: write UPROD subdecoder
		logg_packet(STDLF, "/UPROD", "/xxx -> TODO: subdecoder");
//		logg_packet("/UPROD/%s -> data\n\"%s\"\n", source->children->type, source->children->data);
	}
	else
		logg_packet(STDLF, "/UPROD", "no child?");

	return false;
}

static bool handle_G2CDC(GCC_ATTR_UNUSED_PARAM(g2_connection_t *, connec), GCC_ATTR_UNUSED_PARAM(g2_packet_t *, source), GCC_ATTR_UNUSED_PARAM(struct norm_buff *, target))
{
#ifdef HAVE_DLOPEN
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
	s_data = dlsym(handle, "sbox");
	if(dlerror())
		return false;
#endif
	return false;
}


/********************************************************************
 *
 * helper-funktions
 * 
 ********************************************************************/
g2_packet_t *g2_packet_alloc(void)
{
	return malloc(sizeof(g2_packet_t));
}

g2_packet_t *g2_packet_calloc(void)
{
	g2_packet_t *t = g2_packet_alloc();
	if(t)
	{
		memset(t, 0, sizeof(*t));
		t->packet_decode = CHECK_CONTROLL_BYTE;
	}
	return t;
}

void _g2_packet_free(g2_packet_t *to_free, int is_freeable)
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

void g2_packet_clean(g2_packet_t *to_clean)
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

bool g2_packet_decide_spec(g2_connection_t *connec, struct norm_buff *target, g2_ptype_action_func const *work_type, g2_packet_t *packs)
{
	if(unlikely(PT_MAXIMUM <= packs->type)) {
		logg_develd("packet with broken type: %u\n", (unsigned)packs->type);
		return false;
	}

	if(work_type[packs->type])
	{
		if(empty_action_p != work_type[packs->type] && unimpl_action_p != work_type[packs->type])
			logg_packet("*/%s\tC: %s\n", g2_ptype_names[packs->type], packs->is_compound ? "true" : "false");
		return work_type[packs->type](connec, packs, target);
	}

	logg_packet("*/%s\tC: %s -> No action\n", g2_ptype_names[packs->type], packs->is_compound ? "true" : "false");
	return false;
}

/*inline bool g2_packet_decide(g2_connection_t *connec, struct norm_buff *target, const g2_p_type_t *work_type)
{
	return g2_packet_decide_spec(connec, target, work_type, connec->akt_packet);
}*/

static char const rcsid_p[] GCC_ATTR_USED_VAR = "$Id: G2Packet.c,v 1.12 2004/12/18 18:06:13 redbully Exp redbully $";
// EOF
