/*
 * G2ConHelper.h
 * header-file for G2ConHelper.c
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
 * $Id: G2ConHelper.h,v 1.5 2004/12/18 18:06:13 redbully Exp redbully $
 */

#ifndef _G2CONHELPER_H
#define _G2CONHELPER_H

#include "G2Connection.h"
#include "lib/my_epoll.h"

#ifndef _G2CONHELPERL_C
#define _G2CONHELPER_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#else
#define _G2CONHELPER_EXTRN(x) x GCC_ATTR_VIS("hidden")
#endif /* _G2CONHELPER_C */

_G2CONHELPER_EXTRN(inline g2_connection_t **handle_socket_abnorm(struct epoll_event *));
_G2CONHELPER_EXTRN(inline bool do_read(struct epoll_event *));
_G2CONHELPER_EXTRN(inline bool do_write(struct epoll_event *, int));
_G2CONHELPER_EXTRN(inline bool
recycle_con(
	struct epoll_event *,
	g2_connection_t **,
	struct g2_con_info *,
	int,
	int,
	int,
	void (*)(struct epoll_event *, struct g2_con_info *, int, int)
));

#endif /* _G2CONHELPER_H */
// EOF
