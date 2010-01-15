/*
 * G2PacketSerializer.c
 * Serializer for G2-packets
 *
 * Copyright (c) 2004-2009 Jan Seiffert
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
 * $Id: G2PacketSerializer.c,v 1.4 2004/12/18 18:06:13 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* System includes */
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_MALLOC_H
# include <malloc.h>
#elif defined(HAVE_MALLOC_NP_H)
# include <malloc_np.h>
#endif
/* other */
#include "lib/other.h"
/* Own includes */
#define _G2PACKETSERIALIZER_C
#define G2PACKET_POWER_MONGER
#include "G2PacketSerializer.h"
#include "G2Packet.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"

// #define PS_CHECK_STATES
/********************************************************************
 *
 * local vars
 *
 ********************************************************************/
#ifdef DEBUG_DEVEL
# define ENUM_CMD(x) str_it(x)
static const char *g2_packet_decoder_states_txt[] =
{
	G2_PACKET_DECODER_STATES
};

static const char *g2_packet_encoder_states_txt[] =
{
	G2_PACKET_ENCODER_STATES
};
# undef ENUM_CMD
#endif

// #define DEBUG_SERIALIZER
#ifdef DEBUG_SERIALIZER
/********************************************************************
 *
 * helper
 *
 ********************************************************************/
static inline void stat_packet(g2_packet_t *x, int level)
{
# define str_app(x, y)		(x) = (char *)memcpy((x), (y), str_size(y)) + str_size(y)
	char pr_buf[2048];
	char *pr_ptr = pr_buf;
	int i;
	*pr_ptr++ = '\n'; *pr_ptr = '\0';

	for(i = level; i; i--) str_app(pr_ptr, "   +");
	pr_ptr += sprintf(pr_ptr, "-Type: \"%s\"\tlength: %u\n", g2_ptype_names[x->type], x->length);
	for(i = level; i; i--) str_app(pr_ptr, "   +");
	pr_ptr += sprintf(pr_ptr, "-ll: %hhu\ttl: %hhu\n", x->length_length, x->type_length);
	for(i = level; i; i--) str_app(pr_ptr, "   +");
	pr_ptr += sprintf(pr_ptr, "-big-e: %s\tid-c: %s\n--------------------\n",
		x->big_endian ? "true" : "false", x->is_compound ? "true" : "false");
	logg_develd("%s", pr_buf);
# undef str_app
}
#else
# define stat_packet(x, level) do { level = level; } while(0)
#endif


/********************************************************************
 *
 * extracter funcs
 *
 ********************************************************************/

static inline int check_control_byte_p(struct pointer_buff *source, g2_packet_t *target)
{
	uint8_t control;

	/* get and interpret the control-byte of a packet */
	if(unlikely(1 > buffer_remaining(*source))) {
		target->more_bytes_needed = true;
		return 0;
	}
			
	control = *buffer_start(*source);
	source->pos++;
	if(unlikely(!control)) {
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
		.data = source->data}; /* refer to sec_buffer.h why this is needed */
	int ret_val                 = check_control_byte_p(&tmp_buf, target);

	source->pos                 = tmp_buf.pos;
	source->limit               = tmp_buf.limit;
	
	return ret_val;
}

static inline int read_length_p(struct pointer_buff *source, g2_packet_t *target, size_t max_len)
{
	size_t i;

	/* get the up to three length-bytes */
	if(unlikely(target->length_length > buffer_remaining(*source)))
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
/* early check packet-boundary */
	i = target->length + target->length_length + target->type_length + 1;
	if(unlikely(max_len < i)) {
		logg_develd("packet too long! max: %zu tl: %zu\n", max_len, i);
		return -1;
	}

/*
 * seems to be allowed to send a packet with compound flag and
 * no data to prevent a zero control byte
 * Fix it up, so we do not choke on it later on.
 */
	if(unlikely(!target->length))
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
		.data = source->data}; /* refer to sec_buffer.h why this is needed */
	int ret_val                 = read_length_p(&tmp_buf, target, max_len);

	source->pos                 = tmp_buf.pos;
	source->limit               = tmp_buf.limit;
	
	return ret_val;
}

static inline int read_type_p(struct pointer_buff *source, g2_packet_t *target)
{
	union
	{
		char type_str[16]; /* 8 + 2 */
		uint64_t type_ll[2];
	} u;

	/* fetch the up to eigth type-bytes */
	if(unlikely(target->type_length > buffer_remaining(*source))) {
		target->more_bytes_needed = true;
		return 0;
	}

	/*
	 * Since a type can be 8 byte long, it is also a uint64_t. And
	 * because we need !two! zeros behind the type, we set the !two!
	 * u64 to 0, this should be 2 or 4 moves.
	 * One day we might change the packet typer to a "magic 64Bit
	 * interpreter", which means you treat the u64 with 0 padding at
	 * the end as one big number, to lookup (hashtable, what ever,
	 * don't know, it't the super duper thing shareaza is doing).
	 * instead of our byte-by-byte table interpreter.
	 */
	u.type_ll[0] = 0;
	u.type_ll[1] = 0;

	if(likely(target->type_length))
	{
		char *t_ptr = u.type_str, *s_ptr = buffer_start(*source);
		char *h_ptr = &target->pd.in.type[0], c;
		size_t i;

		for(i = target->type_length, source->pos += i; likely(i); i--)
		{
			c = *s_ptr++;
	/*
	 * A *VERY* simple test if the packet-type is legal,
	 * hopefully we can detect de-sync-ing of the stream with this
	 */
			if(unlikely(c < 0x20 || c & 0x80)) {
				logg_devel("packet with bogus/ugly type-name\n");
				return -1;
			}
			/* save unmodified header for forwarding */
			*h_ptr++ = c;
			*t_ptr++ = c;
		}
	} else {
		/* Normaly can not happen, Illegal Packet... */
		logg_devel("packet with 0 type-length\n");
		return -1;
	}

	g2_packet_find_type(target, u.type_str);

	target->packet_decode++;
	return 1;
}

static inline int read_type(struct norm_buff *source, g2_packet_t *target)
{
	struct pointer_buff tmp_buf =
		{.pos = source->pos,
		.limit = source->limit,
		.capacity = source->capacity,
		.data = source->data}; /* refer to sec_buffer.h why this is needed */
	int ret_val                 = read_type_p(&tmp_buf, target);

	source->pos                 = tmp_buf.pos;
	source->limit               = tmp_buf.limit;
	
	return ret_val;
}


/********************************************************************
 *
 * decoder entry points
 *
 ********************************************************************/

/*
 * First break out (decode) of a G2Packet from a buffer
 *
 * It is called from decode_from_packet to decode Child packets out from the payload.
 * The Childpacket is suited to be feed in decode_from_packet again to get childs of childs
 * (as long the Packets are consistent).
 *
 * !! Only feed it with complete Packtes/buffers !!
 * It assumes the Packet is complete.
 *
 * Params:
 * source - the buffer where a Packet is to be found, payload of a packet
 * target - The (child)packet where the info is stored
 * level - the maximal recursion allowed, old parameter, once func recursed atomatically
 * 	now only used for debuging, but maybe again of use. Simply pass 0.
 *
 * RetVal:
 *  true - everything within normal parameters, all systems go.
 *  false - unrecoverable error (or illegal data), just dump the source of this
 *    byte waste.
 */
bool g2_packet_decode(struct pointer_buff *source, g2_packet_t *target, int level)
{
	int func_ret_val;
	bool ret_val = true;

	target->more_bytes_needed = false;
	while(likely(!target->more_bytes_needed))
	{
		switch(target->packet_decode)
		{
		case CHECK_CONTROLL_BYTE:
		/* get and interpret the control-byte of a packet */
			if(!(func_ret_val = check_control_byte_p(source, target)))
				break;
			else if(0 > func_ret_val)
				return false;
		case READ_LENGTH:
		/*
		 * get the up to three length-bytes
		 * we are here in the save haven, the packet is alredy fetched,
		 * so we can take buffer_remaining for the max_length.
		 * BUT: We already removed the control byte, so +1
		 */
			if(!(func_ret_val = read_length_p(source, target, buffer_remaining(*source) + 1)))
				break;
			else if(0 > func_ret_val)
				return false;
		case READ_TYPE:
		/* fetch the up to eigth type-bytes */
			if(!(func_ret_val = read_type_p(source, target)))
				break;
			else if(0 > func_ret_val)
				return false;

		case DECIDE_DECODE:
			stat_packet(target, level);
			target->data_trunk.pos      = 0;
			target->data_trunk.data     = NULL;

			if(likely(0 < target->length))
				target->packet_decode = GET_PACKET_DATA;
			else
			{
				/* Packet has no length -> DirectAction */
				target->data_trunk.limit = target->data_trunk.capacity = 0;
				target->packet_decode    = DECODE_FINISHED;
				return true;
			}
		case GET_PACKET_DATA:
			/* length is checked above (READ_LENGTH) to not exceed buffer */
			target->data_trunk_is_freeable = false;
			target->data_trunk.limit       = target->length;
			target->data_trunk.capacity    = target->length;
			target->data_trunk.data        = buffer_start(*source);
			if(!target->is_compound)
				target->packet_decode       = DECODE_FINISHED;
			else
				target->packet_decode       = GET_CHILD_PACKETS;
			source->pos                   += target->length;
			return true;
		case DECODE_FINISHED:
		/* Yehaa, it's done! */
			target->more_bytes_needed = true;
			break;
		case START_EXTRACT_PACKET_FROM_STREAM:
		case START_EXTRACT_PACKET_FROM_STREAM_TRUNK:
		case PACKET_EXTRACTION_COMPLETE:
		case EXTRACT_PACKET_FROM_STREAM:
		case EXTRACT_PACKET_DATA:
		case FINISH_PACKET_DATA:
		case GET_CHILD_PACKETS:
		case MAX_DECODER_STATE:
			/* these Packets should not arrive here */
			logg_develd("wrong packet in decode: %i %s\n", target->packet_decode,
				g2_packet_decoder_states_txt[target->packet_decode]);
			return false;
#ifndef PS_CHECK_STATES
		default:
			logg_develd("bogus decoder_state: %i\n", target->packet_decode);
			return false;
#endif
		}
	}

	if(target->more_bytes_needed)
		return false;

	return ret_val;
}

/*
 * Continue to decode a G2Packet
 *
 * Its suited to be called with the Packet extracted from a stream to recive the children.
 * It will return for every children to the caller. So call it until PACKED_FINISHED.
 * After that look for additional data in packet.
 *
 * !! Only feed it with complete Packtes !!
 * It assumes the Packet is complete.
 *
 * Params:
 * source - the recved Packet, not finaly decoded
 * target - The (child)packet where the info is stored
 * level - the maximal recursion allowed, old parameter, once func recursed atomatically
 * 	now only used for debuging, but maybe again of use. Simply pass 0.
 *
 * RetVal:
 *  true - everything within normal parameters, all systems go.
 *  false - unrecoverable error (or illegal data), just dump the source of this
 *    byte waste.
 */
bool g2_packet_decode_from_packet(g2_packet_t *source, g2_packet_t *target, int level)
{
	bool ret_val = true;
	size_t remaining_length;

	while(likely(!target->more_bytes_needed))
	{
		switch(source->packet_decode)
		{
		case PACKET_EXTRACTION_COMPLETE:
			source->packet_decode = DECIDE_DECODE;
		case DECIDE_DECODE:
			stat_packet(source, level);

			if(0 < source->length)
			{
				if(source->is_compound)
					source->packet_decode = GET_CHILD_PACKETS;
				else
					source->packet_decode = GET_PACKET_DATA;
			}
			else {
				/* Packet has no length -> DirectAction */
				source->packet_decode = DECODE_FINISHED;
				return true;
			}
			break;
		case GET_CHILD_PACKETS:
			remaining_length = buffer_remaining(source->data_trunk);
			/* nothing left? */
			if(0 == remaining_length) {
				/* thats it (there don't have to be an '\0' if no data after childs) */
				source->packet_decode = DECODE_FINISHED;
				return true;
			}
			else
			{
				/*
				 * we have one or more bytes left, check if the child terminator
				 * is in place
				 */
				if('\0' == *buffer_start(source->data_trunk))
				{
					/*
					 * ok, here we are also finished, maybe there are bytes behind
					 * the '\0', but thats not our problem, thats data for this packet
					 */
					source->data_trunk.pos++;
					source->packet_decode = DECODE_FINISHED;
					return true;
				}
				else
				{
					/* at least 2 bytes left ? */
					if(likely(1 < remaining_length)) {
						ret_val = g2_packet_decode(&source->data_trunk, target, level + 1);
						/* break after a decode, so caller can handle it */
						return ret_val;
					}
					/* is there only this byte? */
					else if(unlikely(1 == remaining_length))
					{
						/* 
						 * Whoa! thats wrong, one byte cannot be a child,
						 * but no '\0' is also wrong
						 */
						source->data_trunk.pos++;
						logg_devel("no zero byte where one has to be\n");
						return false;
					}
				}
			}
			break;
		case DECODE_FINISHED:
		/* Yehaa, it's done! */
			target->more_bytes_needed = true;
			break;
		case CHECK_CONTROLL_BYTE:
		case READ_LENGTH:
		case READ_TYPE:
		case START_EXTRACT_PACKET_FROM_STREAM:
		case START_EXTRACT_PACKET_FROM_STREAM_TRUNK:
		case EXTRACT_PACKET_FROM_STREAM:
		case GET_PACKET_DATA:
		case EXTRACT_PACKET_DATA:
		case FINISH_PACKET_DATA:
		case MAX_DECODER_STATE:
			/* these Packets should not arrive here */
			logg_develd("wrong packet in extraction: %i %s\n", source->packet_decode,
				g2_packet_decoder_states_txt[source->packet_decode]);
			return false;
#ifndef PS_CHECK_STATES
		default:
			logg_develd("bogus decoder_state: %i\n", source->packet_decode);
			return false;
#endif
		}
	}

	return ret_val;
}


/*
 * First break out (decode) of a G2Packet from a byte stream
 *
 * Its suited to be called in sucsession when recieving bytes
 *
 * Params:
 * source - the hot (recv-)buffer with the uncompressed G2 stream data
 * target - The packet where the info is stored
 * max_len - the maximal legal G2Packet length. If a packet above this is found,
 *   it is seen as an error condition (see RetVal).
 *
 * RetVal:
 *  true - everything within normal parameters, all systems go.
 *  false - unrecoverable error (or illegal data), just dump the source of this
 *    byte waste.
 */
bool g2_packet_extract_from_stream(struct norm_buff *source, g2_packet_t *target, size_t max_len)
{
	int func_ret_val;
	bool ret_val = true;

	target->more_bytes_needed = false;
	while(likely(!target->more_bytes_needed))
	{
		switch(target->packet_decode)
		{
		case CHECK_CONTROLL_BYTE:
		/* get and interpret the control-byte of a packet */
			func_ret_val = check_control_byte(source, target);
			if(unlikely(0 == func_ret_val))
				break;
			else if(unlikely(0 > func_ret_val))
				return false;
		case READ_LENGTH:
		/* get the up to three length-bytes */
			func_ret_val = read_length(source, target, max_len);
			if(unlikely(0 == func_ret_val))
				break;
			else if(unlikely(0 > func_ret_val))
				return false;
		case READ_TYPE:
		/* fetch the up to eigth type-bytes */
		 	func_ret_val = read_type(source, target);
		 	if(unlikely(0 == func_ret_val))
				break;
			else if(unlikely(0 > func_ret_val))
				return false;
		case DECIDE_DECODE:
// TODO: since we know the type now, we may want to play games with skipping
		/* we have to obay a /TO before we can skip anything! */
			func_ret_val = 0;
			stat_packet(target, func_ret_val);

			if(likely(0 < target->length))
				target->packet_decode = START_EXTRACT_PACKET_FROM_STREAM;
			else
			{
			/* Packet has no length -> DirectAction */
				target->data_trunk.pos = target->data_trunk.limit = 0;
				target->more_bytes_needed = true;
				target->packet_decode = DECODE_FINISHED;
			}
			break;
		case START_EXTRACT_PACKET_FROM_STREAM:
		/* look what has to be done to extract the data */
			/* do we have a trunk? */
			if(!target->data_trunk_is_freeable  ||
			   0 == target->data_trunk.capacity ||
			   !target->data_trunk.data)
			{
				/* we do not seem to have a trunk, try to attach the read buffer */
	/*
	 * We are playing with fire here!
	 * data < pos is seen as free, but we linked it to this packet
	 * as long as we do not compact this buffer, data < pos
	 * will stay were it is.
	 *
	 * We are assuming, that a packet with PACKET_EXTRACTION_COMPLETE
	 * is imideatly handeld in one go. See G2Handler.c
	 *
	 */
				/* all data delivered? */
				if(likely(target->length <= buffer_remaining(*source)))
				{
					logg_develd_old("%p would be complete: %lu long %lu remaining\n", (void *)target,
						(unsigned long) target->length, (unsigned long)buffer_remaining(*source));
					target->data_trunk_is_freeable = false;
					target->data_trunk.data = buffer_start(*source);
					target->data_trunk.capacity = target->length;
					source->pos += target->length;
					buffer_clear(target->data_trunk);
					target->data_trunk.pos = target->length;
					target->packet_decode = PACKET_EXTRACTION_COMPLETE;
					break;
				}
				/* could this packet stuffed in this buffer at all? */
				else if(target->length <= source->capacity)
				{
					/* ok, recv more bytes */
					target->more_bytes_needed = true;
					break;
				}
				else
					target->packet_decode++;
			}
		case START_EXTRACT_PACKET_FROM_STREAM_TRUNK:
			/* is our trunk big enough? */
			if(target->length > target->data_trunk.capacity)
			{
				/* no, realloc */
				char *tmp_ptr = NULL;
				if(target->data_trunk_is_freeable ||
				   target->length > sizeof(target->pd.in.buf))
				{
					struct packet_data_store *pds;

					if(target->data_trunk.data)
					{
						struct packet_data_store *tmp_pds =
							container_of(target->data_trunk.data, struct packet_data_store, data);
// TODO: check refcnt for fun and profit
						/* a packet used for input should not have a shared data storage */
#ifdef HAVE_MALLOC_USABLE_SIZE
						size_t block_size = malloc_usable_size(tmp_pds);
						/*
						 * reallocating may save us a big cycle through the allocator
						 * because of usable slack behind allocation, but if this fails
						 * nothing is gained and copying like the std.func is mandated
						 * to do makes it much worse in our case, because we want to
						 * overwrite the buffer with new data.
						 * We can try to avoid it, but it is system dependent...
						 */
						if(block_size < target->length)
						{
							free(tmp_pds);
							target->data_trunk_is_freeable = false;
							memset(&target->data_trunk, 0, sizeof(target->data_trunk));
							pds = malloc(sizeof(*pds) + target->length);
						}
						else
#endif
							pds = realloc(tmp_pds, sizeof(*pds) + target->length);
					}
					else
						pds = malloc(sizeof(*pds) + target->length);

					if(!pds) {
						logg_errno(LOGF_DEBUG, "reallocating packet space");
						return false;
					}
					atomic_set(&pds->refcnt, 1);

					tmp_ptr = pds->data;
					target->data_trunk_is_freeable = true;
					target->data_trunk.capacity = target->length;
				}
				else
				{
					target->data_trunk_is_freeable = false;
					tmp_ptr = target->pd.in.buf;
					target->data_trunk.capacity = sizeof(target->pd.in.buf);
				}

				target->data_trunk.data = tmp_ptr;
				logg_develd("%p -> packet space %p reallocated: %lu bytes\n", (void *) target, (void *) target->data_trunk.data, (unsigned long) target->length);
			}
			target->data_trunk.pos = 0;
			target->data_trunk.limit = target->length;
			target->packet_decode++;
		case EXTRACT_PACKET_FROM_STREAM:
		/* grep payload */
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
				target->packet_decode++;
			}
		case PACKET_EXTRACTION_COMPLETE:
		/* everythings fine for now, next would be the childpackets, if some */
			buffer_flip(target->data_trunk);
			target->more_bytes_needed = true;
			break;
		case GET_PACKET_DATA:
		case EXTRACT_PACKET_DATA:
		case FINISH_PACKET_DATA:
		case GET_CHILD_PACKETS:
		case MAX_DECODER_STATE:
		/*
		 * if we have this states in this funktion, someone filled us with a
		 * packet, wich should be filled in the real decoder function
		 */
			logg_develd("wrong packet in extraction: %i %s\n", target->packet_decode,
				g2_packet_decoder_states_txt[target->packet_decode]);
			return false;
		case DECODE_FINISHED:
		/* Yehaa, it's done! */
			target->more_bytes_needed = true;
			break;
#ifndef PS_CHECK_STATES
		default:
			logg_develd("bogus decoder_state: %i\n", target->packet_decode);
			return false;
#endif
		}
	}

	return ret_val;
}

/*static inline bool g2_packet_extract_from_stream_xxx(struct norm_buff *source, g2_packet_t *target, size_t max_len)
{
	struct pointer_buff tmp_buf =
		{.pos = source->pos,
		 .limit = source->limit,
		 .capacity = source->capacity,
		 .data = source->data}; // refer to sec_buffer.h why this is needed
	bool ret_val                = g2_packet_extract_from_stream(&tmp_buf, target, max_len);

	source->pos                 = tmp_buf.pos;
	source->limit               = tmp_buf.limit;
	
	return ret_val;
}*/


#if 0
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

ssize_t g2_packet_serialize_to_iovec(g2_packet_t *p, struct iovec vec[], size_t vlen)
{
	static const size_t priv_zero = 0;
	size_t vused = 0;
	ssize_t ret;

	switch(p->packet_encode)
	{
	case DECIDE_ENCODE:
		p->packet_encode = PREPARE_SERIALIZATION;
	case SERIALIZATION_PREPARED_MIN:
	case PREPARE_SERIALIZATION:
	case PREPARE_SERIALIZATION_MIN:
		ret = g2_packet_serialize_prep(p);
		if(-1 == ret)
			return ret;
	case SERIALIZATION_PREPARED:
		p->packet_encode = IOVEC_HEADER;
	case IOVEC_HEADER:
		if(!vlen || vused == vlen)
			break;
		vec[vused].iov_base  = p->data;
		vec[vused++].iov_len = 1 + p->length_length + p->type_length;
		p->packet_encode = IOVEC_CHILD;
	case IOVEC_CHILD:
		if(!vlen || vused == vlen)
			break;
		{
			struct list_head *e;
			for(e = p->children.next;
			    prefetch(e->next), e != &p->children && vused < vlen;
			    e = e->next)
			{
				g2_packet_t *child = list_entry(e, g2_packet_t, list);
				ret = g2_packet_serialize_to_iovec(child, &vec[vused], vlen - vused);
				if(-1 == ret)
					return ret;
				vused += (size_t)ret;
			}
		}
		/* stay in this state until we are sure there is no more child data */
		if(vused == vlen)
			break;
		p->packet_encode = IOVEC_ZERO;
	case IOVEC_ZERO:
		if(p->is_compound && p->data_trunk.data && buffer_remaining(p->data_trunk))
		{
			if(!vlen || vused == vlen)
				break;
			vec[vused].iov_base  = (void *)(intptr_t)&priv_zero;
			vec[vused++].iov_len = 1;
		}
		p->packet_encode = IOVEC_DATA;
	case IOVEC_DATA:
		if(p->data_trunk.data && buffer_remaining(p->data_trunk))
		{
			if(!vlen || vused == vlen)
				break;
			vec[vused].iov_base  = buffer_start(p->data_trunk);
			vec[vused++].iov_len = buffer_remaining(p->data_trunk);
		}
		p->packet_encode = IOVEC_CLEANAFTER;
	case IOVEC_CLEANAFTER:
		/* dummy, to clean up after send */
	case ENCODE_FINISHED:
		break;
	default:
		logg_develd("bogus enoder_state: %i\n", p->packet_encode);
		return -1;
	}

	return vused;
}
#endif

static uint8_t create_control_byte(g2_packet_t *p)
{
	uint8_t control = 0;

/*	control |= p->c_reserved  ? 0x01 : 0; */ /* we don't set this bit */
	control |= p->big_endian  ? 0x02 : 0;
	control |= p->is_compound ? 0x04 : 0;
	control |= (p->type_length-1) << 3;
	control |= p->length_length << 6;
	/*
	 * It is possible to create a zero control byte (no length,
	 * 1 char type, no options), and this is not allowed.
	 * Set is_compound in this case, like Shareaza does and is
	 * stated in the spec.
	 * I personaly don't like this, since it creates a special
	 * case: now is_compound is only valid if there is data.
	 * And you always have to look for is_compound because a
	 * packet may grow children.
	 * We could also set big_endian without harm, there is no data
	 * this can affect ;), but i think every WinApp would go belly
	 * up on such a packet...
	 * I don't know why they choose this special meaning for
	 * is_compound, big_endian would have been easier.
	 */
	control = control ? control : 0x04;

	return control;
}

#define SERIALIZE_TYPE_ON_NUM(x, prop) \
			if(prop(source->type_length >= (x))) { \
				if(unlikely(!buffer_remaining(*target))) { \
					source->more_bytes_needed = true; \
					break; \
				} \
				if(likely(PT_UNKNOWN != source->type)) \
					*buffer_start(*target) = g2_ptype_names[source->type][((x)-1)]; \
				else \
					*buffer_start(*target) = source->pd.in.type[((x)-1)]; \
				target->pos++; \
			} \
			source->packet_encode++; \

bool g2_packet_serialize_to_buff(g2_packet_t *source, struct norm_buff *target)
{
	bool ret_val = true, need_zero = false;

	source->more_bytes_needed = false;
	while(likely(!source->more_bytes_needed))
	{
		switch(source->packet_encode)
		{
		case DECIDE_ENCODE:
			source->packet_encode = PREPARE_SERIALIZATION_MIN;
		case PREPARE_SERIALIZATION:
		case PREPARE_SERIALIZATION_MIN:
			{
				ssize_t ret;
				ret = g2_packet_serialize_prep_min(source);
				if(unlikely(-1 == ret))
					return false;
			}
		case SERIALIZATION_PREPARED:
		case SERIALIZATION_PREPARED_MIN:
			source->packet_encode = SERIALIZE_CONTROL;
		case SERIALIZE_CONTROL:
			{
				uint8_t control;
				if(unlikely(!buffer_remaining(*target))) {
					source->more_bytes_needed = true;
					break;
				}
				control = create_control_byte(source);
				*buffer_start(*target) = (char)control;
				target->pos++;
				source->packet_encode++;
			}
		case SERIALIZE_LENGTH1:
			if(likely(source->length_length >= 1))
			{
				if(unlikely(!buffer_remaining(*target))) {
					source->more_bytes_needed = true;
					break;
				}
				if(likely(1 == source->length_length))
					*buffer_start(*target) = (char)(source->length&0xFF);
				else
				{
					if(likely(!source->big_endian))
						*buffer_start(*target) = (char)(source->length&0xFF);
					else {
						if(likely(2 == source->length_length))
							*buffer_start(*target) = (char)((source->length>>8)&0xFF);
						else
							*buffer_start(*target) = (char)((source->length>>16)&0xFF);
					}
				}
				target->pos++;
			}
			source->packet_encode++;
			prefetch(&g2_ptype_names[source->type]);
		case SERIALIZE_LENGTH2:
			if(unlikely(source->length_length >= 2))
			{
				if(unlikely(!buffer_remaining(*target))) {
					source->more_bytes_needed = true;
					break;
				}
				if(likely(!source->big_endian))
					*buffer_start(*target) = (char)((source->length>>8)&0xFF);
				else {
					if(likely(2 == source->length_length))
						*buffer_start(*target) = (char)((source->length)&0xFF);
					else
						*buffer_start(*target) = (char)((source->length>>8)&0xFF);
				}
				target->pos++;
			}
			source->packet_encode++;
		case SERIALIZE_LENGTH3:
			if(unlikely(source->length_length >= 3))
			{
				if(unlikely(!buffer_remaining(*target))) {
					source->more_bytes_needed = true;
					break;
				}
				if(likely(!source->big_endian))
					*buffer_start(*target) = (char)((source->length>>16)&0xFF);
				else
					*buffer_start(*target) = (char)((source->length)&0xFF);
				target->pos++;
			}
			source->packet_encode++;
		case SERIALIZE_TYPE1:
			SERIALIZE_TYPE_ON_NUM(1, likely);
		case SERIALIZE_TYPE2:
			SERIALIZE_TYPE_ON_NUM(2, likely);
		case SERIALIZE_TYPE3:
			SERIALIZE_TYPE_ON_NUM(3, likely);
		case SERIALIZE_TYPE4:
			SERIALIZE_TYPE_ON_NUM(4, unlikely);
		case SERIALIZE_TYPE5:
			SERIALIZE_TYPE_ON_NUM(5, unlikely);
		case SERIALIZE_TYPE6:
			SERIALIZE_TYPE_ON_NUM(6, unlikely);
		case SERIALIZE_TYPE7:
			SERIALIZE_TYPE_ON_NUM(7, unlikely);
		case SERIALIZE_TYPE8:
			SERIALIZE_TYPE_ON_NUM(8, unlikely);
			/* How to continue ? */
			if(list_empty(&source->children)) {
				source->packet_encode = SERIALIZE_DATA_PREP;
				break;
			} else
				source->packet_encode = SERIALIZE_CHILDREN;
		case SERIALIZE_CHILDREN:
			{
				struct list_head *e, *n;
				/* loop over children... */
				list_for_each_safe(e, n, &source->children)
				{
					g2_packet_t *child = list_entry(e, g2_packet_t, list);
					if(likely(ENCODE_FINISHED != child->packet_encode))
						ret_val = g2_packet_serialize_to_buff(child, target);
					if(unlikely(ENCODE_FINISHED != child->packet_encode)) {
						source->more_bytes_needed = child->more_bytes_needed;
						break;
					}
					list_del(e);
					g2_packet_free(child);
				}
				source->packet_encode = BEWARE_OF_ZERO;
			}
		case BEWARE_OF_ZERO:
			/* insert zero byte between child and data, only if there is data... */
			need_zero = true;
		case SERIALIZE_DATA_PREP:
			if(source->data_trunk.data && buffer_remaining(source->data_trunk))
			{
				if(need_zero)
				{
					if(unlikely(!buffer_remaining(*target))) {
						source->more_bytes_needed = true;
						break;
					}
					*buffer_start(*target) = '\0';
					target->pos++;
				}
				source->packet_encode = SERIALIZE_DATA;
			}
			else {
				source->more_bytes_needed = true;
				source->packet_encode = ENCODE_FINISHED;
				break;
			}
		case SERIALIZE_DATA:
			{
				size_t len = buffer_remaining(source->data_trunk);
				len = len < buffer_remaining(*target) ? len : buffer_remaining(*target);
				memcpy(buffer_start(*target), buffer_start(source->data_trunk), len);
				target->pos += len;
				source->data_trunk.pos += len;
			}
			if(!buffer_remaining(source->data_trunk))
				source->packet_encode = ENCODE_FINISHED;
		case ENCODE_FINISHED:
				/* Yehaa, it's done! */
			source->more_bytes_needed = true;
			break;
		case IOVEC_HEADER:
		case IOVEC_CHILD:
		case IOVEC_ZERO:
		case IOVEC_DATA:
		case IOVEC_CLEANAFTER:
		case MAX_ENCODER_STATE:
			logg_develd("wrong packet in enoder: %i %s\n", source->packet_encode,
				g2_packet_encoder_states_txt[source->packet_encode]);
			return false;
#ifndef PS_CHECK_STATES
		default:
			logg_develd("bogus encoder_state: %i\n", source->packet_encode);
			return false;
#endif
		}
	}

	return ret_val;
}
#undef SERIALIZE_TYPE_ON_NUM

/*
 * Implementaion of g2_packet_serialize_prep{min}
 *
 */
static ssize_t g2_packet_serialize_prep_internal(g2_packet_t *p, size_t recurs, bool write_header)
{
	size_t size = 0, child_size = 0, i, j;
	uint8_t control = 0;

	if(recurs > 20) {
		logg_develd("ALARM: %p \n", p);
		return -1;
	}

	if(!p->is_literal)
	{
		/* calculate the inner packet length */
		if(likely(!list_empty(&p->children))) {
			struct list_head *e;
			list_for_each(e, &p->children) {
				g2_packet_t *child = list_entry(e, g2_packet_t, list);
				ssize_t ret = g2_packet_serialize_prep_internal(child, recurs + 1, write_header);
				if(-1 == ret) {
					if(recurs  == 0) {
						logg_develd("Packet \"%s\" is brocken\n",
							g2_ptype_names[p->type]);
					}
					return ret;
				}
				child_size += (size_t)ret;
			}
			p->is_compound = true;
			size += child_size;
		} else
			p->is_compound = false;
	}

	if(likely(p->data_trunk.data && buffer_remaining(p->data_trunk))) {
		size += size ? 1 : 0; /* child terminator */
		size += buffer_remaining(p->data_trunk);
	}

	/* check inner length */
	if(!size)
		p->length_length = 0;
	else if(size <= 0xFF)
		p->length_length = 1;
	else if(unlikely(size <= 0xFFFF))
		p->length_length = 2;
	else if(unlikely(size <= 0xFFFFFF))
		p->length_length = 3;
	else {
		size_t difference;
		/* normaly we should NEVER reach this! */
		logg_develd("Packet with very big size! Trying scary fixup for \"%s\": %zu\n",
			g2_ptype_names[p->type], size);
		difference = size - child_size;
		if(0 == difference || difference >= buffer_remaining(p->data_trunk))
			return -1;
		p->data_trunk.limit -= difference;
		size = 0xFFFFFF;
		p->length_length = 3;
	}

	p->length = size;
	if(likely(PT_UNKNOWN != p->type))
		p->type_length = g2_ptype_names_length[p->type];
	/*
	 * don't touch endiannes, inner packet data mandates
	 * endiannes. We can adapt how we write the length
	 * But beware: endiannes has to be set up!
	 */
	/* p->big_endian  = host_endian ; */

	if(!write_header) {
		p->packet_encode = SERIALIZATION_PREPARED_MIN;
		return size + p->length_length + p->type_length + 1;
	}

// TODO: this is broken ATM, and uneeded
	control = create_control_byte(p);

	/* check if headerspace is free */
	if(p->data_trunk.data == p->pd.out)
	{
		size_t size_needed = p->length_length + p->type_length + 1;
		/* OhOh, we have a Problem, data to send may stored in header space */
		if((sizeof(p->pd.out) - size_needed) < buffer_remaining(p->data_trunk)) {
			char *tmp_ptr;
			/* totaly fucked up, header and data to big */
			logg_develd("Packet with stuffed data! Expensive Fixup for \"%s\": %zu\n",
				g2_ptype_names[p->type], buffer_remaining(p->data_trunk));
			if(!(tmp_ptr = malloc(sizeof(p->pd.out))))
				return -1;
			memcpy(tmp_ptr, buffer_start(p->data_trunk), buffer_remaining(p->data_trunk));
			p->data_trunk.data = tmp_ptr;
			p->data_trunk_is_freeable = true;
		}
		else if(size_needed > p->data_trunk.pos)
		{
			size_t dlength = buffer_remaining(p->data_trunk);
			/* move data up */
			memmove(p->pd.out + size_needed, buffer_start(p->data_trunk), dlength);
			p->data_trunk.pos  = 0;
			p->data_trunk.data = p->pd.out + size_needed;
			p->data_trunk.limit = dlength;
			p->data_trunk.capacity = sizeof(p->pd.out) - size_needed;
			p->data_trunk_is_freeable = false;
		}
		/* else we are lucky bastards */
	}

	i = 0;
	p->pd.out[i++] = control;
	/* write the inner packet length */
	if(1 == p->length_length)
		p->pd.out[i++] = (char)size;
	else if(!p->big_endian) {
		uint32_t nsize = size;
		for(j = 0; j < p->length_length; j++, nsize >>=8)
			p->pd.out[i++] = (char)(nsize & 0xFF);
	} else {
		for(j = p->length_length; j--;)
			p->pd.out[i++] = (char)(size >> (j*8)) & 0xFF;
	}
	for(j = 0; j < p->type_length; i++, j++)
		p->pd.out[i] = g2_ptype_names[p->type][j];

	/*
	 * return REAL packet length !!
	 */
	p->packet_encode = SERIALIZATION_PREPARED;
	return size + p->length_length + p->type_length + 1;
}

/*
 * g2_packet_serialize_prep_min - minimal prepare packet for serialization
 *
 * p - the packet to prepare
 *
 * our job is to go over the packet (and it childs) and:
 * - gather the lengths
 * - set the fields length_length, compound
 *
 * We do NOT write out every header(see down)
 *
 * Warning: Your packet may be is_literal, then we do nothing!
 *
 * return value: REAL packet length !! with header
 *               -1 on error
 */
ssize_t g2_packet_serialize_prep_min(g2_packet_t *p)
{
	return g2_packet_serialize_prep_internal(p, 0, false);
}

/*
 * g2_packet_serialize_prep - prepare packet for serialization
 *
 * p - the packet to prepare
 *
 * our job is to go over the packet (and it childs) and:
 * - gather the lengths
 * - set the fields length_length, compound
 * - write out every header
 *
 * Warning: Your packet may be is_literal, then we do nothing!
 *
 * return value: REAL packet length !! with header
 *               -1 on error
 */
ssize_t g2_packet_serialize_prep(g2_packet_t *p)
{
	return g2_packet_serialize_prep_internal(p, 0, true);
}

static char const rcsid_ps[] GCC_ATTR_USED_VAR = "$Idi$";
//EOF
