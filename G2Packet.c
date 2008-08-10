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
#include "config.h"
#endif
// System includes
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#ifdef HAVE_DLOPEN
#include <dlfcn.h>
#endif
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

#define PT_ENT(m, n)	{.match = (m), .last = false, .term = false, .found = {.next = (n)}}
#define PT_ACT(a)	{.match = '\0', .last = true, .term = false, .found = {.action = (a)}}
#define PT_TERM	{.match = 0, .last = 0, .term = true, .found = {.next = NULL}}

/*
 * seventh type-char-layer
 */
/* CRAWLA - anwser? not my bussines */
static const g2_p_type_t packet_dict_CRAWLA0[] = { PT_ACT(empty_action_p), PT_TERM };
/* CRAWLR */
static const g2_p_type_t packet_dict_CRAWLR0[] = { PT_ACT(NULL), PT_TERM };

/*
 * sixth type-char-layer
 */
/* UPROD */
static const g2_p_type_t packet_dict_UPROD0[] = { PT_ACT(handle_UPROD), PT_TERM };
/* UPROC */
static const g2_p_type_t packet_dict_UPROC0[] = { PT_ACT(handle_UPROC), PT_TERM };
/* CRAWLx */
static const g2_p_type_t packet_dict_CRAWLx[] = {
	PT_ENT('A', packet_dict_CRAWLA0),
	PT_ENT('R', packet_dict_CRAWLR0),
	PT_TERM};
/* G2CDC */
static const g2_p_type_t packet_dict_G2CDC0[] = { PT_ACT(handle_G2CDC), PT_TERM };

/*
 * fith type-char-layer
 */
/* UPROx */
static const g2_p_type_t packet_dict_UPROx[] = {
	PT_ENT('D', packet_dict_UPROD0),
	PT_ENT('C', packet_dict_UPROC0),
	PT_TERM};
/* CRAWx */
static const g2_p_type_t packet_dict_CRAWL[] = { PT_ENT('L', packet_dict_CRAWLx), PT_TERM };
/* G2CDx */
static const g2_p_type_t packet_dict_G2CDC[] = { PT_ENT('C', packet_dict_G2CDC0), PT_TERM };

/*
 * fourth type-char-layer
 */
/* KHL */
static const g2_p_type_t packet_dict_KHL0[] = { PT_ACT(handle_KHL), PT_TERM };
/* LNI */
static const g2_p_type_t packet_dict_LNI0[] = { PT_ACT(handle_LNI), PT_TERM };
/* QHT */
static const g2_p_type_t packet_dict_QHT0[] = { PT_ACT(handle_QHT), PT_TERM };
/* QKR */
static const g2_p_type_t packet_dict_QKR0[] = { PT_ACT(NULL), PT_TERM };
/* QKA */
static const g2_p_type_t packet_dict_QKA0[] = { PT_ACT(NULL), PT_TERM };
/* UPRx */
static const g2_p_type_t packet_dict_UPRO[] = { PT_ENT('O', packet_dict_UPROx), PT_TERM };
/* HAW */
static const g2_p_type_t packet_dict_HAW0[] = { PT_ACT(handle_HAW), PT_TERM };
/* CRAx */
static const g2_p_type_t packet_dict_CRAW[] = { PT_ENT('W', packet_dict_CRAWL), PT_TERM };
/* G2Cx */
static const g2_p_type_t packet_dict_G2CD[] = { PT_ENT('D', packet_dict_G2CDC), PT_TERM };

/*
 * third type-char-layer
 */
/* KHx */
static const g2_p_type_t packet_dict_KHL[] = { PT_ENT('L', packet_dict_KHL0), PT_TERM };
/* LNx */
static const g2_p_type_t packet_dict_LNI[] = { PT_ENT('I', packet_dict_LNI0), PT_TERM };
/* PI */
static const g2_p_type_t packet_dict_PI0[] = { PT_ACT(handle_PI), PT_TERM };
/* PO */
static const g2_p_type_t packet_dict_PO0[] = { PT_ACT(NULL), PT_TERM };
/* Q2 */
static const g2_p_type_t packet_dict_Q20[] = { PT_ACT(NULL), PT_TERM };
/* QHx */
static const g2_p_type_t packet_dict_QHT[] = { PT_ENT('T', packet_dict_QHT0), PT_TERM };
/* QKx */
static const g2_p_type_t packet_dict_QKx[] = {
	PT_ENT('A', packet_dict_QKA0),
	PT_ENT('R', packet_dict_QKR0),
	PT_TERM};
/* UPx */
static const g2_p_type_t packet_dict_UPR[] = { PT_ENT('R', packet_dict_UPRO), PT_TERM };
/* HAx */
static const g2_p_type_t packet_dict_HAW[] = { PT_ENT('W', packet_dict_HAW0), PT_TERM };
/* CRx */
static const g2_p_type_t packet_dict_CRA[] = { PT_ENT('A', packet_dict_CRAW), PT_TERM };
/* G2x */
static const g2_p_type_t packet_dict_G2C[] = { PT_ENT('C', packet_dict_G2CD), PT_TERM };

/* 
 * second type-char-layer
 */
/* Kx */
static const g2_p_type_t packet_dict_KH[] = { PT_ENT('H', packet_dict_KHL), PT_TERM };
/* Lx */
static const g2_p_type_t packet_dict_LN[] = { PT_ENT('N', packet_dict_LNI), PT_TERM };
/* Px */
static const g2_p_type_t packet_dict_Px[] = {
		PT_ENT('I', packet_dict_PI0),
		PT_ENT('O', packet_dict_PO0),
		PT_TERM};
/* Qx */
static const g2_p_type_t packet_dict_Qx[] = {
		PT_ENT('H', packet_dict_QHT),
		PT_ENT('K', packet_dict_QKx),
		PT_ENT('2', packet_dict_Q20),
		PT_TERM};
/* Ux */
static const g2_p_type_t packet_dict_UP[] = { PT_ENT('P', packet_dict_UPR), PT_TERM };
/* Hx */
static const g2_p_type_t packet_dict_HA[] = { PT_ENT('A', packet_dict_HAW), PT_TERM };
/* Cx */
static const g2_p_type_t packet_dict_CR[] = { PT_ENT('R', packet_dict_CRA), PT_TERM };
/* Gx */
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
	PT_ENT('H', packet_dict_HA),
	PT_ENT('C', packet_dict_CR),
	PT_ENT('G', packet_dict_G2),
	PT_TERM
};

/*
 * LNI-childs
 */
/* third */
/* /LNI/HS */
static const g2_p_type_t LNI_packet_dict_HS0[] = { PT_ACT(handle_LNI_HS), PT_TERM };
/* /LNI/GU */
static const g2_p_type_t LNI_packet_dict_GU0[] = { PT_ACT(handle_LNI_GU), PT_TERM };
/* /LNI/FW */
static const g2_p_type_t LNI_packet_dict_FW0[] = { PT_ACT(NULL), PT_TERM };
/* /LNI/LS */
static const g2_p_type_t LNI_packet_dict_LS0[] = { PT_ACT(empty_action_p), PT_TERM };
/* /LNI/NA */
static const g2_p_type_t LNI_packet_dict_NA0[] = { PT_ACT(handle_LNI_NA), PT_TERM };
/* /LNI/QK */
static const g2_p_type_t LNI_packet_dict_QK0[] = { PT_ACT(NULL), PT_TERM };

/* second */
/* /LNI/Hx */
static const g2_p_type_t LNI_packet_dict_HS[] = { PT_ENT('S', LNI_packet_dict_HS0), PT_TERM };
/* /LNI/Gx */
static const g2_p_type_t LNI_packet_dict_GU[] = { PT_ENT('U', LNI_packet_dict_GU0), PT_TERM };
/* /LNI/Fx */
static const g2_p_type_t LNI_packet_dict_FW[] = { PT_ENT('W', LNI_packet_dict_FW0), PT_TERM };
/* /LNI/Lx */
static const g2_p_type_t LNI_packet_dict_LS[] = { PT_ENT('S', LNI_packet_dict_LS0), PT_TERM };
/* /LNI/Nx */
static const g2_p_type_t LNI_packet_dict_NA[] = { PT_ENT('A', LNI_packet_dict_NA0), PT_TERM };
/* /LNI/Qx */
static const g2_p_type_t LNI_packet_dict_QK[] = { PT_ENT('K', LNI_packet_dict_QK0), PT_TERM };
/* /LNI/V */
static const g2_p_type_t LNI_packet_dict_V0[] = { PT_ACT(handle_LNI_V), PT_TERM };

/* first */
static const g2_p_type_t LNI_packet_dict[] = {
	PT_ENT('G', LNI_packet_dict_GU),
	PT_ENT('H', LNI_packet_dict_HS),
	PT_ENT('F', LNI_packet_dict_FW),
	PT_ENT('L', LNI_packet_dict_LS),
	PT_ENT('N', LNI_packet_dict_NA),
	PT_ENT('Q', LNI_packet_dict_QK),
	PT_ENT('V', LNI_packet_dict_V0),
	PT_TERM
};


/*
 * KHL-childs
 */
/* third */
/* /KHL/TS */
static const g2_p_type_t KHL_packet_dict_TS0[] = { PT_ACT(handle_KHL_TS), PT_TERM };
/* /KHL/NH */
static const g2_p_type_t KHL_packet_dict_NH0[] = { PT_ACT(NULL), PT_TERM };
/* /KHL/CH */
static const g2_p_type_t KHL_packet_dict_CH0[] = { PT_ACT(NULL), PT_TERM };

/* second */
/* /KHL/Tx */
static const g2_p_type_t KHL_packet_dict_TS[] = { PT_ENT('S', KHL_packet_dict_TS0), PT_TERM };
/* /KHL/Nx */
static const g2_p_type_t KHL_packet_dict_NH[] = { PT_ENT('H', KHL_packet_dict_NH0), PT_TERM };
/* /KHL/Cx */
static const g2_p_type_t KHL_packet_dict_CH[] = { PT_ENT('H', KHL_packet_dict_CH0), PT_TERM };

/* first */
static const g2_p_type_t KHL_packet_dict[] = { 
	PT_ENT('C', KHL_packet_dict_CH),
	PT_ENT('N', KHL_packet_dict_NH),
	PT_ENT('T', KHL_packet_dict_TS),
	PT_TERM
};


/*
 * CRAWLR-childs
 */
/* seventh */
/* /CRAWLR/RLEAF */
static const g2_p_type_t CRAWLR_packet_dict_RLEAF0[] = { PT_ACT(NULL), PT_TERM };
/* /CRAWLR/RNAME */
static const g2_p_type_t CRAWLR_packet_dict_RNAME0[] = { PT_ACT(NULL), PT_TERM };

/* sixth */
/* /CRAWLR/RLEAx */
static const g2_p_type_t CRAWLR_packet_dict_RLEAF[] = { PT_ENT('F', CRAWLR_packet_dict_RLEAF0), PT_TERM };
/* /CRAWLR/RNAMx */
static const g2_p_type_t CRAWLR_packet_dict_RNAME[] = { PT_ENT('E', CRAWLR_packet_dict_RNAME0), PT_TERM };
/* /CRAWLR/RGPS */
static const g2_p_type_t CRAWLR_packet_dict_RGPS0[] = { PT_ACT(NULL), PT_TERM };
/* /CRAWLR/REXT */
static const g2_p_type_t CRAWLR_packet_dict_REXT0[] = { PT_ACT(NULL), PT_TERM };

/* fourth */
/* /CRAWLR/RLEx */
static const g2_p_type_t CRAWLR_packet_dict_RLEA[] = { PT_ENT('A', CRAWLR_packet_dict_RLEAF), PT_TERM };
/* /CRAWLR/RNAx */
static const g2_p_type_t CRAWLR_packet_dict_RNAM[] = { PT_ENT('M', CRAWLR_packet_dict_RNAME), PT_TERM };
/* /CRAWLR/RGPx */
static const g2_p_type_t CRAWLR_packet_dict_RGPS[] = { PT_ENT('S', CRAWLR_packet_dict_RGPS0), PT_TERM };
/* /CRAWLR/REXx */
static const g2_p_type_t CRAWLR_packet_dict_REXT[] = { PT_ENT('T', CRAWLR_packet_dict_REXT0), PT_TERM };

/* third */
/* /CRAWLR/RLx */
static const g2_p_type_t CRAWLR_packet_dict_RLE[] = { PT_ENT('E', CRAWLR_packet_dict_RLEA), PT_TERM };
/* /CRAWLR/RNx */
static const g2_p_type_t CRAWLR_packet_dict_RNA[] = { PT_ENT('A', CRAWLR_packet_dict_RNAM), PT_TERM };
/* /CRAWLR/RGx */
static const g2_p_type_t CRAWLR_packet_dict_RGP[] = { PT_ENT('P', CRAWLR_packet_dict_RGPS), PT_TERM };
/* /CRAWLR/REx */
static const g2_p_type_t CRAWLR_packet_dict_REX[] = { PT_ENT('X', CRAWLR_packet_dict_REXT), PT_TERM };

/* second */
/* /CRAWLR/Rx */
static const g2_p_type_t CRAWLR_packet_dict_Rx[] = {
	PT_ENT('E', CRAWLR_packet_dict_REX),
	PT_ENT('G', CRAWLR_packet_dict_RGP),
	PT_ENT('N', CRAWLR_packet_dict_RNA),
	PT_ENT('L', CRAWLR_packet_dict_RLE),
	PT_TERM
};

/* first */
static const g2_p_type_t CRAWLR_packet_dict[] = { PT_ENT('R', CRAWLR_packet_dict_Rx), PT_TERM };


/*
 * HAW-childs
 */
/* third */
/* /HAW/HS */
static const g2_p_type_t HAW_packet_dict_HS0[] = { PT_ACT(NULL), PT_TERM };
/* /HAW/NA */
static const g2_p_type_t HAW_packet_dict_NA0[] = { PT_ACT(NULL), PT_TERM };

/* second */
/* /HAW/Hx */
static const g2_p_type_t HAW_packet_dict_HS[] = { PT_ENT('S', HAW_packet_dict_HS0), PT_TERM };
/* /HAW/Nx */
static const g2_p_type_t HAW_packet_dict_NA[] = { PT_ENT('A', HAW_packet_dict_NA0), PT_TERM };
/* /HAW/V */
static const g2_p_type_t HAW_packet_dict_V0[] = { PT_ACT(NULL), PT_TERM };

/* first */
static const g2_p_type_t HAW_packet_dict[] = {
	PT_ENT('H', HAW_packet_dict_HS),
	PT_ENT('N', HAW_packet_dict_NA),
	PT_ENT('V', HAW_packet_dict_V0),
	PT_TERM
};

#define ENUM_CMD(x) str_it(x)
const char *const g2_ptype_names[] = 
{
	G2_PACKET_TYPES
};
#undef ENUM_CMD

#define PT_T \
	{ .c = 0, .u = { .t = PT_UNKNOWN }}
#define PT_E(x) \
	{ .c = '\0', .u = { .t = x }}
#define PT_N(x, y) \
	{ .c = x, .u = { .d = y}}

static const struct 
{
	const char c;
	union
	{
		const enum g2_ptype t;
		const unsigned char d;
	} u;
} g2_ptype_state_table[] = {
/* CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC */
[  0] =	PT_N('H', 2), /* CH */
[  1] =	PT_T,
[  3] =		PT_E(PT_CH), /* CH0 */
[  4] =		PT_T,
/* FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF */
[  5] =	PT_N('W', 2), /* FW */
[  6] =	PT_T,
[  7] =		PT_E(PT_FW), /* FW0 */
[  8] =		PT_T,	
/* GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG */
[  9] =	PT_N('U', 3), /* GU */
[ 10] =	PT_N('2', 4), /* G2 */
[ 11] =	PT_T,
[ 12] =		PT_E(PT_GU), /* GU0 */
[ 13] =		PT_T,
[ 14] =		PT_N('C', 2), /* G2C */
[ 15] =		PT_T,
[ 16] =			PT_N('D', 2), /* G2CD */
[ 17] =			PT_T,
[ 18] =				PT_N('C', 2), /* G2CDC */
[ 19] =				PT_T,
[ 20] =					PT_E(PT_G2CDC), /* G2CDC0 */
[ 21] =					PT_T,
/* HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH */
[ 22] =	PT_N('S', 3), /* HS */
[ 23] =	PT_N('A', 4), /* HA */
[ 24] =	PT_T,
[ 25] =		PT_E(PT_HS), /* HS0 */
[ 26] =		PT_T,
[ 27] =		PT_N('W', 2), /* HAW */
[ 28] =		PT_T,
[ 29] =			PT_E(PT_HAW), /* HAW0 */
[ 30] =			PT_T,
/* KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK */
[ 31] =	PT_N('H', 2), /* KH */
[ 32] =	PT_T,
[ 33] =		PT_N('L', 2), /* KHL */
[ 34] =		PT_T,
[ 35] =			PT_E(PT_KHL), /* KHL0 */
[ 36] =			PT_T,
/* LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL */
[ 37] =	PT_N('N', 3), /* LN */
[ 38] =	PT_N('S', 6), /* LS */
[ 39] =	PT_T,
[ 40] =		PT_N('I', 2), /* LNI */
[ 41] =		PT_T,
[ 42] =			PT_E(PT_LNI), /* LNI0 */
[ 43] =			PT_T,
[ 44] =		PT_E(PT_LS), /* LS0 */
[ 45] =		PT_T,
/* NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN */
[ 46] =	PT_N('A', 3), /* NA */
[ 47] =	PT_N('H', 4), /* NH */
[ 48] =	PT_T,
[ 49] =		PT_E(PT_NA), /* NA0 */
[ 50] =		PT_T,
[ 51] =		PT_E(PT_NH), /* NH0 */
[ 52] =		PT_T,
/* PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP */
[ 53] =	PT_N('I', 3), /* PI */
[ 54] =	PT_N('O', 4), /* PO */
[ 55] =	PT_T,
[ 56] =		PT_E(PT_PI), /* PI0 */
[ 57] =		PT_T,
[ 58] =		PT_E(PT_PO), /* PO0 */
[ 59] =		PT_T,
/* QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ */
[ 60] =	PT_N('H', 3), /* QH */
[ 61] =	PT_N('K', 6), /* QK */
[ 62] =	PT_T,
[ 63] =		PT_N('T', 2), /* QHT */
[ 64] =		PT_T,
[ 65] =			PT_E(PT_QHT), /* QHT0 */
[ 66] =			PT_T,
[ 67] =		PT_E(PT_QK), /* QK0 */
[ 68]	=		PT_N('A', 3), /* QKA */
[ 69]	=		PT_N('R', 4), /* QKR */
[ 70] =		PT_T,
[ 71] =			PT_E(PT_QKA), /* QKA0 */
[ 72] =			PT_T,
[ 73] =			PT_E(PT_QKR), /* QKR0 */
[ 74] =			PT_T,
/* RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR */
[ 75] =	PT_N('E', 5), /* RE */
[ 76] =	PT_N('G', 6), /* RG */
[ 77]	=	PT_N('L', 7), /* RL */
[ 78]	=	PT_N('N', 8), /* RN */
[ 79] =	PT_T,
[ 80] =		PT_N('X', 8), /* REX */
[ 81] =		PT_T,
[ 82] =		PT_N('P', 8), /* RGP */
[ 83] =		PT_T,
[ 84] =		PT_N('E', 8), /* RLE */
[ 85] =		PT_T,
[ 86] =		PT_N('A', 8), /* RNA */
[ 87]	=		PT_T,
[ 88]	=			PT_N('T', 8), /* REXT */
[ 89] =			PT_T,
[ 90] =			PT_N('S', 8), /* RGPS */
[ 91] =			PT_T,
[ 92] =			PT_N('A', 8), /* RLEA */
[ 93] =			PT_T,
[ 94] =			PT_N('M', 8), /* RNAM */
[ 95] =			PT_T,
[ 96] =				PT_E(PT_REXT), /* REXT0 */
[ 97]	=				PT_T,
[ 98]	=				PT_E(PT_RGPS), /* RGPS0 */
[ 99] =				PT_T,
[100] =				PT_N('F', 4), /* RLEAF */
[101] =				PT_T,
[102]	=				PT_N('E', 4), /* RNAME */
[103]	=				PT_T,
[104] =					PT_E(PT_RLEAF), /* RLEAF0 */
[105] =					PT_T,
[106] =					PT_E(PT_RNAME), /* RNAME0 */
[107] =					PT_T,
/* TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT */
[108] =	PT_N('S', 2), /* TS */
[109] =	PT_T,
[110] =		PT_E(PT_TS), /* TS0 */
[111]	=		PT_T,
/* UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU */
[112] =	PT_N('P', 2), /* UP */
[113] =	PT_T,
[114] =		PT_N('R', 2), /* UPR */
[115] =		PT_T,
[116] =				PT_N('O', 2), /* UPRO */
[117] =				PT_T,
[118] =					PT_N('C', 3), /* UPROC */
[119] = 					PT_N('D', 4), /* UPROD */
[120]	=					PT_T,
[121]	=						PT_E(PT_UPROC), /* UPROC0 */
[122] =						PT_T,
[123] =						PT_E(PT_UPROD), /* UPROD0 */
[124] =						PT_T,
/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV */
[125] =	PT_E(PT_V), /* V0 */
[126] =	PT_T,
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
	/* 40 */ -1, -1, -1,  0, -1, -1,  5,  9, 22, -1, -1, 31, 37, -1, 46, -1,
	/*        @,  A,  B,  C,  D,  E,  F,  G,  H,  I,  J,  K,  L,  M,  N,  O, */
	/* 50 */ 53, 60, 75, -1,108,112,125, -1, -1, -1, -1, -1, -1, -1, -1, -1,
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
	unsigned char i = g2_ptype_dict_table[(unsigned char)type_str[0]];
	unsigned int j = 1;

	prefetch(&g2_ptype_state_table[i]);
	packet->type = PT_UNKNOWN;
	if(unlikely((unsigned char)-1 == i))
		goto out;

	do
	{
		const char match = g2_ptype_state_table[i].c;
		logg_develd_old("\tm: '%c', '%c', %i, %i\n", match ? : '0',
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
	} while(j < sizeof(type_str) && type_str[j]);

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

bool g2_packet_decide_spec(g2_connection_t *connec, struct norm_buff *target, const g2_p_type_t *work_type, g2_packet_t *packs)
{
	const char *to_match = g2_ptype_names[packs->type];
	bool ret_val = false;

	do
	{
		if(work_type->match == *to_match)
		{
			if(work_type->last)
			{
				if(work_type->found.action)
				{
					if(empty_action_p == work_type->found.action)
						logg_packet("*/%s\tC: %s -> ignored\n", g2_ptype_names[packs->type], packs->is_compound ? "true" : "false");
					else
						logg_packet("*/%s\tC: %s\n", g2_ptype_names[packs->type], packs->is_compound ? "true" : "false");
					ret_val |= work_type->found.action(connec, packs, target);
				}
				else
					logg_packet("*/%s\tC: %s -> No action\n", g2_ptype_names[packs->type], packs->is_compound ? "true" : "false");

				break;
			}
			else
				work_type = work_type->found.next;

			to_match++;
		}
		else
			work_type++;
	} while(!work_type->term);

	return ret_val;
}

/*inline bool g2_packet_decide(g2_connection_t *connec, struct norm_buff *target, const g2_p_type_t *work_type)
{
	return g2_packet_decide_spec(connec, target, work_type, connec->akt_packet);
}*/

static char const rcsid_p[] GCC_ATTR_USED_VAR = "$Id: G2Packet.c,v 1.12 2004/12/18 18:06:13 redbully Exp redbully $";
// EOF
