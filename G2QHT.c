/*
 * G2QHT.c
 * helper-functions for G2-QHTs
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
// System includes
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <alloca.h>
#include <zlib.h>
// other
#include "other.h"
// Own includes
#define _G2QHT_C
#include "G2QHT.h"
// #include "G2MainServer.h"
#include "lib/log_facility.h"
#include "lib/my_bitops.h"

/* Internal Prototypes */

/* helper-funktions */
inline void g2_qht_clean(struct qhtable *to_clean)
{
	size_t tmp_dlen;

	if(!to_clean)
		return;

	tmp_dlen = to_clean->data_length;

	if(to_clean->fragments.data)
		free(to_clean->fragments.data);

	memset(to_clean, 0, offsetof(struct qhtable, data));
	memset(to_clean->data, -1, tmp_dlen);
	to_clean->data_length = tmp_dlen;
}

inline void g2_qht_free(struct qhtable *to_free)
{
	if(!to_free)
		return;

	if(to_free->fragments.data)
		free(to_free->fragments.data);

	free(to_free);
}

inline void g2_qht_frag_clean(struct qht_fragment *to_clean)
{
	if(!to_clean)
		return;

	if(to_clean->data)
		free(to_clean->data);

	memset(to_clean, 0, sizeof(*to_clean));
}

inline const char *g2_qht_patch(struct qhtable **ttable, struct qht_fragment *frag)
{
	struct qhtable *tmp_table;
	uint8_t *tmp_ptr;
	const char *ret_val;

	if(!ttable)
		return NULL;

	/* remove qht from source while we work on it */
	tmp_table = *ttable;
	*ttable = NULL;

	logg_develd_old("qlen: %lu\tflen: %lu\n", tmp_table->data_length, frag->length);

	switch(frag->compressed)
	{
	case COMP_DEFLATE:
		{
			int ret_int;
			z_stream patch_decoder;
			/* fix allocations... */	
			memset(&patch_decoder, 0, sizeof(patch_decoder));
// TODO: do something about this alloca, our BSD friends have little default task stacks (64k),
// allocating 128k will crash, so either:
// - raise the task stack size before creating the tasks
// - use a static buffer with a mutex
// - dynamically allocate it (from a buffer-cache?)
			tmp_ptr = alloca(tmp_table->data_length);

			patch_decoder.next_in = frag->data;
			patch_decoder.avail_in = frag->length;
			patch_decoder.next_out = tmp_ptr;
			patch_decoder.avail_out = tmp_table->data_length;

			if(Z_OK != inflateInit(&patch_decoder))
			{
				logg_devel("failure initiliasing compressed patch\n");
				*ttable = tmp_table;
				return NULL;
			} 
			if(Z_STREAM_END != (ret_int = inflate(&patch_decoder, Z_SYNC_FLUSH)))
			{
				logg_develd("failure decompressing patch: %i\n", ret_int);
				*ttable = tmp_table;
				return NULL;
			}
// TODO: fix memleak (inflateEnd)
		}
		ret_val = "compressed patch applied";
		break;
	case COMP_NONE:
		tmp_ptr = frag->data;
		ret_val = "patch applied";
		break;
	default:
		logg_develd("funky compression: %i\n", frag->compressed);
		*ttable = tmp_table;
		return NULL;
	}

	memxor(tmp_table->data, tmp_ptr, tmp_table->data_length);
	*ttable = tmp_table;

	return ret_val;
}

inline int g2_qht_add_frag(struct qhtable *ttable, struct qht_fragment *frag)
{
	uint8_t *tmp_ptr;
	struct qht_fragment *tablef = &ttable->fragments;
	if(!ttable)
		return -1;

	if(frag->compressed != COMP_NONE && frag->compressed != COMP_DEFLATE)
	{
		logg_develd("unknown compression %i in qht fragment\n", frag->compressed);
		return -1;
	}

	if(tablef->data)
	{
		if(frag->count != tablef->count)
		{
			logg_devel("fragment-count changed!\n");
			return -1;
		}

		if(frag->compressed != tablef->compressed)
		{
			logg_devel("compression changed!\n");
			return -1;
		}
	
		if(frag->nr != (tablef->nr + 1))
		{
			logg_devel("fragment missed\n");
			return -1;
		}
	}
	else if(1 == frag->nr)
	{
		tablef->compressed = frag->compressed;
		tablef->count = frag->count;
	}
	else
	{
		logg_devel("fragment not starting at one?\n");
		return -1;
	}

	{
		size_t legal_length = ttable->data_length;
// TODO: If the patch is compressed, we are just guessing a max length
		if(tablef->compressed)
			legal_length /= 2;

		if((tablef->length + frag->length) > legal_length)
		{
			logg_devel("patch to big\n");
			return -1;
		}
	}

	tablef->nr = frag->nr;

	tmp_ptr = realloc(tablef->data, tablef->length + frag->length);
	if(!tmp_ptr)
	{
		logg_errno(LOGF_DEBUG, "reallocating qht-fragments");
		return -1;
	}
	tablef->data = tmp_ptr;

	memcpy(tablef->data + tablef->length, frag->data, frag->length);
	tablef->length += frag->length;

	logg_develd_old("nr: %u\tcount: %u\n", tablef->nr, tablef->count);
	return tablef->nr < tablef->count ? 0 : 1;
}

inline bool g2_qht_reset(struct qhtable **ttable, uint32_t qht_ent)
{
	size_t w_size, bits;
	struct qhtable *tmp_table;

	if(!ttable)
	{
		logg_devel("called with NULL ttable?\n");
		return true;
	}

	w_size = (size_t)(qht_ent / BITS_PER_CHAR);
	w_size += (qht_ent % BITS_PER_CHAR) ? 1u : 0u;
	if(!qht_ent || w_size > MAX_BYTES_QHT)
	{
		logg_devel("illegal number of elements\n");
		return true;
	}

	/* qht_ent is zero-checked, we need the power, not the index, so -1 */
	bits = flsst(qht_ent) - 1;
	if((1 << bits) ^ qht_ent)
		logg_develd("TODO: %lu bits, %lu entries, can handle this, but hashes will be bogus\n",
			(unsigned long) bits, (unsigned long) qht_ent);

	/* 
	 * set the master table NULL, someone could traverse it,
	 * while we process it
	 */
	tmp_table = *ttable;
	*ttable = NULL;
	if(tmp_table)
	{
		if(tmp_table->data_length < w_size)
		{
			struct qhtable *tmp_table2 = realloc(tmp_table, sizeof(*tmp_table) + w_size);
			if(tmp_table2)
			{
				tmp_table = tmp_table2;
				tmp_table->data_length = w_size;
			}
			else
			{
				logg_errno(LOGF_DEBUG, "reallocating qh-table");
				qht_ent = tmp_table->entries;
				/* control flow will resurect old table, only wiped */
			}
		}
	}
	else
	{
		tmp_table = malloc(sizeof(*tmp_table) + w_size);
		if(!tmp_table)
		{
			logg_errno(LOGF_DEBUG, "allocating qh-table");
			return true;
		}
		tmp_table->fragments.data = NULL;
		tmp_table->data_length = w_size;
	}

	g2_qht_clean(tmp_table);
	tmp_table->entries = qht_ent;
	tmp_table->bits = bits;
	/* bring back the table */
	*ttable = tmp_table;
// TODO: Global QHT-needs-update flag (wich would need per connection locking)
	return false;
}

static char const rcsid[] GCC_ATTR_USED_VAR = "$Id:$";
// EOF
