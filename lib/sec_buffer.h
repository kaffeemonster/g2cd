/*
 * sec_buffer.h
 * home of struct norm_buff, big_buff, pointer_buff and some
 * helper-macros, which hopefully make working safe with this
 * buffers
 *
 * Copyright (c) 2004-2010 Jan Seiffert
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
 * $Id: sec_buffer.h,v 1.9 2005/11/05 10:48:17 redbully Exp redbully $
 */

#ifndef LIB_SECBUFFER_H
# define LIB_SECBUFFER_H

# include <string.h>
# include "other.h"
# include "../builtin_defaults.h"

/*       - - - * * * ! ! WARNING ! ! * * * - - -
 * struct norm_buff and struct big_buff are cast-compatible, as
 * long as they are cleanly handled with the informations (pos,
 * limit, capacity) obtained along with the buffer (the define
 * of NORM_BUFF_CAPACITY is *not* for the programmer, its for
 * maintinance reasons), not what is assumed about them.
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
 * "C in X days", and you better take some time till you really
 * "groked it down") and learn about the subtile little
 * differences between them.
 *
 * Shortly spoken: They are NOT the same thing, they only
 * _behave_ mostly the same, but not in every context.
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

# ifndef ASSERT_BUFFERS
#  define buffer_start(x) ((x).data + (x).pos)
#  define buffer_remaining(x) ((x).limit - (x).pos)
# else
#  define buffer_start(x) ( \
	(unlikely((x).pos > (x).capacity)) ? \
	logg_posd(LOGF_EMERG, \
		"buffer pos of by %ld\n", \
		(long) (x).pos - (x).capacity), \
	(x).data + (x).pos : \
	(x).data + (x).pos)
#  define buffer_remaining(x) ( \
	(unlikely((x).pos > (x).limit)) ? \
	logg_posd(LOGF_EMERG, \
		"buffer wrap around by %ld\n", \
		(long) (x).pos - (x).limit), \
	(x).limit - (x).pos : \
	(x).limit - (x).pos)
# endif

# define buffer_clear(x) \
	do { \
		(x).pos = 0; \
		(x).limit = (x).capacity; \
	} while(0)

# define buffer_flip(x) \
	do { \
		(x).limit = (x).pos; \
		(x).pos = 0; \
	} while(0)

# define buffer_unflip(x) \
	do { \
		(x).pos = buffer_remaining(x); \
		(x).limit = (x).capacity; \
	} while(0)

# define buffer_compact(x) \
	do { \
		if(buffer_remaining(x)) \
			if((x).pos) my_memmove((x).data, \
				buffer_start(x), \
				buffer_remaining(x)); \
		buffer_unflip(x); \
	} while(0)

# define buffer_skip(x) \
	do { \
		(x).pos = (x).limit; \
	} while(0)

# define buffer_headroom(x) \
	((x).capacity - (x).limit)

# define buffer_possible_remaining(x) \
	((x).capacity - (x).pos)

/*
 * only valid after a compact
 */
# define buffer_cempty(x) \
	!(x).pos

static inline void INIT_PBUF(struct pointer_buff *p)
{
	p->pos = p->limit = p->capacity = 0;
	p->data = NULL;
}

#endif /* LIB_SECBUFFER_H */
/* EOF */
