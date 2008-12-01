/*
 * G2KHL.h
 * header for known hublist foo
 *
 * Copyright (c) 2008 Jan Seiffert
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
 * $Id:$
 */

#ifndef _G2KHL_H
# define _G2KHL_H

# include <stdbool.h>
# include <time.h>
# include "lib/combo_addr.h"

# ifndef _G2KHL_C
#  define _G2KHL_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define _G2KHL_EXTRN(x) x GCC_ATTR_VIS("hidden")
# endif

_G2KHL_EXTRN(void g2_khl_add(const union combo_addr *, time_t, bool));
_G2KHL_EXTRN(bool g2_khl_init(void));
_G2KHL_EXTRN(bool g2_khl_tick(int *));
_G2KHL_EXTRN(void g2_khl_end(void));

#endif
/* EOF */
