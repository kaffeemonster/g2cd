/*
 * G2GUIDCache.h
 * header for known GUID cache foo
 *
 * Copyright (c) 2009 Jan Seiffert
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

#ifndef _G2GUIDCACHE_H
# define _G2GUIDCACHE_H

# include <stdbool.h>
# include <time.h>
# include "lib/combo_addr.h"

# ifndef _G2GUIDCACHE_C
#  define _G2GUIDC_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define _G2GUIDC_EXTRN(x) x GCC_ATTR_VIS("hidden")
# endif

# define GUID_SIZE 16

/* increment version on change */
struct guid_entry
{
	union combo_addr na;
	time_t when;
	uint8_t guid[GUID_SIZE];
};

_G2GUIDC_EXTRN(void g2_guid_add(const uint8_t [GUID_SIZE], const union combo_addr *, time_t));
_G2GUIDC_EXTRN(bool g2_guid_init(void));
_G2GUIDC_EXTRN(void g2_guid_end(void));

#endif
/* EOF */
