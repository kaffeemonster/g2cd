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
# include "other.h"
# include "G2PacketSerializerStates.h"
# include "lib/sec_buffer.h"

#define G2_PACKET_TYPES \
	ENUM_CMD( UNKNOWN ), \
	ENUM_CMD( KHL     ), \
	ENUM_CMD( LNI     ), \
	ENUM_CMD( PI      ), \
	ENUM_CMD( PO      ), \
	ENUM_CMD( QHT     ), \
	ENUM_CMD( UPROC   ), \
	ENUM_CMD( UPROD   ), \
	ENUM_CMD( QKR     ), \
	ENUM_CMD( QKA     ), \
	ENUM_CMD( HAW     ), \
	ENUM_CMD( CRAWLA  ), \
	ENUM_CMD( CRAWLR  ), \
	ENUM_CMD( G2CDC   ), \
	ENUM_CMD( TS      ), \
	ENUM_CMD( NA      ), \
	ENUM_CMD( HS      ), \
	ENUM_CMD( LS      ), \
	ENUM_CMD( GU      ), \
	ENUM_CMD( V       ), \
	ENUM_CMD( NH      ), \
	ENUM_CMD( CH      ), \
	ENUM_CMD( QK      ), \
	ENUM_CMD( FW      ), \
	ENUM_CMD( RLEAF   ), \
	ENUM_CMD( RNAME   ), \
	ENUM_CMD( RGPS    ), \
	ENUM_CMD( REXT    ), \
	ENUM_CMD( MAXIMUM )

#define ENUM_CMD(x) PT_##x
enum g2_ptype
{
	G2_PACKET_TYPES
} GCC_ATTR_PACKED;
#undef ENUM_CMD

typedef struct g2_packet
{
	/* internal state */
	size_t   length;
	enum g2_packet_decoder_states packet_decode;
	enum g2_packet_encoder_states packet_encode;
	uint8_t  length_length;
	uint8_t  type_length;
	bool     more_bytes_needed;
	bool     source_needs_compact;

	/* packet-data */
	struct g2_packet *children;
	size_t   num_child;
	bool     child_is_freeable;
	enum g2_ptype type;
	bool     big_endian;
	bool     is_compound;
	bool     c_reserved;
	bool     data_trunk_is_freeable;

	struct pointer_buff data_trunk;
} g2_packet_t;

# ifndef _G2PACKET_C
#  define _G2PACK_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#  define _G2PACK_EXTRNVAR(x) extern x
# else
#  define _G2PACK_EXTRN(x) inline x GCC_ATTR_VIS("hidden")
#  define _G2PACK_EXTRNVAR(x)
# endif /* _G2PACKET_C */

_G2PACK_EXTRN(g2_packet_t *g2_packet_alloc(void));
_G2PACK_EXTRN(g2_packet_t *g2_packet_calloc(void));
# define g2_packet_free(x) _g2_packet_free((x), true)
_G2PACK_EXTRN(void _g2_packet_free(g2_packet_t *, int));
_G2PACK_EXTRN(void g2_packet_clean(g2_packet_t *to_clean));
_G2PACK_EXTRN(void g2_packet_find_type(g2_packet_t *packet, const char type_str[16]));
_G2PACK_EXTRNVAR(const char *const g2_ptype_names[PT_MAXIMUM]);
	
#endif /* _G2PACKET_H */

#ifdef _NEED_G2_P_TYPE
# undef _NEED_G2_P_TYPE
# ifndef _HAVE_G2_P_TYPE
#  define _HAVE_G2_P_TYPE
#  include "lib/sec_buffer.h"
#  include "G2Connection.h"

struct g2_p_type;

union p_type_action
{
	bool (*action) (g2_connection_t *, g2_packet_t *, struct norm_buff *);
	const struct g2_p_type *next;
};

typedef struct g2_p_type
{
	const char match;
	const bool last;
	const bool term;
	const union p_type_action found;
} g2_p_type_t;

_G2PACK_EXTRNVAR(const g2_p_type_t g2_packet_dict[];)
_G2PACK_EXTRN(bool g2_packet_decide_spec(g2_connection_t *, struct norm_buff *, const g2_p_type_t *, g2_packet_t *));
# endif /* _HAVE_G2_P_TYPE */
#endif /* _NEED_G2_P_TYPE */

#endif /*! defined _G2PACKET_H || defined _NEED_G2_P_TYPE */
/* EOF */
