/*
 * G2KHL.h
 * header for known hublist foo
 *
 * Copyright (c) 2008-2010 Jan Seiffert
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
 * $Id:$
 */

#ifndef _G2KHL_H
# define _G2KHL_H

# include <stdbool.h>
# include <time.h>
# include "G2Packet.h"
# include "lib/combo_addr.h"

# ifndef _G2KHL_C
#  define _G2KHL_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define _G2KHL_EXTRN(x) x GCC_ATTR_VIS("hidden")
# endif

#define G2KHL_SOCK_COM_HANDLER ((void (*)(struct sock_com *, short))1)

/* increment version on change */
struct khl_entry
{
	union combo_addr na;
	time_t when;
};

_G2KHL_EXTRN(void g2_khl_add(const union combo_addr *, time_t, bool));
_G2KHL_EXTRN(size_t g2_khl_fill_p(struct khl_entry [], size_t, int));
_G2KHL_EXTRN(size_t g2_khl_fill_s(struct khl_entry [], size_t, int));
_G2KHL_EXTRN(const char *g2_khl_get_url(void));
_G2KHL_EXTRN(bool g2_khl_init(void));
_G2KHL_EXTRN(bool g2_khl_tick(void));
_G2KHL_EXTRN(void g2_khl_end(void));

#endif
/* EOF */
