/*
 * guid.h
 * little stuff to generate a guid
 *
 * Copyright (c) 2010 Jan Seiffert
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

#ifndef LIB_GUID_H
# define LIB_GUID_H

# define GUID_SIZE 16

# include "other.h"

# define LIB_GUID_EXTRN(x) x GCC_ATTR_VIS("hidden")

LIB_GUID_EXTRN(void guid_generate(unsigned char out[GUID_SIZE]));
LIB_GUID_EXTRN(void guid_tick(void));
LIB_GUID_EXTRN(void guid_init(void));
#endif
