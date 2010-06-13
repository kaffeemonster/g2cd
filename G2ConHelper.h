/*
 * G2ConHelper.h
 * header-file for G2ConHelper.c
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
 * $Id: G2ConHelper.h,v 1.5 2004/12/18 18:06:13 redbully Exp redbully $
 */

#ifndef _G2CONHELPER_H
# define _G2CONHELPER_H

# include "G2Connection.h"
# include "lib/my_epoll.h"
# include "lib/sec_buffer.h"

# ifndef _G2CONHELPERL_C
#  define _G2CONHELPER_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define _G2CONHELPER_EXTRN(x) inline x GCC_ATTR_VIS("hidden")
# endif /* _G2CONHELPER_C */

_G2CONHELPER_EXTRN(g2_connection_t *handle_socket_abnorm(struct epoll_event *));
_G2CONHELPER_EXTRN(bool do_read(struct epoll_event *));
# if 0
_G2CONHELPER_EXTRN(ssize_t do_writev(struct epoll_event *, int, const struct iovec [], size_t, bool));
# endif
_G2CONHELPER_EXTRN(bool do_write(struct epoll_event *, int));
_G2CONHELPER_EXTRN(bool recycle_con(g2_connection_t *, int, int));
_G2CONHELPER_EXTRN(void teardown_con(g2_connection_t *, int));

_G2CONHELPER_EXTRN(bool manage_buffer_before(struct norm_buff **con_buff, struct norm_buff **our_buff));
_G2CONHELPER_EXTRN(void manage_buffer_after(struct norm_buff **con_buff, struct norm_buff **our_buff));

#endif /* _G2CONHELPER_H */
// EOF
