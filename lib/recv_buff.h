/*
 * recv_buff.h
 * Header for the recvieve buffer allocator.
 *
 * Copyright (c) 2007-2010 Jan Seiffert
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
 * $Id: $
 */

#ifndef LIB_RECV_BUFF_H
#define LIB_RECV_BUFF_H

#include "other.h"
#include "sec_buffer.h"

# define LIB_RECV_BUFF_EXTRN(x) x GCC_ATTR_VIS("hidden")

LIB_RECV_BUFF_EXTRN(struct norm_buff *recv_buff_local_get(void) GCC_ATTR_MALLOC);
LIB_RECV_BUFF_EXTRN(void recv_buff_local_ret(struct norm_buff *));
LIB_RECV_BUFF_EXTRN(void recv_buff_local_refill(void));
LIB_RECV_BUFF_EXTRN(void recv_buff_local_free(void));
LIB_RECV_BUFF_EXTRN(struct norm_buff *recv_buff_alloc(void) GCC_ATTR_MALLOC);
LIB_RECV_BUFF_EXTRN(void recv_buff_free(struct norm_buff *) GCC_ATTR_FASTCALL );

#endif /* LIB_RECV_BUFF_H */
