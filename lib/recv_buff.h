/*
 * recv_buff.h
 * Header for the recvieve buffer allocator.
 * 
 * Copyright (c) 2007 Jan Seiffert
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
 * $Id: $
 */

#ifndef LIB_RECV_BUFF_H
#define LIB_RECV_BUFF_H

#include "../other.h"
#include "sec_buffer.h"

# define LIB_RECV_BUFF_EXTRN(x) x GCC_ATTR_VIS("hidden")

LIB_RECV_BUFF_EXTRN(struct norm_buff *recv_buff_local_get(void));
LIB_RECV_BUFF_EXTRN(void recv_buff_local_ret(struct norm_buff *));
LIB_RECV_BUFF_EXTRN(void recv_buff_local_refill(void));
LIB_RECV_BUFF_EXTRN(struct norm_buff *recv_buff_alloc(void));
LIB_RECV_BUFF_EXTRN(void recv_buff_free(struct norm_buff *) GCC_ATTR_FASTCALL );

#endif /* LIB_RECV_BUFF_H */
