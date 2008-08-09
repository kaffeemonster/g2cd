/*
 * G2PacketSerializerSates.h
 * packet serializer states exportet
 *
 * Copyright (c) 2008 Jan Seiffert
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
 * $Id:$
 */

#ifndef G2PACKETSERIALIZERSTATES_H
# define G2PACKETSERIALIZERSTATES_H

# define G2_PACKET_DECODER_STATES \
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

# define ENUM_CMD(x) x
enum g2_packet_decoder_states
{
	G2_PACKET_DECODER_STATES
} GCC_ATTR_PACKED;
# undef ENUM_CMD

enum g2_packet_encoder_states
{
	CONCAT_HEADER, /* = 50 */
	ENCODE_FINISHED, /* = 100 */
} GCC_ATTR_PACK;

#endif /* G2PACKETSERIALIZERSTATES_H */
