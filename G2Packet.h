/*
 * G2Packet.h
 * home of g2_packet_t and header-file for G2Packet.c
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
	ENUM_CMD( CRAWLA ,      0 ), \
	ENUM_CMD( CRAWLR ,     34 ), \
	ENUM_CMD( HAW    ,   1464 ), \
	ENUM_CMD( KHL    ,  10065 ), \
	ENUM_CMD( LNI    ,  23226 ), \
	ENUM_CMD( PI     ,  24756 ), \
	ENUM_CMD( PO     ,      0 ), \
	ENUM_CMD( PUSH   ,      1 ), \
	ENUM_CMD( Q2     , 585085 ), \
	ENUM_CMD( QA     ,  15635 ), \
	ENUM_CMD( QH2    ,  44285 ), \
	ENUM_CMD( QHT    ,  11458 ), \
	ENUM_CMD( QKR    , 258528 ), \
	ENUM_CMD( QKA    ,  17385 ), \
	ENUM_CMD( UPROC  ,    649 ), \
	ENUM_CMD( UPROD  ,      0 ), \
	ENUM_CMD( KHLR   ,      3 ), /* UDP */ \
	ENUM_CMD( KHLA   ,      0 ), /* UDP */ \
	ENUM_CMD( DIS    ,      1 ), /* UDP, KHL extention */ \
	ENUM_CMD( JCT    ,      0 ), /* UDP connect test */ /* root packets */ \
	ENUM_CMD( G2CDc  ,      0 ), \
	ENUM_CMD( DN     ,  87891 ), \
	ENUM_CMD( GU     ,  27226 ), \
	ENUM_CMD( HS     ,   3124 ), \
	ENUM_CMD( LS     ,  23225 ), \
	ENUM_CMD( NA     ,  24690 ), \
	ENUM_CMD( MD     ,  54438 ), \
	ENUM_CMD( TS     ,  10065 ), \
	ENUM_CMD( URN    ,1043964 ), \
	ENUM_CMD( NH     ,      1 ), /* KHL(A) */ \
	ENUM_CMD( CH     , 287328 ), /* KHL(A) */ \
	ENUM_CMD( UKHLID ,      5 ), /* KHLA/KHLR? */ \
	ENUM_CMD( YOURIP ,      0 ), /* KHLA */ \
	ENUM_CMD( CV     ,      0 ), /* version */ \
	ENUM_CMD( V      ,  24689 ), /* commom child packets */ \
	ENUM_CMD( FW     ,   9494 ), /* LNI? */ \
	ENUM_CMD( RTR    ,     93 ), /* LNI */ \
	ENUM_CMD( UP     ,    199 ), /* LNI */ \
	ENUM_CMD( HA     ,    199 ), /* LNI */ \
	ENUM_CMD( NFW    ,      1 ), /* LNI */ \
	ENUM_CMD( TCPNFW ,      1 ), /* LNI */ \
	ENUM_CMD( UDPFW  ,      1 ), /* LNI */ \
	ENUM_CMD( UDPNFW ,      1 ), /* LNI */ \
	ENUM_CMD( BUP    ,      1 ), /* QH2 */ \
	ENUM_CMD( BH     ,      1 ), /* QH2 */ \
	ENUM_CMD( BUSY   ,      1 ), /* QH2 */ \
	ENUM_CMD( UNSTA  ,      1 ), /* QH2 */ \
	ENUM_CMD( H      ,      1 ), /* QH2 */ \
	ENUM_CMD( HG     ,      1 ), /* QH2 */ \
	ENUM_CMD( PCH    ,      1 ), /* QH2 */ \
	ENUM_CMD( UPRO   ,      1 ), /* QH2 */ \
	ENUM_CMD( CS     ,      1 ), /* QH2 */ \
	ENUM_CMD( CSC    ,      1 ), /* QH2/H */ \
	ENUM_CMD( COM    ,      1 ), /* QH2/H */ \
	ENUM_CMD( G      ,      1 ), /* QH2/H */ \
	ENUM_CMD( ID     ,      1 ), /* QH2/H */ \
	ENUM_CMD( PART   ,      1 ), /* QH2/H */ \
	ENUM_CMD( PVU    ,      1 ), /* QH2/H */ \
	ENUM_CMD( SZ     ,      1 ), /* QH2/H */ \
	ENUM_CMD( URL    ,      1 ), /* QH2/H */ \
	ENUM_CMD( SS     ,      1 ), /* QH2/HG */ \
	ENUM_CMD( QKY    ,      1 ), /* Q2 extention, + key & no addr */ \
	ENUM_CMD( I      , 400525 ), /* Q2 */ \
	ENUM_CMD( SZR    , 365881 ), /* Q2 */ \
	ENUM_CMD( UDP    , 592569 ), /* Q2 PI */ \
	ENUM_CMD( G1     ,    122 ), /* Q2 */ \
	ENUM_CMD( G1PP   ,      0 ), /* QH2 */ \
	ENUM_CMD( NOG1   ,     43 ), /* Q2 */ \
	ENUM_CMD( D      ,      1 ), /* QA */ \
	ENUM_CMD( FR     ,      1 ), /* QA */ \
	ENUM_CMD( RA     ,      1 ), /* QA */ \
	ENUM_CMD( S      ,      1 ), /* QA */ \
	ENUM_CMD( NICK   ,      1 ), /* QH2/UPRO */ \
	ENUM_CMD( SNA    ,  66687 ), /* QKA/QKR */ \
	ENUM_CMD( QNA    ,  39421 ), /* QKA/QKR */ \
	ENUM_CMD( QK     ,  18846 ), /* QKA */ \
	ENUM_CMD( CACHED ,      1 ), /* QKA */ \
	ENUM_CMD( RNA    ,  19709 ), /* QKR */ \
	ENUM_CMD( REF    ,      1 ), /* QKR */ \
	ENUM_CMD( RLEAF  ,     34 ), /* CRAWLR */ \
	ENUM_CMD( RNAME  ,     34 ), /* CRAWLR */ \
	ENUM_CMD( RGPS   ,     34 ), /* CRAWLR */ \
	ENUM_CMD( REXT   ,     34 ), /* CRAWLR */ \
	ENUM_CMD( NL     ,      0 ), /* CRAWLA */ \
	ENUM_CMD( SELF   ,      0 ), /* CRAWLA */ \
	ENUM_CMD( HUB    ,      0 ), /* CRAWLA */ \
	ENUM_CMD( LEAF   ,      0 ), /* CRAWLA */ \
	ENUM_CMD( GPS    ,    199 ), /* CRAWLA */ \
	ENUM_CMD( NAME   ,      0 ), /* CRAWLA, same as NICK, idiots */ \
	ENUM_CMD( RELAY  ,   6727 ), /* PI PO */ \
	ENUM_CMD( TO     ,      1 ), \
	ENUM_CMD( XML    ,      1 ), /* UPROD */ \
	ENUM_CMD( dna    , 167288 ), /* GnucDNA */ \
	ENUM_CMD( CR     ,   5824 ), /* GnucDNA */ \
	ENUM_CMD( NAT    ,    343 ), /* ?? */ \
	ENUM_CMD( QH1    ,    738 ), /* ?? */ \
	ENUM_CMD( IDENT  ,     73 ), /* GnucDNA */ \
	ENUM_CMD( NBW    ,    199 ), /* GnucDNA */ \
	ENUM_CMD( PM     ,      0 ), /* GnucDNA */ \
	ENUM_CMD( CM     ,    199 ), /* GnucDNA */ \
	ENUM_CMD( TFW    ,     74 ), /* ?? */ \
	ENUM_CMD( G2NS   ,      5 ), /* ?? */ \
	ENUM_CMD( CLOSE  ,      4 ), /* ?? */ \
	ENUM_CMD( HURN   ,    651 ), \
	ENUM_CMD( HKEY   ,    842 ), \
	ENUM_CMD( MAXIMUM,      0 ) /* loop counter, hopefully none invents a Packet with this name... */

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
	uint8_t       type_length;	/* 4 */
	uint8_t       length_length;	/* 5 */
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
	bool          is_literal;	/* 15 */
	union	/* 16 */
	{
		struct _pd_mixed_buf_internal
		{
			/* 8 maximum type len */
			char    type[8];
			/* 1+3+type = maximum header len + pad/reserve + 32 space for out */
			char    buf[1 + 3 + 4 + 32];
		} in;
		char       out[sizeof(struct _pd_mixed_buf_internal)];
	} pd;

	/* everything up to data trunk gets wiped */
	struct pointer_buff data_trunk;	/* 64 */

	/* packet-data */
	struct list_head list;	/* 80/96 */
	struct list_head children;	/* 88/112 */
	/* 96/128 */
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
 * to data[0] which should be 8 byte or one or
 * two moves.
 * Warning! Adapt when layout changes.
 */
# define g2_packet_init_on_stack(x) \
	do { \
		memset(&(x)->is_compound, 0, offsetof(g2_packet_t, pd.out[0]) - offsetof(g2_packet_t, is_compound)); \
		INIT_PBUF(&(x)->data_trunk); \
		INIT_LIST_HEAD(&(x)->list); \
		INIT_LIST_HEAD(&(x)->children); \
	} while(0)

_G2PACK_EXTRN(g2_packet_t *g2_packet_init(g2_packet_t *));
_G2PACK_EXTRN(g2_packet_t *g2_packet_alloc(void) GCC_ATTR_MALLOC);
_G2PACK_EXTRN(g2_packet_t *g2_packet_calloc(void) GCC_ATTR_MALLOC);
_G2PACK_EXTRN(g2_packet_t *g2_packet_clone(g2_packet_t *) GCC_ATTR_MALLOC);
_G2PACK_EXTRN(void g2_packet_local_alloc_init(void));
_G2PACK_EXTRN(void g2_packet_local_alloc_init_min(void));
_G2PACK_EXTRN(void g2_packet_local_refill(void));
_G2PACK_EXTRN(void g2_packet_free(g2_packet_t *));
_G2PACK_EXTRN(void g2_packet_free_glob(g2_packet_t *));
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
	g2_connection_t  *connec;
	g2_packet_t      *father;
	g2_packet_t      *source;
	union combo_addr *src_addr;
	union combo_addr *dst_addr;
	shortlock_t      *target_lock;
	struct list_head *target;
	void             *opaque;
};

typedef bool (*g2_ptype_action_func) (struct ptype_action_args *);
_G2PACK_EXTRNVAR(const g2_ptype_action_func g2_packet_dict[PT_MAXIMUM])
_G2PACK_EXTRNVAR(const g2_ptype_action_func g2_packet_dict_udp[PT_MAXIMUM])
_G2PACK_EXTRN(bool g2_packet_decide_spec(struct ptype_action_args *, g2_ptype_action_func const *));
_G2PACK_EXTRN(bool g2_packet_search_finalize(uint32_t hashes[], size_t num, void *data, bool hubs));
_G2PACK_EXTRN(intptr_t g2_packet_hub_qht_match(g2_connection_t *con, void *data));
_G2PACK_EXTRN(intptr_t g2_packet_hub_qht_done(g2_connection_t *con, void *data));
_G2PACK_EXTRN(intptr_t g2_packet_leaf_qht_match(g2_connection_t *con, void *data));
_G2PACK_EXTRN(intptr_t send_HAW_callback(g2_connection_t *con, void *carg));

# endif /* _HAVE_G2_P_TYPE */
#endif /* _NEED_G2_P_TYPE */

#ifdef G2PACKET_POWER_MONGER
# include "lib/atomic.h"
struct packet_data_store
{
	atomic_t refcnt;
	char     data[DYN_ARRAY_LEN];
};
#endif /* G2PACKET_POWER_MONGER */

#endif /*! defined _G2PACKET_H || defined _NEED_G2_P_TYPE */
/* EOF */
