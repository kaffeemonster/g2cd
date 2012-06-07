/*
 * G2PacketSerializerSates.h
 * packet serializer states exportet
 *
 * Copyright (c) 2008-2012 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd.
 * If not, see <http://www.gnu.org/licenses/>.
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

#define G2_PACKET_ENCODER_STATES \
	ENUM_CMD( DECIDE_ENCODE ), \
	ENUM_CMD( PREPARE_SERIALIZATION_MIN ), \
	ENUM_CMD( PREPARE_SERIALIZATION ), \
	ENUM_CMD( SERIALIZATION_PREPARED_MIN ), \
	ENUM_CMD( SERIALIZE_CONTROL ), \
	ENUM_CMD( SERIALIZE_LENGTH1 ), \
	ENUM_CMD( SERIALIZE_LENGTH2 ), \
	ENUM_CMD( SERIALIZE_LENGTH3 ), \
	ENUM_CMD( SERIALIZE_TYPE1 ), \
	ENUM_CMD( SERIALIZE_TYPE2 ), \
	ENUM_CMD( SERIALIZE_TYPE3 ), \
	ENUM_CMD( SERIALIZE_TYPE4 ), \
	ENUM_CMD( SERIALIZE_TYPE5 ), \
	ENUM_CMD( SERIALIZE_TYPE6 ), \
	ENUM_CMD( SERIALIZE_TYPE7 ), \
	ENUM_CMD( SERIALIZE_TYPE8 ), \
	ENUM_CMD( SERIALIZATION_PREPARED ), \
	ENUM_CMD( IOVEC_HEADER ), \
	ENUM_CMD( IOVEC_CHILD ), \
	ENUM_CMD( IOVEC_ZERO ), \
	ENUM_CMD( IOVEC_DATA ), \
	ENUM_CMD( IOVEC_CLEANAFTER ), \
	ENUM_CMD( SERIALIZE_CHILDREN ), \
	ENUM_CMD( BEWARE_OF_ZERO ), \
	ENUM_CMD( SERIALIZE_DATA_PREP ), \
	ENUM_CMD( SERIALIZE_DATA ), \
	ENUM_CMD( ENCODE_FINISHED ), \
	ENUM_CMD( MAX_ENCODER_STATE ) /* keep this entry the last */

# define ENUM_CMD(x) x
enum g2_packet_encoder_states
{
	G2_PACKET_ENCODER_STATES
} GCC_ATTR_PACKED;
# undef ENUM_CMD

#endif /* G2PACKETSERIALIZERSTATES_H */
