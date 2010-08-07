/*
 * G2UDP.h
 * header-file for G2UDP.c
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
 * $Id: G2UDP.h,v 1.8 2005/11/05 10:40:01 redbully Exp redbully $
 */

#ifndef G2UDP_H
# define G2UDP_H

# include "lib/other.h"
# include <stdbool.h>
# include "lib/combo_addr.h"
# include "lib/my_epoll.h"
# include "lib/sec_buffer.h"

typedef struct
{
	uint16_t	sequence;
	uint8_t	part;
	uint8_t	count;
	struct
	{
		bool deflate;
		bool ack_me;
	} flags;
} gnd_packet_t;


# ifndef _G2UDP_C
#  define _G2UDP_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define _G2UDP_EXTRN(x) x GCC_ATTR_VIS("hidden")
# endif /* _G2UDP_C */

_G2UDP_EXTRN(bool init_udp(some_fd));
_G2UDP_EXTRN(void clean_up_udp(void));
_G2UDP_EXTRN(void handle_udp(struct epoll_event *, struct norm_buff *[MULTI_RECV_NUM], some_fd));
_G2UDP_EXTRN(void g2_udp_send(const union combo_addr *, const union combo_addr *, struct list_head *));
_G2UDP_EXTRN(void g2_udp_reas_timeout(void));

#endif /* G2UDP_H */
/* EOF */
