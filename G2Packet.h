/*
 * G2Packet.h
 * home of g2_packet_t and header-file for G2Packet.c
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
 * $Id: G2Packet.h,v 1.11 2004/12/18 18:06:13 redbully Exp redbully $
 */

#if ! defined _G2PACKET_H || defined _NEED_G2_P_TYPE
#ifndef _G2PACKET_H
#define _G2PACKET_H

#include <stdbool.h>
#include "other.h"
#define _NEED_ONLY_SERIALIZER_STATES
#include "G2PacketSerializer.h"
#undef _NEED_ONLY_SERIALIZER_STATES
#include "lib/sec_buffer.h"

typedef struct g2_packet
{
	// internal state
	enum g2_packet_decoder_states packet_decode;
	enum g2_packet_encoder_states packet_encode;
	size_t	length;
	uint8_t	length_length;
	uint8_t	type_length;
	bool		more_bytes_needed;

	// packet-data
	bool		child_is_freeable;
	struct g2_packet *children;
	size_t	num_child;
	char		type[16]; //8+1;
	bool		big_endian;
	bool		is_compound;
	bool		c_reserved;
	bool		data_trunk_is_freeable;

	struct pointer_buff data_trunk;
} g2_packet_t;

#ifndef _G2PACKET_C
#define _G2PACK_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#define _G2PACK_EXTRNVAR(x) extern x
#else
#define _G2PACK_EXTRN(x) x GCC_ATTR_VIS("hidden")
#define _G2PACK_EXTRNVAR(x)
#endif // _G2PACKET_C

#define g2_packet_free(x) _g2_packet_free((x), true)
_G2PACK_EXTRN(inline void _g2_packet_free(g2_packet_t *, int));
_G2PACK_EXTRN(inline void g2_packet_clean(g2_packet_t *to_clean));
	
#endif //_G2PACKET_H

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
	const struct g2_p_type *found;
};

typedef struct g2_p_type
{
	const struct g2_p_type	*next;
	const union p_type_action	work;
	const char	match;
	const bool	last;
} g2_p_type_t;

_G2PACK_EXTRNVAR(const g2_p_type_t g2_packet_dict;)
_G2PACK_EXTRN(inline bool g2_packet_decide(g2_connection_t *, struct norm_buff *, const g2_p_type_t *));
# endif // _HAVE_G2_P_TYPE
#endif // _NEED_G2_P_TYPE

#endif //! defined _G2PACKET_H || defined _NEED_G2_P_TYPE
//EOF
