/*
 * sec_buffer.h
 * home of struct norm_buff, big_buff, pointer_buff and some
 * helper-macros, which hopefully make working safe with this
 * buffers
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
 * $Id: sec_buffer.h,v 1.9 2005/11/05 10:48:17 redbully Exp redbully $
 */

#ifndef _SECBUFFER_H
#define _SECBUFFER_H

#include <string.h>
#include "../other.h"
#include "../builtin_defaults.h"

/* ! ! WARNING ! !
 * struct norm_buff and struct big_buff are cast-compatible, as
 * long as they are cleanly handled with the informations (pos,
 * limit, capacity) obtained along with the buffer (the define
 * of NORM_BUFF_CAPACITY is *not* for the programmer, its for
 * maintinace reasons), not what is assumed about them.
 *
 * struct pointer_buff is ***NOT*** cast-compatible with the
 * other buffer structs!!
 *
 * Never ever cast a struct pointer_buff to any other
 * buff-struct and vice-versa.
 *
 * For those not understanding this, please refer to a good
 * C-reference and doublecheck you fully understand the nature
 * of pointers and arrays in C (only good references describe
 * it in _full_ detail, so you may have problems with
 * "C in X days") and learn about the subtile little
 * differences between them.
 * 
 * Shortly spoken: They are NOT the same thing, they only
 * _behave_ mostly the same, but not allways.
 * This is such a case.
 */

struct norm_buff
{
	size_t pos;
	size_t limit;
	size_t capacity;
	char data[NORM_BUFF_CAPACITY];
};

struct big_buff
{
	size_t pos;
	size_t limit;
	size_t capacity;
	char data[DYN_ARRAY_LEN];
};

struct pointer_buff
{
	size_t pos;
	size_t limit;
	size_t capacity;
	char *data;
};

#ifndef ASSERT_BUFFERS
# define buffer_start(x) ((x).data + (x).pos)
# define buffer_remaining(x) ((x).limit - (x).pos)
#else
# define buffer_start(x) ( \
	(unlikely((x).pos > (x).capacity)) ? \
	logg_posd(LOGF_EMERG, \
		"buffer pos of by %lu", \
		(long unsigned) (x).pos - (x).capacity), \
	(x).data + (x).pos : \
	(x).data + (x).pos)
# define buffer_remaining(x) ( \
	(unlikely((x).pos > (x).limit)) ? \
	logg_posd(LOGF_EMERG, \
		"buffer wrap around by %lu", \
		(long unsigned) (x).pos - (x).limit), \
	(x).limit - (x).pos : \
	(x).limit - (x).pos)
#endif

#define buffer_clear(x) \
	do { \
		(x).pos = 0; \
		(x).limit = (x).capacity; \
	} while(0)

#define buffer_flip(x) \
	do { \
		(x).limit = (x).pos; \
		(x).pos = 0; \
	} while(0)

#define buffer_unflip(x) \
	do { \
		(x).pos = buffer_remaining(x); \
		(x).limit = (x).capacity; \
	} while(0)

#define buffer_compact(x) \
	do { \
		if(buffer_remaining(x)) \
			if((x).pos) memmove((x).data, \
				buffer_start(x), \
				buffer_remaining(x)); \
		buffer_unflip(x); \
	} while(0)

#define buffer_headroom(x) \
	((x).capacity - (x).limit)

#define buffer_possible_remaining(x) \
	((x).capacity - (x).pos)

/*
 * only valid after a compact
 */
#define buffer_cempty(x) \
	!(x).pos

static inline void INIT_PBUF(struct pointer_buff *p)
{
	p->pos = p->limit = p->capacity = 0;
	p->data = NULL;
}

#endif // _SECBUFFER_H
//EOF
