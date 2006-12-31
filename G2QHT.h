/*
 * G2QHT.h
 * Header for the G2 QHT
 *
 * Copyright (c) 2006, Jan Seiffert
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
 * $Id:$
 */

#ifndef _G2QHT_H
#define _G2QHT_H

// Includes if included
#include <stdbool.h>
#include <sys/types.h>
// Own
#include "version.h"
// #include "lib/hzp.h"

enum g2_qht_comp
{
	COMP_NONE = 0,
	COMP_DEFLATE = 1
};

struct qht_fragment
{
	size_t	length;
	size_t	nr;
	size_t	count;
	enum g2_qht_comp	compressed;
	uint8_t	*data;
};

#define MAX_BYTES_QHT (512 * 1024)
struct qhtable
{
	size_t	entries;
	size_t	bits;
	size_t	data_length;
	enum g2_qht_comp	compressed;
	struct qht_fragment	fragments;
	struct qht_flags
	{
		bool reset_needed;
	} flags;
/* ----- Everthing above this gets simply wiped ------ */
	uint8_t	data[DYN_ARRAY_LEN];
};

#ifndef _G2QHT_C
#define _G2QHT_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#define _G2QHT_EXTRNVAR(x) extern x
#else
#define _G2QHT_EXTRN(x) x GCC_ATTR_VIS("hidden")
#define _G2QHT_EXTRNVAR(x)
#endif // _G2QHT_C

_G2QHT_EXTRN(inline void g2_qht_clean(struct qhtable *));
_G2QHT_EXTRN(inline void g2_qht_free(struct qhtable *));
_G2QHT_EXTRN(inline void g2_qht_frag_clean(struct qht_fragment *));
_G2QHT_EXTRN(inline const char *g2_qht_patch(struct qhtable **, struct qht_fragment *));
_G2QHT_EXTRN(inline int g2_qht_add_frag(struct qhtable *, struct qht_fragment *));
_G2QHT_EXTRN(inline bool g2_qht_reset(struct qhtable **, uint32_t qht_ent));

#endif // _G2QHT_H
//EOF
