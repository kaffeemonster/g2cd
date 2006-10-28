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
#include <pthread.h>
#include <zlib.h>
// other
#include "other.h"
// Own includes
#define _G2QHT_C
#include "G2QHT.h"
// #include "G2MainServer.h"
#include "lib/log_facility.h"
#include "lib/sec_buffer.h"
#include "lib/my_bitops.h"
#include "lib/my_bitopsm.h"
#include "other.h"

struct zpad_heap
{
	struct zpad_heap *next;
	/* 
	 * length should be aligned to sizeof(void *)
	 * so at least the LSB cann be used as a flag
	 */
	size_t length;
	char data[DYN_ARRAY_LEN];
};

struct zpad
{
	z_stream z;
	struct zpad_heap *pad_free;  // pointer in the pad to free space
	unsigned char pad[1<<14];    // other allok space, 16k
	unsigned char window[1<<15]; // 15 Bit window size, should result in 32k byte
};

struct scratch
{
	size_t length;
	unsigned char data[DYN_ARRAY_LEN];
};

/* Vars */
static pthread_key_t key2qht_zpad;
static pthread_key_t key2qht_scratch1;

/* Internal Prototypes */
	/* do not remove this proto, our it won't work... */
static void qht_init(void) GCC_ATTR_CONSTRUCT;
static void qht_end(void *);
static void *qht_zpad_alloc(void *, unsigned int, unsigned int);
static void qht_zpad_free(void *, void *);
static inline void qht_zpad_merge(struct zpad_heap *);
static inline struct zpad *qht_get_zpad(void);

/* tls thingies */
static void qht_init(void)
{
	if(pthread_key_create(&key2qht_zpad, qht_end))
		diedie("couldn't create TLS key for qht");

	if(pthread_key_create(&key2qht_scratch1, qht_end))
		diedie("couldn't create TLS key for qht");
}

static void qht_end(void *to_free)
{
	if(!to_free)
		return;

	free(to_free);
}

/* zpad helper */
/*
 * Poor mans allocator for zlib
 *
 * normaly zlib makes two allocs, one seams to be the internal
 * state (+9k on 32bit *cough*) and the other is the window, norm
 * 32k for 15 bit.
 *
 * So this is a very stupid alloactor to serve these two allocs.
 * But, not to be too dependent on these internals (future zlib
 * versions) it is at least so intelligent it may also handle 3
 * allocs, and frees, maybe ;)
 *
 * !! THE heap for this zpad instance is NOT locked !!
 */
static void *qht_zpad_alloc(void *opaque, unsigned int num, unsigned int size)
{
	struct zpad *z_pad = opaque;
	size_t bsize;

	if(!z_pad)
		goto err_record;

	/* mildly sanetiy check */
	if( num > sizeof(z_pad->pad) + sizeof(z_pad->window) || 
		size > sizeof(z_pad->pad) + sizeof(z_pad->window))
		goto err_record;
	bsize = num * size;

	/* bsize should afterwards have the LSB cleared */
	bsize = ALIGN_SIZE(bsize, sizeof(void *));

	if(bsize + sizeof(struct zpad_heap) > sizeof(z_pad->pad) + sizeof(z_pad->window))
		goto err_record;

	/* start lock */
	{
		struct zpad_heap *zheap = z_pad->pad_free, *tmp_zheap;
		size_t org_len = zheap->length & ~1; /* mask out the free flag */
		/* 
		 * normaly we simply allocate at the end, and 'nuff said, but
		 * to be complete...
		 */
		if(!(zheap->length & 1) || bsize + sizeof(*zheap) > org_len)
		{
			tmp_zheap = (struct zpad_heap *) z_pad->pad;
			/* walk the heap list */
			do
			{
				size_t tmp_len;
retry:
				tmp_len = tmp_zheap->length & ~1;
				/* if free and big enough */
				if(tmp_zheap->length & 1)
				{
					if(bsize + sizeof(*zheap) <= tmp_len)
					{
						org_len = tmp_len;
						zheap = tmp_zheap;
						goto insert_in_heap;
					}
					else if(tmp_zheap->next && (tmp_zheap->next->length & 1))
					{
						qht_zpad_merge(tmp_zheap);
						goto retry;
					}
				}
			} while((tmp_zheap = tmp_zheap->next));
			goto err_record;
		}
insert_in_heap:
		zheap->length     = bsize;
		/* Mind the parentheses... */
		tmp_zheap         = (struct zpad_heap*)(zheap->data + bsize);
		if((unsigned char *)tmp_zheap < z_pad->pad)
			die("zpad heap probably corrupted, underflow!\n");
		if((unsigned char *)tmp_zheap > (z_pad->pad + sizeof(z_pad->pad) + sizeof(z_pad->window) - sizeof(*zheap)))
		{
			logg_posd(LOGF_EMERG, "zpad heap probably corrupted, overflow!\n%p > %p + %u + %u - %u = %p\n",
				(void *)tmp_zheap, z_pad->pad, sizeof(z_pad->pad), sizeof(z_pad->window), sizeof(*zheap),
				(void *)(z_pad->pad + sizeof(z_pad->pad) + sizeof(z_pad->window) - sizeof(*zheap)));
			exit(EXIT_FAILURE);
		}
		tmp_zheap->length = (org_len - bsize - sizeof(*zheap)) | 1;
		tmp_zheap->next   = zheap->next;
		zheap->next       = tmp_zheap;
		z_pad->pad_free   = tmp_zheap;
	/* end lock */
		logg_develd("zpad alloc: n: %u\ts: %u\tb: %lu\tp: 0x%p\tzh: 0x%p\tzhn: 0x%p\n",
			num, size, (unsigned long) bsize, (void *) z_pad->pad, (void *) zheap, (void *) tmp_zheap);
		return zheap->data;
	}
err_record:
	logg_develd("zpad alloc failed!! 0x%p\tn: %u\ts: %u\n", opaque, num, size);
	return NULL;
}

static void qht_zpad_free(void *opaque, void *addr)
{
	struct zpad_heap *zheap;

	if(!addr || !opaque)
		return;

	/* 
	 * watch the parentheses (/me cuddles his new vim 7), or you
	 * have wrong pointer-arith, and a SIGSEV on 3rd. alloc...
	 */
	zheap = (struct zpad_heap *) ((char *)addr - offsetof(struct zpad_heap, data));
	zheap->length |= 1; // mark free

	logg_develd("zpad free: o: %p\ta: %p\tzh: %p\n", opaque, addr, (void *)zheap);

	qht_zpad_merge(zheap);
}

static inline void qht_zpad_merge(struct zpad_heap *zheap)
{
	while(zheap->next)
	{
		if(zheap->next->length & 1) // if next block is free
		{
			zheap->length += (zheap->next->length & ~1) + sizeof(struct zpad_heap);
			zheap->next = zheap->next->next;
		}
		else
			zheap = zheap->next;
	}
}

static inline struct zpad *qht_get_zpad(void)
{
	struct zpad *z_pad = pthread_getspecific(key2qht_zpad);

	if(z_pad)
		return z_pad;

	z_pad = malloc(sizeof(*z_pad));
	if(!z_pad)
	{
		logg_errno(LOGF_DEVEL, "no z_pad");
		return NULL;
	}

	if(pthread_setspecific(key2qht_zpad, z_pad))
	{
		logg_errno(LOGF_CRIT, "qht-zpad key not initilised?");
		free(z_pad);
		return NULL;
	}
	memset(&z_pad->z, 0, sizeof(z_pad->z));
	z_pad->pad_free         = (struct zpad_heap *)z_pad->pad;
	z_pad->pad_free->next   = NULL;
	z_pad->pad_free->length = (sizeof(z_pad->pad) + sizeof(z_pad->window)) | 1; /* set the free bit */
	z_pad->z.zalloc         = qht_zpad_alloc;
	z_pad->z.zfree          = qht_zpad_free;
	z_pad->z.opaque         = z_pad;

	return z_pad;
}

/**/
static inline unsigned char *qht_get_scratch1(size_t length)
{
	struct scratch *scratch = pthread_getspecific(key2qht_scratch1);

	if(scratch)
	{
		if(scratch->length >= length)
			return scratch->data;
		else
		{
			free(scratch);
			scratch = NULL;
			if(pthread_setspecific(key2qht_scratch1, NULL))
				diedie("could not remove qht scratch!");
		}
	}

	scratch = malloc(sizeof(*scratch) + length);

	if(!scratch)
	{
		logg_errno(LOGF_DEVEL, "no qht scratchram");
		return NULL;
	}
	scratch->length = length;

	if(pthread_setspecific(key2qht_scratch1, scratch))
	{
		logg_errno(LOGF_CRIT, "qht scratch key not initilised?");
		free(scratch);
		return NULL;
	}

	return scratch->data;
}

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

/* funcs */
inline const char *g2_qht_patch(struct qhtable **ttable, struct qht_fragment *frag)
{
	struct qhtable *tmp_table;
	const char *ret_val = NULL;

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
			unsigned char *tmp_ptr = qht_get_scratch1(tmp_table->data_length);
			struct zpad *zpad = qht_get_zpad();

			if(!(tmp_ptr && zpad))
			{
				logg_devel("patch mem failed\n");
				break;
			}

			/* fix allocations... done, TLS rocks ;) */	
			zpad->z.next_in = frag->data;
			zpad->z.avail_in = frag->length;
			zpad->z.next_out = tmp_ptr;
			zpad->z.avail_out = tmp_table->data_length;

			if(Z_OK != inflateInit(&zpad->z))
				logg_devel("failure initiliasing compressed patch\n");
			else if(Z_STREAM_END != (ret_int = inflate(&zpad->z, Z_SYNC_FLUSH)))
				logg_develd("failure decompressing patch: %i\n", ret_int);
			else
			{
				/* apply it */
				memxor(tmp_table->data, tmp_ptr, tmp_table->data_length);
				ret_val = "compressed patch applied";
			}

			inflateEnd(&zpad->z);
		}
		break;
	case COMP_NONE:
		memxor(tmp_table->data, frag->data, tmp_table->data_length);
		ret_val = "patch applied";
		break;
	default:
		logg_develd("funky compression: %i\n", frag->compressed);
	}

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

static char const rcsid_q[] GCC_ATTR_USED_VAR = "$Id:$";
// EOF