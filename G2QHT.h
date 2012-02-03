/*
 * G2QHT.h
 * Header for the G2 QHT
 *
 * Copyright (c) 2006-2012 Jan Seiffert
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

#ifndef G2QHT_H
# define G2QHT_H

/* Includes if included */
# include <stdbool.h>
# include <sys/types.h>
# include <time.h>
/* Own */
# include "lib/hzp.h"
# include "lib/atomic.h"
# include "lib/tchar.h"

# ifdef WANT_QHT_ZPAD
#  include <zlib.h>
#  include "lib/palloc.h"
struct zpad
{
	z_stream z;
	struct d_heap d;
};
# else
struct zpad;
#endif

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
# define QHT_DEFAULT_BITS 20
# define QHT_DEFAULT_BYTES ((1 << QHT_DEFAULT_BITS)/8)
struct qhtable
{
	struct hzp_free hzp;
	atomic_t refcnt;
	size_t   data_length;
/* ----- Everything below this gets simply wiped ----- */
	size_t   entries;
	size_t   used;
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

struct qht_search_walk
{
	uint32_t *hashes;
	size_t num;
	void *data;
};

# ifndef _G2QHT_C
#  define _G2QHT_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#  define _G2QHT_EXTRNVAR(x) extern x
# else
#  define _G2QHT_EXTRN(x) x GCC_ATTR_VIS("hidden")
#  define _G2QHT_EXTRNVAR(x)
# endif /* _G2QHT_C */

_G2QHT_EXTRN(struct zpad *qht_get_zpad(void));

_G2QHT_EXTRN(void g2_qht_clean(struct qhtable *));
_G2QHT_EXTRN(void g2_qht_put(struct qhtable *));
_G2QHT_EXTRN(bool g2_qht_search_prepare(void));
_G2QHT_EXTRN(void g2_qht_search_add_word(const tchar_t *s, size_t start, size_t len));
_G2QHT_EXTRN(void g2_qht_search_add_hash(uint32_t h));
_G2QHT_EXTRN(void g2_qht_search_add_sha1(const unsigned char *h));
_G2QHT_EXTRN(void g2_qht_search_add_ttr(const unsigned char *h));
_G2QHT_EXTRN(void g2_qht_search_add_ed2k(const unsigned char *h));
_G2QHT_EXTRN(void g2_qht_search_add_bth(const unsigned char *h));
_G2QHT_EXTRN(void g2_qht_search_add_md5(const unsigned char *h));
_G2QHT_EXTRN(bool g2_qht_search_add_hash_urn(const tchar_t *str, size_t len));
_G2QHT_EXTRN(bool g2_qht_search_drive(char *metadata, size_t metadata_len, char *dn, size_t dn_len, void *data, bool had_urn, bool hubs));
_G2QHT_EXTRN(void g2_qht_match_hubs(uint32_t hashes[], size_t num, void *data));
_G2QHT_EXTRN(void g2_qht_match_leafs(uint32_t hashes[], size_t num, void *data));
_G2QHT_EXTRN(bool g2_qht_global_search_bucket(struct qht_search_walk *, struct qhtable *));
_G2QHT_EXTRN(void g2_qht_global_search_chain(struct qht_search_walk *, void *)); /* last parm is a g2_connec, but to not pull the header */
_G2QHT_EXTRN(const char *g2_qht_patch(struct qhtable *, struct qht_fragment *));
_G2QHT_EXTRN(void g2_qht_aggregate(struct qhtable *, struct qhtable *));
_G2QHT_EXTRN(int g2_qht_add_frag(struct qhtable *, struct qht_fragment *, uint8_t *data));
_G2QHT_EXTRN(bool g2_qht_reset(struct qhtable **, uint32_t qht_ent, bool try_compress));
_G2QHT_EXTRN(struct qht_fragment *g2_qht_diff_get_frag(const struct qhtable *, const struct qhtable *) GCC_ATTR_MALLOC);
_G2QHT_EXTRN(struct qht_fragment *g2_qht_frag_alloc(size_t len) GCC_ATTR_MALLOC);
_G2QHT_EXTRN(void g2_qht_frag_free(struct qht_fragment *));
/* funcs for the global qht are in the G2ConRegistry */
_G2QHT_EXTRN(size_t g2_qht_global_get_ent(void));
_G2QHT_EXTRN(struct qhtable *g2_qht_global_get(void));
_G2QHT_EXTRN(struct qhtable *g2_qht_global_get_hzp(void));
_G2QHT_EXTRN(void g2_qht_global_search(struct qht_search_walk *));
_G2QHT_EXTRN(void g2_qht_global_update(void));

#endif /* G2QHT_H */
/* EOF */
