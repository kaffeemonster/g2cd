/*
 * G2Handler.h
 * header-file for G2Handler.c
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
 * $Id: G2Handler.h,v 1.5 2004/12/18 18:06:13 redbully Exp redbully $
 */

#ifndef G2HANDLER_H
# define G2HANDLER_H

# include "lib/other.h"
# include "lib/my_epoll.h"
# include "lib/sec_buffer.h"
# include "gup.h"

# define HANDLER_ACTIVE_TIMEOUT (91 * 10)

# ifndef _G2HANDLER_C
#  define _G2HAN_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define _G2HAN_EXTRN(x) x GCC_ATTR_VIS("hidden")
# endif /* _G2HANDLER_C */

_G2HAN_EXTRN(void handle_con(struct epoll_event *, struct norm_buff *[2], int));

#endif /* G2HANDLER_H */
/* EOF */
