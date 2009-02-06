/*
 * G2QHT.h
 * Header for the G2 QHT
 *
 * Copyright (c) 2006-2008 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * g2cd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
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

#ifndef G2QHT_H
# define G2QHT_H

/* Includes if included */
# include <stdbool.h>
# include <sys/types.h>
# include <time.h>
/* Own */
# include "lib/hzp.h"
# include "lib/atomic.h"

enum g2_qht_comp
{
	COMP_NONE    = 0,
	COMP_DEFLATE = 1,
	COMP_RLE     = 2,
};

struct qht_fragment
{
	struct qht_fragment *next;
	size_t   length;
	size_t   nr;
	size_t   count;
	enum g2_qht_comp compressed;
	uint8_t  data[DYN_ARRAY_LEN];
};

# define MAX_BYTES_QHT (512 * 1024)
# define QHT_PATCH_HEADER_BYTES 5
# define QHT_RESET_HEADER_BYTES 6
struct qhtable
{
	struct hzp_free hzp;
	atomic_t refcnt;
	size_t   data_length;
/* ----- Everything below this gets simply wiped ----- */
	size_t   entries;
	size_t   bits;
	time_t   time_stamp;
	struct qht_fragment *fragments;
	enum g2_qht_comp     compressed;
	struct qht_flags
	{
		bool  reset_needed;
	} flags;
/* ----- Everthing above this gets simply wiped ------ */
	uint8_t  *data;
};

# ifndef _G2QHT_C
#  define _G2QHT_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#  define _G2QHT_EXTRNVAR(x) extern x
# else
#  define _G2QHT_EXTRN(x) x GCC_ATTR_VIS("hidden")
#  define _G2QHT_EXTRNVAR(x)
# endif /* _G2QHT_C */

_G2QHT_EXTRN(void g2_qht_clean(struct qhtable *));
_G2QHT_EXTRN(void g2_qht_put(struct qhtable *));
_G2QHT_EXTRN(const char *g2_qht_patch(struct qhtable *, struct qht_fragment *));
_G2QHT_EXTRN(void g2_qht_aggregate(struct qhtable *, struct qhtable *));
_G2QHT_EXTRN(int g2_qht_add_frag(struct qhtable *, struct qht_fragment *, uint8_t *data));
_G2QHT_EXTRN(bool g2_qht_reset(struct qhtable **, uint32_t qht_ent, bool try_compress));
_G2QHT_EXTRN(struct qht_fragment *g2_qht_diff_get_frag(const struct qhtable *, const struct qhtable *));
_G2QHT_EXTRN(struct qht_fragment *g2_qht_frag_alloc(size_t len));
_G2QHT_EXTRN(void g2_qht_frag_free(struct qht_fragment *));
/* funcs for the global qht are in the G2ConRegistry */
_G2QHT_EXTRN(size_t g2_qht_global_get_ent(void));
_G2QHT_EXTRN(struct qhtable *g2_qht_global_get(void));
_G2QHT_EXTRN(void g2_qht_global_update(void));

#endif /* G2QHT_H */
/* EOF */
