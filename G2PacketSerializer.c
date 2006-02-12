/*
 * G2PacketSerializer.c
 * Serializer for G2-packets
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
 * $Id: G2PacketSerializer.c,v 1.4 2004/12/18 18:06:13 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
// System includes
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
// other
#include "other.h"
// Own includes
#define _G2PACKETSERIALIZER_C
//#define _NEED_G2_P_TYPE
#include "G2PacketSerializer.h"
#include "G2Packet.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"

static inline int check_control_byte_p(struct pointer_buff *source, g2_packet_t *target)
{
	uint8_t control;

	// get and interpret the control-byte of a packet
	if(1 > buffer_remaining(*source))
	{
		target->more_bytes_needed = true;
		return 0;
	}
			
	control = *buffer_start(*source);
	source->pos++;
	if(!control)
	{
		logg_devel("stream terminated '\\0'\n");
		return -1;
	}

	target->length_length = ((control >> 6) & 0x03);
	target->type_length = ((control >> 3) & 0x07) + 1;
	target->c_reserved = (control & 0x01) ? true : false;
	target->big_endian = (control & 0x02) ? true : false;
	target->is_compound = (control & 0x04) ? true : false;
	target->packet_decode++;

	return 1;
}

static inline int check_control_byte(struct norm_buff *source, g2_packet_t *target)
{
	struct pointer_buff tmp_buf =
		{.pos = source->pos,
		.limit = source->limit,
		.capacity = source->capacity,
		.data = source->data}; // refer to sec_buffer.h why this is needed
	int ret_val                 = check_control_byte_p(&tmp_buf, target);

	source->pos                 = tmp_buf.pos;
	source->limit               = tmp_buf.limit;
	
	return ret_val;
}

static inline int read_length_p(struct pointer_buff *source, g2_packet_t *target, size_t max_len)
{
	size_t i;

	// get the up to three length-bytes
	if(target->length_length > buffer_remaining(*source))
	{
		target->more_bytes_needed = true;
		return 0;
	}
			
	target->length = 0;
	for(i = 0; i < target->length_length; i++)
	{
		uint8_t tmp_byte = *buffer_start(*source);
		source->pos++;
		if(!target->big_endian)
			target->length |= (((size_t)tmp_byte) << (i*8));
		else
			target->length = (target->length << 8) | ((size_t)tmp_byte);
	}
// early check packet-boundary
	if(max_len < (target->length + target->length_length + target->type_length + 1))
	{
		logg_devel("packet too long!\n");
		return -1;
	}

// seems to be allowed to send a packet with compound flag and no data..
	if(!target->length)
		target->is_compound = false;
	
	target->packet_decode++;
	return 1;
}

static inline int read_length(struct norm_buff *source, g2_packet_t *target, size_t max_len)
{
	struct pointer_buff tmp_buf =
		{.pos = source->pos,
		.limit = source->limit,
		.capacity = source->capacity,
		.data = source->data}; // refer to sec_buffer.h why this is needed
	int ret_val                 = read_length_p(&tmp_buf, target, max_len);

	source->pos                 = tmp_buf.pos;
	source->limit               = tmp_buf.limit;
	
	return ret_val;
}

static inline int read_type_p(struct pointer_buff *source, g2_packet_t *target)
{
	// fetch the up to eigth type-bytes
	if(target->type_length > buffer_remaining(*source))
	{
		target->more_bytes_needed = true;
		return 0;
	}

	if(target->type_length)
	{
		char *w_ptr = target->type;
		size_t i;
		for(i = target->type_length; i; i--, w_ptr++, source->pos++)
		{
			*w_ptr = *buffer_start(*source);
	// A *VERY* simple test if the packet-type is legal,
	// hopefully we can detect de-sync-ing of the stream with this
			if(!isgraph((int)*w_ptr))
			{
				logg_devel("packet with bogus type-name\n");
				return -1;
			}
		}

		*w_ptr = '\0';
	}
	else
	{
		// Illegal Packet...
		logg_devel("packet with 0 type-length\n");
		return -1;
	}

	target->packet_decode++;
	return 1;
}

static inline int read_type(struct norm_buff *source, g2_packet_t *target)
{
	struct pointer_buff tmp_buf =
		{.pos = source->pos,
		.limit = source->limit,
		.capacity = source->capacity,
		.data = source->data}; // refer to sec_buffer.h why this is needed
	int ret_val                 = read_type_p(&tmp_buf, target);

	source->pos                 = tmp_buf.pos;
	source->limit               = tmp_buf.limit;
	
	return ret_val;
}

#if 0 
inline bool g2_packet_decode(struct pointer_buff *source, g2_packet_t *target, int level)
{
	size_t i;
	long remaining_length = 0;
	int func_ret_val;
	bool ret_val = true;

	target->more_bytes_needed = false;
	
	while(!target->more_bytes_needed)
	{
		switch(target->packet_decode)
		{
		case CHECK_CONTROLL_BYTE:
		// get and interpret the control-byte of a packet
			if(!(func_ret_val = check_control_byte_p(source, target)))
				break;
			else if(0 > func_ret_val)
				return false;
		case READ_LENGTH:
		// get the up to three length-bytes
		// we are makeing a little failure with the max_len, but only some bytes
			if(!(func_ret_val = read_length_p(source, target, buffer_remaining(*source))))
				break;
			else if(0 > func_ret_val)
				return false;
		case READ_TYPE:
		// fetch the up to eigth type-bytes
			if(!(func_ret_val = read_type_p(source, target)))
				break;
			else if(0 > func_ret_val)
				return false;
		case DECIDE_DECODE:
//			{
//#define str_app(x, y)		(x) = (char *)memcpy((x), (y), str_size(y)) + str_size(y)
//				char pr_buf[2048];
//				char *pr_ptr = pr_buf;
//				*pr_ptr = '\0';
//
//				for(i = level; i; i--) str_app(pr_ptr, "   +");
//				//pr_ptr = memcpy(pr_ptr, target->type, target->type_length) + target->type_length;
//				pr_ptr += sprintf(pr_ptr, "-Type: \"%s\"\tlength: %u 0x%02X\n", target->type, target->length, control);
//				for(i = level; i; i--) str_app(pr_ptr, "   +");
//				pr_ptr += sprintf(pr_ptr, "-ll: %hhu\ttl: %hhu\n", target->length_length, target->type_length);
//				for(i = level; i; i--) str_app(pr_ptr, "   +");
//				pr_ptr += sprintf(pr_ptr, "-big-e: %s\tid-c: %s\n--------------------\n", target->big_endian ? "true" : "false", target->is_compound ? "true" : "false");
//				log_develd("%s", pr_buf);
//			}

			target->data_trunk.pos      = 0;
			target->data_trunk.limit    = 0;
			target->data_trunk.capacity = 0;
			target->data_trunk.data     = NULL;

			if(0 < target->length)
			{
				if(target->is_compound)
					target->packet_decode = GET_CHILD_PACKETS;
				else
					target->packet_decode = GET_PACKET_DATA;
			}
			else
			{
			// Packet has no length -> DirectAction
				return true;
				target->packet_decode = DECODE_FINISHED;
			}
			break;
		case GET_PACKET_DATA:
			if(1 > buffer_remaining(*source))
			{
				target->packet_decode = FINISH_PACKET_DATA;
				break;
			}
			else if(0 > buffer_remaining(target->data_trunk))
			{
				// something realy went wrong
				logg_devel("Child Packets too long\n");
				return false;
			}

			
			if(target->num_child)
			{
				g2_packet_t *pack_ptr = target->children;
				remaining_length = target->length;

				for(i = target->num_child; i; i--, pack_ptr++)
					remaining_length -= (pack_ptr->length + pack_ptr->length_length + pack_ptr->type_length + 1);
				remaining_length--; // substract the Zero-byte after the child-packets
				if(0 < remaining_length)
				{
					target->data = malloc((unsigned)remaining_length);
					if(!target->data)
					{
						logg_errno(LOGF_DEBUG, "allocating packet buffer");
						return false;
					}
					target->data_length = remaining_length;
				}
			}
			else
			{
				target->data = malloc(target->length);
				if(!target->data)
				{
					logg_errno(LOGF_DEBUG, "allocating packet buffer");
					return false;
				}
				target->data_length = target->length;
			}
			target->packet_decode++;
		case EXTRACT_PACKET_DATA:
		// grep the Payload
			if(buffer_remaining(*source) < (target->data_length - target->data_pos))
			{
				memcpy(target->data + target->data_pos, buffer_start(*source), buffer_remaining(*source));
				target->data_pos += buffer_remaining(*source);
				source->pos += buffer_remaining(*source);
				target->more_bytes_needed = true;
				break;
			}
			else
			{
				memcpy(target->data + target->data_pos, buffer_start(*source), target->data_length - target->data_pos);
				source->pos += (target->data_length - target->data_pos);
				target->data_pos = 0;
				target->packet_decode++;
			}
		case FINISH_PACKET_DATA:
		// everything abord
			target->data_trunk_is_freeable = false;
			target->packet_decode = DECODE_FINISHED;
			target->more_bytes_needed = true;
			break;
		case GET_CHILD_PACKETS:
		// if we have some (is_compound && length != 0) get ChildPackete
			if(!remaining_length)
			{
				g2_packet_t *pack_ptr = target->children;
				remaining_length = target->length;

				for(i = target->num_child; i; i--, pack_ptr++)
					remaining_length -= (pack_ptr->length + pack_ptr->length_length + pack_ptr->type_length + 1);
			}

			if(target->num_child)
			{
				if(DECODE_FINISHED == target->children[target->num_child-1].packet_decode)
				{
					g2_packet_t *tmp_pack_ptr = realloc(target->children, (target->num_child + 1) * sizeof(*(target->children)));
					if(!tmp_pack_ptr)
					{
						logg_errno(LOGF_DEBUG, "reallocating child packets");
						return false;
					}
					target->children = tmp_pack_ptr;
					target->num_child++;
					tmp_pack_ptr = &target->children[target->num_child-1];
					tmp_pack_ptr->data = NULL;
					tmp_pack_ptr->num_child = 0;
					tmp_pack_ptr->packet_decode =  CHECK_CONTROLL_BYTE;
				}
			}
			else
			{
				target->children = calloc(1, sizeof(*(target->children)));
				if(!target->children)
				{
					logg_errno(LOGF_DEBUG, "allocating child packet");
					return false;
				}
				target->children->data = NULL;
				target->children->num_child = 0;
				target->children->packet_decode = CHECK_CONTROLL_BYTE;
				target->num_child = 1;
			}

			// call us recursivly with the buffer and the Childpacket
			ret_val = g2_packet_decode(source, &target->children[target->num_child-1], (size_t)remaining_length, level+1);
			// decide what next
			if(ret_val && (DECODE_FINISHED == target->children[target->num_child-1].packet_decode))
			{
				g2_packet_t *pack_ptr = target->children;
				remaining_length = target->length;

				for(i = target->num_child; i; i--, pack_ptr++)
					remaining_length -= (pack_ptr->length + pack_ptr->length_length + pack_ptr->type_length + 1);

				logg_develd_old("rml: %li\n", remaining_length);
				if(0 == remaining_length)
				{
					target->packet_decode = DECODE_FINISHED;
					return ret_val;
				}
				else if(1 == remaining_length)
				{
					buffer_flip(*source);
					if(buffer_remaining(*source) < 1) { target->more_bytes_needed = true; break;}
					if('\0' == *buffer_start(*source))
					{
						source->pos++;
						target->packet_decode = DECODE_FINISHED;
						target->more_bytes_needed = true;
						break;
					}
					else
					{
						source->pos++;
						logg_devel("no Zero-byte where one has to be\n");
						return false;
					}
				}
				else
				{
					if(0 < remaining_length)
					{
						buffer_flip(*source);
						if(buffer_remaining(*source) < 1) { target->more_bytes_needed = true; break; }
						if('\0' != *buffer_start(*source))
						{
							break;
							//target->more_bytes_needed = true;
						}
						else
						{
							source->pos++;
							target->packet_decode = GET_PACKET_DATA;
						}
						break;
					}
					else
					{
						logg_devel("Child Packets too long\n");
						return false;
					}
				}
			}
			else return ret_val;
			break;
		case DECODE_FINISHED:
		// Yehaa, it's done!
			target->more_bytes_needed = true;
			break;
		default:
			logg_devel("bogus decoder_state\n");
			return false;
		}
	}

	if(target->more_bytes_needed)
		return false;
	
	// do not remove decoded data and set buffer-position, since we have a snapshot
//	if(buffer_remaining(*source))
//	buffer_compact(*source);
//	else buffer_clear(*source);

	return ret_val;
}
#endif

/*
 * First break out (decode) of a G2Packet from a byte stream
 *
 * Params:
 * source - the hot (recv-)buffer with the uncompressed G2 stream data
 * target - 
 * max_len - the maximal legal G2Packet length. If a packet above this is found,
 *   it is seen as an error condition (see RetVal).
 * 
 * RetVal:
 *  true - everything within normal parameters, all systems go.
 *  false - unrecoverable error (or illegal data), just dump the source of this
 *    byte waste.
 */
inline bool g2_packet_extract_from_stream(struct norm_buff *source, g2_packet_t *target, size_t max_len)
{
	int func_ret_val;
	bool ret_val = true;

	target->more_bytes_needed = false;
	
	while(!target->more_bytes_needed)
	{
		switch(target->packet_decode)
		{
		case CHECK_CONTROLL_BYTE:
		// get and interpret the control-byte of a packet
			if(!(func_ret_val = check_control_byte(source, target)))
				break;
			else if(0 > func_ret_val)
				return false;
		case READ_LENGTH:
		// get the up to three length-bytes
			if(!(func_ret_val = read_length(source, target, max_len)))
				break;
			else if(0 > func_ret_val)
				return false;
		case READ_TYPE:
		// fetch the up to eigth type-bytes
			if(!(func_ret_val = read_type(source, target)))
				break;
			else if(0 > func_ret_val)
				return false;
		case DECIDE_DECODE:
/*			{
#define str_app(x, y)		(x) = (char *)memcpy((x), (y), str_size(y)) + str_size(y)
				char pr_buf[2048];
				char *pr_ptr = pr_buf;
				*pr_ptr = '\0';

				pr_ptr += sprintf(pr_ptr, "-Type: \"%s\"\tlength: %lu 0x%02X\n", target->type, (unsigned long)target->length, (unsigned)control);
//				pr_ptr += sprintf(pr_ptr, "-ll: %hhu\ttl: %hhu\n", target->length_length, target->type_length);
//				pr_ptr += sprintf(pr_ptr, "-big-e: %s\tid-c: %s\n--------------------\n", target->big_endian ? "true" : "false", target->is_compound ? "true" : "false");
				logg_develd("%s", pr_buf);
			}*/

			if(0 < target->length)
			{
				target->packet_decode = START_EXTRACT_PACKET_FROM_STREAM;
			}
			else
			{
			// Packet has no length -> DirectAction
				target->data_trunk.pos = target->data_trunk.limit = 0;
				target->more_bytes_needed = true;
				target->packet_decode = DECODE_FINISHED;
			}
			break;
		case START_EXTRACT_PACKET_FROM_STREAM:
		// look what have to be done to extract the data
			if(target->length > target->data_trunk.capacity)
			{
				char *tmp_ptr = realloc(target->data_trunk.data, target->length);
				if(!tmp_ptr)
				{
					logg_errno(LOGF_DEBUG, "reallocating packet space");
					return false;
				}

				target->data_trunk.data = tmp_ptr;
				target->data_trunk.capacity = target->length;
				logg_develd_old("%p -> packet space reallocated: %lu bytes\n", (void *) target, (unsigned long) target->length);
				buffer_clear(target->data_trunk);
			}
			else
			{
				target->data_trunk.pos = 0;
				target->data_trunk.limit = target->length;
			}
			target->packet_decode++;
		case EXTRACT_PACKET_FROM_STREAM:
		// grep payload
			if(buffer_remaining(*source) < buffer_remaining(target->data_trunk))
			{
				size_t buff_remain_source = buffer_remaining(*source);
				memcpy(buffer_start(target->data_trunk), buffer_start(*source), buff_remain_source);
				target->data_trunk.pos += buff_remain_source;
				source->pos += buff_remain_source;
				target->more_bytes_needed = true;
				break;
			}
			else
			{
				size_t buff_remain_target = buffer_remaining(target->data_trunk);
				memcpy(buffer_start(target->data_trunk), buffer_start(*source), buff_remain_target);
				target->data_trunk.pos += buff_remain_target;
				source->pos += buff_remain_target;
//				target->data_pos = 0;
				target->packet_decode++;
			}
		case PACKET_EXTRACTION_COMPLETE:
		// everythings fine for now, next would be the childpackets  if some
			buffer_flip(target->data_trunk);
			target->more_bytes_needed = true;
			break;
		case GET_PACKET_DATA:
		case EXTRACT_PACKET_DATA:
		case FINISH_PACKET_DATA:
		case GET_CHILD_PACKETS:
		// if we have this states in this funktion, someone filled us with a
		// packet, wich should be filled in the real decoder function
			logg_devel("wrong packet in extraction\n");
			return false;
		case DECODE_FINISHED:
		// Yehaa, it's done!
			target->more_bytes_needed = true;
			break;
		default:
			logg_devel("bogus decoder_state\n");
			return false;
		}
	}

	// remove decoded data and set buffer-position so new data gets written behind
	buffer_compact(*source);

	return ret_val;
}

/*	public boolean encode(ByteBuffer target, G2Packet source, int level)
	{
		source.more_bytes_needed = false;
		
		while(!source.more_bytes_needed)
		{
		// Wir mussendas Packet ruckwaerts vorwaerts zusammenfuegen
			switch(source.packet_encode)
			{
			case 0:
			// Packetinformationen zuruecksetzen und, wenn vorhanden, Payload Anfuegen
				source.length = 0;
				source.length_length = 0;
				source.type_length = 0;
				source.big_endian = false;
				source.is_compound = false;
				source.c_reserved = false;

				if(null != source.data)
				{
					source.data.rewind();
					int old_remaining = source.data.remaining();
				// wenn wir Payload und Children haben, Nullbyte einplanen
					if(null != source.children) old_remaining++;
				//	if(target.remaining() < old_remaining) { source.more_bytes_needed = true; break; }
					target.position(target.limit() - old_remaining);
				// ggf. Nullbyte auch einfuegen
					if(null != source.children) target.put((byte)0x00);
					target.put(source.data).rewind();
					target.limit(target.limit() - old_remaining);
					source.length += old_remaining;
				}
				source.packet_encode++;
			case 1:
			// wenn vorhanden Children in den Bytestream einfuegen
				if(null != source.children)
				{
					for(int i = source.children.length - 1; i >= 0; i--)
					{
						if((null != source.children[i]) && (ENCODE_FINISHED != source.children[i].packet_encode))
						{
						// Wenn es was zu tun gibt, uns selbst aufrufen mit dem puffer und dem Childpacket
							boolean ret_val = encode(target, source.children[i], (level + 1));
							if(ret_val && (ENCODE_FINISHED == source.children[i].packet_encode))
							{
								source.length += source.children[i].length + source.children[i].length_length + source.children[i].type_length + 1;
							}
							else
							{
								source.more_bytes_needed = source.children[i].more_bytes_needed;
								return(ret_val);
							}
						}
					}
				}
				source.packet_encode = CONCAT_HEADER;
			case CONCAT_HEADER:
			// die Acht moeglichen Type-Bytes einfuegen
			//	if(target.remaining() < source.type_length) { source.more_bytes_needed = true; break; }
				if(null != source.type)
				{
					ByteBuffer b_type = std_utf8.encode(source.type);
					source.type_length = (byte)b_type.remaining();
					if(8 < source.type_length)
					{
						// ?? wer hat nen Type mit mehr als 8 Buchstaben zugelassen ??
						System.out.println("Packet mit zu langem Typ! Werde kuerzen!");
						b_type.limit(b_type.position() + 8);
						source.type_length = 8;
					}
					target.position(target.limit() - source.type_length);
					target.put(b_type).rewind();
					target.limit(target.limit() - source.type_length);
				}
				else
				{
					// da wollen wir uns wohl selbst ein ungueltiges Packet unterjubeln
					System.out.println("ACHTUNG! Packet mit Nullreferenz als Typ!\nVersuche es zu droppen...");
					return(false);
				}
				source.packet_encode++;
			case CONCAT_HEADER + 1:
			// die drei moeglichen Laengenbytes einfuegen
				if(0 < source.length)
				{
					source.length_length = 1;
					if(0 != (source.length & 0x0000FF00)) source.length_length++;
					if(0 != (source.length & 0x00FF0000)) source.length_length++;
					
				//	if(target.remaining() < source.length_length) { source.more_bytes_needed = true; break; }
					target.position(target.limit() - source.length_length);
					for(byte i = 0; i < source.length_length; i++)
					{
				// Da Java ja leider unfaehig ist einem unsigned zu geben...
						byte cast_byte = 0;
						if(!source.big_endian) cast_byte = (byte)(source.length >> (i * 8));
						else cast_byte = (byte)(source.length >> (source.length_length - i - 1));
						target.put(cast_byte);
					}
					target.rewind();
					target.limit(target.limit() - source.length_length);
				}
				source.packet_encode++;
			case CONCAT_HEADER + 2:
			// Controllbyte erzeugen
				//if(target.remaining() < 1) { source.more_bytes_needed = true; break; }
				control = 0;
				control |= (byte)((source.length_length << 6) & 0xC0);
				control |= (byte)(((source.type_length - 1) << 3) & 0x38);
				if(source.c_reserved) control |= (byte)0x01;
				if(source.big_endian) control |= (byte)0x02;
				if(source.is_compound) control |= (byte)0x04;
				if(0 == control) control |= (byte)0x04;
				target.position(target.limit() - 1);
				target.put(control).rewind();
				target.limit(target.limit() - 1);
				source.packet_encode = ENCODE_FINISHED;
				//break;
			case ENCODE_FINISHED:
				return(true);
			}
		}

		return(false); //wenn wir hier ankommen werden wir wohl mehr bytes brauchen im Puffer
	}
	// EOC
}

*/

static char const rcsid[] GCC_ATTR_USED_VAR = "$Idi$";
//EOF
