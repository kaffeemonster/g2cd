/*
 * G2PacketSerializer.h
 * header-file for G2PacketSerializer.c
 *
 * Copyright (c) 2004 - 2010 Jan Seiffert
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
 * $Id: G2PacketSerializer.h,v 1.2 2004/12/18 18:06:13 redbully Exp redbully $
 */

#ifndef G2PACKETSERIALIZER_H
# define G2PACKETSERIALIZER_H

# include "lib/other.h"
# include "lib/sec_buffer.h"
# include "G2PacketSerializerStates.h"

# ifndef _G2PACKETSERIALIZER_C
#  define _G2PACKSER_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#  define _G2PACKSER_EXTRNVAR(x) extern x
# else
#  define _G2PACKSER_EXTRN(x) x GCC_ATTR_VIS("hidden")
#  define _G2PACKSER_EXTRNVAR(x)
# endif /* _G2PACKET_C */

# include "G2Packet.h"

_G2PACKSER_EXTRN(bool g2_packet_decode(struct pointer_buff *, g2_packet_t *, int));
_G2PACKSER_EXTRN(bool g2_packet_decode_from_packet(g2_packet_t *, g2_packet_t *, int));
_G2PACKSER_EXTRN(bool g2_packet_extract_from_stream(struct norm_buff *, g2_packet_t *, size_t, bool));
_G2PACKSER_EXTRN(bool g2_packet_extract_from_stream_b(struct big_buff *, g2_packet_t *, size_t, bool));
_G2PACKSER_EXTRN(ssize_t g2_packet_serialize_prep(g2_packet_t *));
_G2PACKSER_EXTRN(ssize_t g2_packet_serialize_prep_min(g2_packet_t *));
_G2PACKSER_EXTRN(bool g2_packet_serialize_to_buff(g2_packet_t *, struct norm_buff *));
_G2PACKSER_EXTRN(bool g2_packet_serialize_to_buff_p(g2_packet_t *, struct pointer_buff *));

# ifdef DEBUG_DEVEL
_G2PACKSER_EXTRN(const char *g2_packet_decoder_state_name(int));
# else
#  define g2_packet_decoder_state_name(x) ""
# endif

#endif /* G2PACKETSERIALIZER_H */
/* EOF */
