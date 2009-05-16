/*
 * G2Packet.h
 * home of g2_packet_t and header-file for G2Packet.c
 *
 * Copyright (c) 2004-2008 Jan Seiffert
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
 * $Id: G2Packet.h,v 1.11 2004/12/18 18:06:13 redbully Exp redbully $
 */

#if ! defined _G2PACKET_H || defined _NEED_G2_P_TYPE
#ifndef _G2PACKET_H
# define _G2PACKET_H

# include <stdbool.h>
# include "lib/other.h"
# include "G2PacketSerializerStates.h"
# include "lib/sec_buffer.h"
# include "lib/list.h"

/*
 * This is the list of all known packet types (names)
 * Every entry gets:
 * - An enum value of PT_{NAME}
 * - gets atomagic recognized by the packet typer
 * - packets get their type set to their PT_{NAME} when
 *   they pass the Packet{De}Serializer
 *
 * If you hit an unknown packet, simply add it, everything's fine.
 *
 * These entrys are not sortet alphabeticaly, but grouped
 * by correlance (there are arrays of function pointer, we
 * want to keep the "related" packets in one cache line).
 *
 * The number behind the name is an internal detail of the packet
 * typer ATM. It's the sorting weight when packets share the same
 * prefix. A packet sortet first gets tested first everytime a
 * packet decends in this prefix!! So this should be the common packet.
 * (Do not confuse with the sorting down here, which is <see above>.
 * When there is no "collision", the packet typer sorts alphabeticaly,
 * then by weight)
 *
 * Hint: ATM there are some hacky optimisations to reduce
 * cache footprint, so after more than 126 Packet types
 * things need to be reviewd. And hopefully none gets
 * interresting ideas what are legal characters in a type.
 */
#define G2_PACKET_TYPES \
	ENUM_CMD( UNKNOWN, 0 ), /* The unknown !! ALWAYS keep at zero !! */ \
	ENUM_CMD( CRAWLA , 1 ), \
	ENUM_CMD( CRAWLR , 1 ), \
	ENUM_CMD( HAW    , 1 ), \
	ENUM_CMD( KHL    , 1 ), \
	ENUM_CMD( LNI    , 1 ), \
	ENUM_CMD( PI     , 1 ), \
	ENUM_CMD( PO     , 1 ), \
	ENUM_CMD( PUSH   , 1 ), \
	ENUM_CMD( Q2     , 1 ), \
	ENUM_CMD( QA     , 1 ), \
	ENUM_CMD( QH2    , 1 ), \
	ENUM_CMD( QHT    , 1 ), \
	ENUM_CMD( QKR    , 1 ), \
	ENUM_CMD( QKA    , 1 ), \
	ENUM_CMD( UPROC  , 1 ), \
	ENUM_CMD( UPROD  , 1 ), \
	ENUM_CMD( KHLR   , 1 ), /* UDP */ \
	ENUM_CMD( KHLA   , 1 ), /* UDP */ \
	ENUM_CMD( DIS    , 1 ), /* UDP? */ \
	ENUM_CMD( JCT    , 1 ), /* UDP connect test */ /* root packets */ \
	ENUM_CMD( G2CDc  , 1 ), \
	ENUM_CMD( DN     , 1 ), \
	ENUM_CMD( GU     , 1 ), \
	ENUM_CMD( HS     , 1 ), \
	ENUM_CMD( LS     , 1 ), \
	ENUM_CMD( NA     , 1 ), \
	ENUM_CMD( NH     , 1 ), \
	ENUM_CMD( MD     , 1 ), \
	ENUM_CMD( TS     , 1 ), \
	ENUM_CMD( URN    , 1 ), \
	ENUM_CMD( V      , 1 ), /* commom child packets */ \
	ENUM_CMD( FW     , 1 ), /* LNI? */ \
	ENUM_CMD( BUP    , 1 ), /* QH2 */ \
	ENUM_CMD( H      , 1 ), /* QH2 */ \
	ENUM_CMD( HG     , 1 ), /* QH2 */ \
	ENUM_CMD( PCH    , 1 ), /* QH2 */ \
	ENUM_CMD( UPRO   , 1 ), /* QH2 */ \
	ENUM_CMD( CSC    , 1 ), /* QH2/H */ \
	ENUM_CMD( COM    , 1 ), /* QH2/H */ \
	ENUM_CMD( G      , 1 ), /* QH2/H */ \
	ENUM_CMD( ID     , 1 ), /* QH2/H */ \
	ENUM_CMD( PART   , 1 ), /* QH2/H */ \
	ENUM_CMD( PVU    , 1 ), /* QH2/H */ \
	ENUM_CMD( SZ     , 1 ), /* QH2/H */ \
	ENUM_CMD( URL    , 1 ), /* QH2/H */ \
	ENUM_CMD( SS     , 1 ), /* QH2/HG */ \
	ENUM_CMD( I      , 1 ), /* Q2 */ \
	ENUM_CMD( SZR    , 1 ), /* Q2 */ \
	ENUM_CMD( UDP    , 1 ), /* Q2 PI */ \
	ENUM_CMD( D      , 1 ), /* QA */ \
	ENUM_CMD( FR     , 1 ), /* QA */ \
	ENUM_CMD( RA     , 3 ), /* QA */ \
	ENUM_CMD( S      , 1 ), /* QA */ \
	ENUM_CMD( NICK   , 1 ), /* QH2/UPRO */ \
	ENUM_CMD( SNA    , 1 ), /* QKA/QKR */ \
	ENUM_CMD( QNA    , 1 ), /* QKA/QKR */ \
	ENUM_CMD( QK     , 1 ), /* QKA */ \
	ENUM_CMD( CACHED , 1 ), /* QKA */ \
	ENUM_CMD( RNA    , 1 ), /* QKR */ \
	ENUM_CMD( REF    , 2 ), /* QKR */ \
	ENUM_CMD( RLEAF  , 4 ), /* CRAWLR */ \
	ENUM_CMD( RNAME  , 4 ), /* CRAWLR */ \
	ENUM_CMD( RGPS   , 4 ), /* CRAWLR */ \
	ENUM_CMD( REXT   , 4 ), /* CRAWLR */ \
	ENUM_CMD( CH     , 1 ), \
	ENUM_CMD( RELAY  , 1 ), /* PI PO */ \
	ENUM_CMD( TO     , 1 ), \
	ENUM_CMD( XML    , 1 ), /* UPROD */ \
	ENUM_CMD( MAXIMUM, 0 ) /* loop counter, hopefully none invents a Packet with this name... */

#define ENUM_CMD(x, y) PT_##x
enum g2_ptype
{
	G2_PACKET_TYPES
} GCC_ATTR_PACKED;
#undef ENUM_CMD

// TODO: more packing with bit fields?
typedef struct g2_packet
{
	/* official packet info */
	uint32_t      length;	/* 0 */
	uint8_t       length_length;	/* 4 */
	uint8_t       type_length;	/* 5 */
	enum g2_ptype type;	/* 6 */
	bool          big_endian;	/* 7 */
	bool          is_compound;	/* 8 */
	bool          c_reserved;	/* 9 */

	/* internal state */
	enum g2_packet_decoder_states packet_decode;	/* 10 */
	enum g2_packet_encoder_states packet_encode;	/* 11 */
	bool          more_bytes_needed;	/* 12 */
	bool          is_freeable;	/* 13 */
	bool          data_trunk_is_freeable;	/* 14 */
	/* 1+3+8 maximum header length +5 pad/reserve */
	char          data[17];	/* 15 */

	/* everything up to data trunk gets wiped */
	struct pointer_buff data_trunk;	/* 32 */

	/* packet-data */
	struct list_head list;	/* 48/64 */
	struct list_head children; /* 56/76 */
	/* 64/96 */
} g2_packet_t;

# ifndef _G2PACKET_C
#  define _G2PACK_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#  define _G2PACK_EXTRNVAR(x) extern x GCC_ATTR_VIS("hidden");
# else
#  define _G2PACK_EXTRN(x) x GCC_ATTR_VIS("hidden")
#  define _G2PACK_EXTRNVAR(x)
# endif /* _G2PACKET_C */

/*
 * Use dirty tricks to init a g2_packet fast.
 * We set everything to 0 from is_compound
 * to data[1] which should be 8 byte or one or
 * two moves.
 * Warning! Adapt when layout changes.
 */
# define g2_packet_init_on_stack(x) \
	do { \
		memset(&(x)->is_compound, 0, offsetof(g2_packet_t, data[1]) - offsetof(g2_packet_t, is_compound)); \
		INIT_PBUF(&(x)->data_trunk); \
		INIT_LIST_HEAD(&(x)->list); \
		INIT_LIST_HEAD(&(x)->children); \
	} while(0)
_G2PACK_EXTRN(g2_packet_t *g2_packet_init(g2_packet_t *));
_G2PACK_EXTRN(g2_packet_t *g2_packet_alloc(void));
_G2PACK_EXTRN(g2_packet_t *g2_packet_calloc(void));
_G2PACK_EXTRN(g2_packet_t *g2_packet_clone(g2_packet_t *));
_G2PACK_EXTRN(void g2_packet_free(g2_packet_t *));
_G2PACK_EXTRN(void g2_packet_clean(g2_packet_t *to_clean));
_G2PACK_EXTRN(void g2_packet_find_type(g2_packet_t *packet, const char type_str[16]));
_G2PACK_EXTRNVAR(const char g2_ptype_names[PT_MAXIMUM][8])
_G2PACK_EXTRNVAR(const uint8_t g2_ptype_names_length[PT_MAXIMUM])

#endif /* _G2PACKET_H */

#ifdef _NEED_G2_P_TYPE
# undef _NEED_G2_P_TYPE
# ifndef _HAVE_G2_P_TYPE
#  define _HAVE_G2_P_TYPE
#  include "G2Connection.h"
#  include "lib/combo_addr.h"

struct ptype_action_args
{
	g2_connection_t *connec;
	g2_packet_t *source;
	union combo_addr *src_addr;
	struct list_head *target;
	void *opaque;
};

typedef bool (*g2_ptype_action_func) (struct ptype_action_args *) ;
_G2PACK_EXTRNVAR(const g2_ptype_action_func g2_packet_dict[PT_MAXIMUM])
_G2PACK_EXTRNVAR(const g2_ptype_action_func g2_packet_dict_udp[PT_MAXIMUM])
_G2PACK_EXTRN(bool g2_packet_decide_spec(struct ptype_action_args *, g2_ptype_action_func const *));
# endif /* _HAVE_G2_P_TYPE */
#endif /* _NEED_G2_P_TYPE */

#endif /*! defined _G2PACKET_H || defined _NEED_G2_P_TYPE */
/* EOF */
