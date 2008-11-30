/*
 * G2PacketSerializer.h
 * header-file for G2PacketSerializer.c
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
#  define _G2PACKSER_EXTRN(x) inline x GCC_ATTR_VIS("hidden")
#  define _G2PACKSER_EXTRNVAR(x)
# endif /* _G2PACKET_C */

# include "G2Packet.h"

_G2PACKSER_EXTRN(bool g2_packet_decode(struct pointer_buff *, g2_packet_t *, int));
_G2PACKSER_EXTRN(bool g2_packet_decode_from_packet(g2_packet_t *, g2_packet_t *, int));
_G2PACKSER_EXTRN(bool g2_packet_extract_from_stream(struct norm_buff *, g2_packet_t *, size_t));
_G2PACKSER_EXTRN(ssize_t g2_packet_serialize_prep(g2_packet_t *));
_G2PACKSER_EXTRN(ssize_t g2_packet_serialize_prep_min(g2_packet_t *));
_G2PACKSER_EXTRN(bool g2_packet_serialize_to_buff(g2_packet_t *, struct norm_buff *));

#endif /* G2PACKETSERIALIZER_H */
/* EOF */
