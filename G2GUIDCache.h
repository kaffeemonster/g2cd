/*
 * G2GUIDCache.h
 * header for known GUID cache foo
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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

#ifndef _G2GUIDCACHE_H
# define _G2GUIDCACHE_H

# include <stdbool.h>
# include <time.h>
# include "lib/combo_addr.h"
# include "lib/guid.h"

# ifndef _G2GUIDCACHE_C
#  define _G2GUIDC_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define _G2GUIDC_EXTRN(x) x GCC_ATTR_VIS("hidden")
# endif

/*
 * this enum is sorted by "importance" of
 * the guid, lower is more important.
 * Not so important guids will get kicked
 * earlier when the space is needed.
 */
enum guid_type
{
	GT_NEIGHBOUR,
	GT_LEAF,
	GT_KHL_NEIGHBOUR,
	GT_KHL,
	GT_PEER,
	GT_QUERY,
	GT_HAW,
	GT_UNKNOWN,
} GCC_ATTR_PACKED;

/* increment version on change */
struct guid_entry
{
	union combo_addr na;
	time_t when;
	uint8_t guid[GUID_SIZE];
	enum guid_type type;
};

_G2GUIDC_EXTRN(bool g2_guid_lookup(const uint8_t [GUID_SIZE], enum guid_type, union combo_addr *));
_G2GUIDC_EXTRN(bool g2_guid_add(const uint8_t [GUID_SIZE], const union combo_addr *, time_t, enum guid_type));
_G2GUIDC_EXTRN(bool g2_guid_init(void));
_G2GUIDC_EXTRN(void g2_guid_end(void));

#endif
/* EOF */
