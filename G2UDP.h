/*
 * G2UDP.h
 * header-file for G2UDP.c
 *
 * Copyright (c) 2004, Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2
 * of the License, or any later version.
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
 * $Id: G2UDP.h,v 1.8 2005/11/05 10:40:01 redbully Exp redbully $
 */

#ifndef _G2UDP_H
#define _G2UDP_H

#include "other.h"
#include <stdbool.h>

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


#ifndef _G2UDP_C
#define _G2UDP_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#else
#define _G2UDP_EXTRN(x) x GCC_ATTR_VIS("hidden")

#endif // _G2UDP_C
_G2UDP_EXTRN(void *G2UDP(void *));

#endif //_G2UDP_H
//EOF
