/*
 * G2PacketSerializer.h
 * header-file for G2PacketSerializer.c
 *
 * Copyright (c) 2004, Jan Seiffert
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
 * $Id: G2PacketSerializer.h,v 1.2 2004/12/18 18:06:13 redbully Exp redbully $
 */

#ifndef _G2PACKETSERIALIZER_H
#define _G2PACKETSERIALIZER_H

#include "lib/sec_buffer.h"

#ifndef _SERIALIZER_STATES_DEFINED
#define _SERIALIZER_STATES_DEFINED

#define G2_PACKET_DECODER_STATES \
	ENUM_CMD( CHECK_CONTROLL_BYTE ), \
	ENUM_CMD( READ_LENGTH ), \
	ENUM_CMD( READ_TYPE ), \
	ENUM_CMD( DECIDE_DECODE ), \
	ENUM_CMD( START_EXTRACT_PACKET_FROM_STREAM ), \
	ENUM_CMD( EXTRACT_PACKET_FROM_STREAM ), \
	ENUM_CMD( START_EXTRACT_PACKET_FROM_STREAM_TRUNK ), \
	ENUM_CMD( PACKET_EXTRACTION_COMPLETE ), \
	ENUM_CMD( GET_PACKET_DATA ), \
	ENUM_CMD( EXTRACT_PACKET_DATA ), \
	ENUM_CMD( FINISH_PACKET_DATA ), \
	ENUM_CMD( GET_CHILD_PACKETS ), \
	ENUM_CMD( DECODE_FINISHED ), \
	ENUM_CMD( MAX_DECODER_STATE) /* keep this entry the last */

#define ENUM_CMD(x) x
enum g2_packet_decoder_states
{
	G2_PACKET_DECODER_STATES
};
#undef ENUM_CMD

enum g2_packet_encoder_states
{
	CONCAT_HEADER, // = 50
	ENCODE_FINISHED, // = 100
};

#endif

#ifndef _G2PACKETSERIALIZER_C
#define _G2PACKSER_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#define _G2PACKSER_EXTRNVAR(x) extern x
#else
#define _G2PACKSER_EXTRN(x) inline x GCC_ATTR_VIS("hidden")
#define _G2PACKSER_EXTRNVAR(x)
#endif // _G2PACKET_C

#ifndef _NEED_ONLY_SERIALIZER_STATES
#include "G2Packet.h"

_G2PACKSER_EXTRN(bool g2_packet_decode(struct pointer_buff *, g2_packet_t *, int));
_G2PACKSER_EXTRN(bool g2_packet_decode_from_packet(g2_packet_t *, g2_packet_t *, int));
_G2PACKSER_EXTRN(bool g2_packet_extract_from_stream(struct norm_buff *, g2_packet_t *, size_t));
#else
#undef _G2PACKETSERIALIZER_H
#endif

#endif //_G2PACKETSERIALIZER_H
//EOF
