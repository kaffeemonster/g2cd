/*
 * str_spn_space.c
 * count white space span length, Tile implementation
 *
 * Copyright (c) 2012 Jan Seiffert
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

static char const rcsid_strspnsti[] GCC_ATTR_USED_VAR = "$Id: $";
#include "tile.h"
/*
 * tile has some nice instructions, everythings good. Could loose
 * one or two instructions which come from the compat layer with
 * it's impl.
 */
#define STR_SPN_NO_PRETEST
#include "../generic/str_spn_space.c"
/* EOF */
