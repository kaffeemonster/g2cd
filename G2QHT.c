/*
 * G2QHT.c
 * helper-functions for G2-QHTs
 *
 * Copyright (c) 2006-2009 Jan Seiffert
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
/* debug file */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zlib.h>
/* other */
#include "lib/other.h"
/* Own includes */
#define _G2QHT_C
#include "G2QHT.h"
// #include "G2MainServer.h"
#include "lib/log_facility.h"
#include "lib/sec_buffer.h"
#include "lib/my_bitops.h"
#include "lib/my_bitopsm.h"
#include "lib/hzp.h"
#include "lib/atomic.h"

/* RLE maximum size */
// TODO: take less?
#define QHT_RLE_MAXSIZE (1 << 14) /* 16k */

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

/* unfortunatly, the deflater is memory greedy... */
struct zpad
{
	z_stream z;
	struct zpad_heap *pad_free;                   /* pointer in the pad to free space */
	unsigned char pad[1<<19] GCC_ATTR_ALIGNED(8); /* other allok space, 512k */
	unsigned char window[1<<16];                  /* 16 Bit window size, should result in 64k */
};

struct scratch
{
	size_t length;
	unsigned char data[DYN_ARRAY_LEN];
};

/* Vars */
static pthread_key_t key2qht_zpad;
static pthread_key_t key2qht_scratch1;
static pthread_key_t key2qht_scratch2;

/* Internal Prototypes */
	/* do not remove this proto, our it won't work... */
static void qht_init(void) GCC_ATTR_CONSTRUCT;
static void qht_deinit(void) GCC_ATTR_DESTRUCT;
static void *qht_zpad_alloc(void *, unsigned int, unsigned int);
static void qht_zpad_free(void *, void *);
static inline void qht_zpad_merge(struct zpad_heap *);
static inline struct zpad *qht_get_zpad(void);
static void g2_qht_free_hzp(void *);
#ifdef QHT_DUMP
static void qht_dump_init(void);
static void qht_dump_deinit(void);
static void qht_sdump(void *, size_t);
static void qht_dump(void *, void *, size_t);
#else
# define qht_dump_init()
# define qht_dump_deinit()
# define qht_sdump(x, y) do { } while(0)
# define qht_dump(x, y, z) do { } while(0)
#endif

/* tls thingies */
static void qht_init(void)
{
	qht_dump_init();
	if(pthread_key_create(&key2qht_zpad, free))
		diedie("couldn't create TLS key for qht zpad");

	if(pthread_key_create(&key2qht_scratch1, free))
		diedie("couldn't create TLS key for qht scratch1");

	if(pthread_key_create(&key2qht_scratch2, free))
		diedie("couldn't create TLS key for qht scratch2");
}

static void qht_deinit(void)
{
	qht_dump_deinit();
	pthread_key_delete(key2qht_zpad);
	pthread_key_delete(key2qht_scratch1);
	pthread_key_delete(key2qht_scratch2);
}

/* zpad helper */
/*
 * Poor mans allocator for zlib
 *
 * normaly zlib makes two allocs, one seems to be the internal
 * state (+9k on 32bit *cough*) and the other is the window, norm
 * 32k for 15 bit.
 *
 * So this is a very stupid alloactor to serve these two allocs.
 * But, not to be too dependent on these internals (future zlib
 * versions) it is at least so intelligent it may also handle 3
 * allocs, and frees, maybe ;)
 *
 * And since a deflater is very memory hungry, one time 6k,
 * three (!!!) times 32k * 2 and one time 16k * 4, total over
 * 260k, with a little more space in the pad we also can handle
 * a deflater.
 *
 * !! THE heap for this zpad instance is NOT locked !!
 * Why: it should be thread local to this deflater/inflater!
 */
static void *qht_zpad_alloc(void *opaque, unsigned int num, unsigned int size)
{
	struct zpad *z_pad = opaque;
	struct zpad_heap *zheap, *tmp_zheap;
	size_t bsize, org_len;

	if(unlikely(!z_pad))
		goto err_record;

	/* mildly sanetiy check */
	if(unlikely( num > sizeof(z_pad->pad) + sizeof(z_pad->window)) ||
	   unlikely(size > sizeof(z_pad->pad) + sizeof(z_pad->window)) ||
	   unlikely(size > UINT_MAX / num))
		goto err_record;
	bsize = num * size;

	/* bsize should afterwards have the LSB cleared */
	bsize = ALIGN_SIZE(bsize, sizeof(void *));

	if(unlikely(bsize + sizeof(struct zpad_heap) > sizeof(z_pad->pad) + sizeof(z_pad->window)))
		goto err_record;

	/* start lock */
	zheap = z_pad->pad_free;
	org_len = zheap->length & ~1; /* mask out the free flag */
	/*
	 * normaly we simply allocate at the end, and 'nuff said, but
	 * to be complete...
	 */
	if(unlikely(!(zheap->length & 1) || bsize + sizeof(*zheap) > org_len))
	{
		tmp_zheap = (struct zpad_heap *)z_pad->pad;
		/* walk the heap list */
		do
		{
			size_t tmp_len = tmp_zheap->length & ~1;
			/* if free and big enough */
			if(likely(tmp_zheap->length & 1))
			{
				if(bsize + sizeof(*zheap) <= tmp_len) {
					org_len = tmp_len;
					zheap = tmp_zheap;
					goto insert_in_heap;
				}
				if(tmp_zheap->next && (tmp_zheap->next->length & 1)) {
					qht_zpad_merge(tmp_zheap);
					continue;
				}
			}
			tmp_zheap = tmp_zheap->next;
		} while(tmp_zheap);
		goto err_record;
	}
insert_in_heap:
	zheap->length     = bsize;
	/* Mind the parentheses... */
	tmp_zheap         = (struct zpad_heap*)(zheap->data + bsize);
	if(unlikely((unsigned char *)tmp_zheap < z_pad->pad))
		die("zpad heap probably corrupted, underflow!\n");
	if(unlikely((unsigned char *)tmp_zheap > (z_pad->pad + sizeof(z_pad->pad) + sizeof(z_pad->window) - sizeof(*zheap))))
	{
		logg_posd(LOGF_EMERG, "zpad heap probably corrupted, overflow!\n%p > %p + %zu + %zu - %zu = %p\n",
		          tmp_zheap, z_pad->pad, sizeof(z_pad->pad), sizeof(z_pad->window), sizeof(*zheap),
		          z_pad->pad + sizeof(z_pad->pad) + sizeof(z_pad->window) - sizeof(*zheap));
		exit(EXIT_FAILURE);
	}
	tmp_zheap->length = (org_len - bsize - sizeof(*zheap)) | 1;
	tmp_zheap->next   = zheap->next;
	zheap->next       = tmp_zheap;
	z_pad->pad_free   = tmp_zheap;
/* end lock */

	logg_develd_old("zpad alloc: n: %u\ts: %u\tb: %zu\tp: %p\tzh: %p\tzhn: %p\n",
	                num, size, bsize, z_pad->pad, zheap, tmp_zheap);
	return zheap->data;

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

	logg_develd_old("zpad free: o: %p\ta: %p\tzh: %p\n", opaque, addr, zheap);

	qht_zpad_merge(zheap);
}

static inline void qht_zpad_merge(struct zpad_heap *zheap)
{
	while(zheap->next)
	{
		if(zheap->next->length & 1) { /* if next block is free */
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

	if(unlikely(!z_pad))
	{
		z_pad = malloc(sizeof(*z_pad));
		if(!z_pad) {
			logg_errno(LOGF_DEVEL, "no z_pad");
			return NULL;
		}
		if(pthread_setspecific(key2qht_zpad, z_pad)) {
			logg_errno(LOGF_CRIT, "qht-zpad key not initilised?");
			free(z_pad);
			return NULL;
		}
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
static inline unsigned char *qht_get_scratch_intern(size_t length, pthread_key_t scratch_key)
{
	struct scratch *scratch = pthread_getspecific(scratch_key);

	/*
	 * ompf, very unpleasant...
	 * we want the ->data aligned to 16. Since it is a char gcc
	 * alinges it to 1 so effectivly 4/8 with the length in front.
	 * We could tell gcc to align data to 16, but then we get a
	 * nice interference with malloc. It alignes this to 4 ;(
	 * (and i don't understand why... only because it is a huge
	 * alloc? and if i want to put doubles in it?)
	 * And since every malloc could align it to another boundery
	 * we fix it up once and by hand...
	 */
	length += 16;
	if(scratch)
	{
		if(scratch->length >= length)
			return (unsigned char *)ALIGN(scratch->data, 16);
		else
		{
			free(scratch);
			scratch = NULL;
			if(pthread_setspecific(scratch_key, NULL))
				diedie("could not remove qht scratch!");
		}
	}

	scratch = malloc(sizeof(*scratch) + length);

	logg_develd_old("scratch: %p, %lu\n", (void *)scratch, (unsigned long)scratch);

	if(!scratch) {
		logg_errno(LOGF_DEVEL, "no qht scratchram");
		return NULL;
	}
	scratch->length = length;

	if(pthread_setspecific(scratch_key, scratch)) {
		logg_errno(LOGF_CRIT, "qht scratch key not initilised?");
		free(scratch);
		return NULL;
	}

	return (unsigned char *)ALIGN(scratch->data, 16);
}

static inline unsigned char *qht_get_scratch1(size_t length)
{
	return qht_get_scratch_intern(length, key2qht_scratch1);
}

static inline unsigned char *qht_get_scratch2(size_t length)
{
	return qht_get_scratch_intern(length, key2qht_scratch2);
}

static inline void _g2_qht_clean(struct qhtable *to_clean, bool reset_needed)
{
	if(!to_clean)
		return;

	g2_qht_frag_free(to_clean->fragments);

	memset(&to_clean->entries, 0, offsetof(struct qhtable, data) - offsetof(struct qhtable, entries));
	if(reset_needed)
		to_clean->flags.reset_needed = true;
	else if(to_clean->data)
		memset(to_clean->data, ~0, to_clean->data_length);
}

/* helper-funktions */
void g2_qht_clean(struct qhtable *to_clean)
{
	_g2_qht_clean(to_clean, true);
}

static void g2_qht_free_hzp(void *qtable)
{
	struct qhtable *to_free;

	if(!qtable)
		return;
	to_free = qtable;
	if(atomic_read(&to_free->refcnt) > 0) {
		logg_develd("qht refcnt race %p %i\n", (void*)to_free, atomic_read(&to_free->refcnt));
		return;
	}
	g2_qht_frag_free(to_free->fragments);
	free(to_free->data);
	free(to_free);
}

void g2_qht_put(struct qhtable *to_free)
{
	if(!to_free)
		return;

	if(atomic_dec_test(&to_free->refcnt))
		hzp_deferfree(&to_free->hzp, to_free, g2_qht_free_hzp);
	else if(atomic_read(&to_free->refcnt) < 0)
		logg_develd("qht refcnt below zero! %p %i\n", (void*)to_free, atomic_read(&to_free->refcnt));
}

/* funcs */
static bool qht_compress_table(struct qhtable *table, uint8_t *data, size_t qht_size)
{
	uint8_t *ndata = qht_get_scratch1(1 << 20); /* try to not realloc scratch */
	ssize_t res;
	uint8_t *t_data;
	void *t_x = NULL;

	res = bitfield_encode(ndata, QHT_RLE_MAXSIZE, data, qht_size);
	if(res < 0)
	{
		logg_develd("QHT %p could not be compressed!\n", table);
		t_data = malloc(qht_size);
		if(!t_data) {
			logg_develd("could not allocate memory for not compressable QHT: %zu\n", qht_size);
			return false;
		}
		memcpy(t_data, data, qht_size);
		if(table->data)
		{
			t_x = atomic_px(t_x, (atomicptr_t *)&table->data);
			if(table->data_length >= sizeof(struct hzp_free))
				hzp_deferfree(t_x, t_x, free);
			else
				free(t_x);
		}
		table->compressed = COMP_NONE;
		table->data_length = qht_size;
		t_x = atomic_px(t_data, (atomicptr_t *)&table->data);
		if(t_x)
		{
			if(table->data_length >= sizeof(struct hzp_free))
				hzp_deferfree(t_x, t_x, free);
			else
				free(t_x);
		}
	}
	else
	{
		logg_develd("QHT %p compressed - 1:%u.%u\n", table,
		            (unsigned)(qht_size / res),
		            (unsigned)(((qht_size % res) * 1000) / res));
		t_data = malloc(res);
		if(!t_data) {
			logg_develd("could not allocate memory for compressable QHT: %zu\n", res);
			return false;
		}
		memcpy(t_data, ndata, res);
		if(table->data)
		{
			t_x = atomic_px(t_x, (atomicptr_t *)&table->data);
			if(table->data_length >= sizeof(struct hzp_free))
				hzp_deferfree(t_x, t_x, free);
			else
				free(t_x);
// TODO: use after free is possible here
		}
		table->compressed = COMP_RLE;
		table->data_length = res;
		t_x = atomic_px(t_data, (atomicptr_t *)&table->data);
		if(t_x) {
			if(table->data_length >= sizeof(struct hzp_free))
				hzp_deferfree(t_x, t_x, free);
			else
				free(t_x);
		}
	}
	return true;
}

const char *g2_qht_patch(struct qhtable *table, struct qht_fragment *frag)
{
	const char *ret_val = NULL;
	unsigned char *target_ptr;
	size_t pos, qht_size;

	if(!table || !frag)
		return NULL;

	qht_size = DIV_ROUNDUP(table->entries, BITS_PER_CHAR);

	if(atomic_read(&table->refcnt) > 1)
		logg_develd("WARNING: patch called on table with refcnt > 1!! %p %i\n", (void*)table, atomic_read(&table->refcnt));

	logg_develd_old("qlen: %lu\tflen: %lu\n", table->data_length, frag->length);

/*	if(table->flags.reset_needed)
		logg_develd("reset_needed qht-table passed: %p\n", (void *) table);*/

// TODO: prevent hub QHTs getting compressed

	if(COMP_RLE == table->compressed || table->flags.reset_needed)
	{
		target_ptr = qht_get_scratch2(qht_size);
		if(!target_ptr)
			return NULL;
		if(COMP_RLE == table->compressed && !table->flags.reset_needed) {
			ssize_t res = bitfield_decode(target_ptr, qht_size, table->data, table->data_length);
			if(res < 0) {
				logg_develd("failed to de-rle QHT: %zi\n", res);
				return NULL;
			}
		}
	}
	else
		target_ptr = table->data;

	switch(frag->compressed)
	{
	case COMP_DEFLATE:
		{
			int res;
			unsigned char *tmp_ptr = qht_get_scratch1(qht_size);
			struct zpad *zpad = qht_get_zpad();

			if(!(tmp_ptr && zpad)) {
				logg_devel("patch mem failed\n");
				break;
			}

			/* fix allocations... done, TLS rocks ;) */
			zpad->z.next_out = tmp_ptr;
			zpad->z.avail_out = qht_size;
			zpad->z.next_in = frag->data;
			zpad->z.avail_in = frag->length;
			frag = frag->next;

			if(Z_OK != inflateInit(&zpad->z)) {
				inflateEnd(&zpad->z);
				logg_devel("failure initiliasing patch decompressor\n");
				break;
			}
			do
			{
				res = inflate(&zpad->z, Z_SYNC_FLUSH);
				if(Z_OK == res && !zpad->z.avail_in)
				{
					if(!frag)
						break;
					zpad->z.next_in = frag->data;
					zpad->z.avail_in = frag->length;
					frag = frag->next;
				} else if(Z_STREAM_END != res) {
					logg_develd("failure compressing patch: %i\n", res);
					break;
				}
			} while(Z_STREAM_END != res);
			inflateEnd(&zpad->z);
			if(Z_STREAM_END != res) {
				logg_develd("failure decompressing patch: %i\n", res);
				break;
			}

			/* apply it */
			if(table->flags.reset_needed)
				memneg(target_ptr, tmp_ptr, qht_size);
			else
				memxor(target_ptr, tmp_ptr, qht_size);
			qht_dump(target_ptr, tmp_ptr, qht_size);
			if(server.settings.qht.compress_internal &&
			   (COMP_RLE == table->compressed || table->flags.reset_needed))
				if(!qht_compress_table(table, target_ptr, qht_size))
					break;
			table->flags.reset_needed = false;
			ret_val = "compressed patch applied";
		}
		break;
	case COMP_NONE:
		/* size for no compression case is already checked */
		for(pos = 0; frag; frag = frag->next)
		{
			if(table->flags.reset_needed)
				memneg(target_ptr + pos, frag->data, frag->length);
			else
				memxor(target_ptr + pos, frag->data, frag->length);
			qht_dump(target_ptr + pos, frag->data, frag->length);
			pos += frag->length;
		}

		if(server.settings.qht.compress_internal &&
		   (COMP_RLE == table->compressed || table->flags.reset_needed))
			if(!qht_compress_table(table, target_ptr, qht_size))
				break;
		table->flags.reset_needed = false;
		ret_val = "patch applied";
		break;
	default:
		logg_develd("funky compression: %i\n", frag->compressed);
	}

	table->time_stamp = local_time_now;

	return ret_val;
}

void g2_qht_aggregate(struct qhtable *to, struct qhtable *from)
{
	size_t qht_size = DIV_ROUNDUP(to->entries, BITS_PER_CHAR);
	uint8_t *from_ptr;

	qht_size = qht_size <= DIV_ROUNDUP(from->entries, BITS_PER_CHAR) ? qht_size :
	           DIV_ROUNDUP(from->entries, BITS_PER_CHAR);

	if(COMP_RLE == from->compressed)
	{
		ssize_t res;
		from_ptr = qht_get_scratch1(qht_size);
		if(!from_ptr)
			return;
		res = bitfield_decode(from_ptr, qht_size, from->data, from->data_length);
		if(res < 0)
			logg_develd("failed to de-rle QHT: %zi\n", res);
			/* means we have not enough space, but we can use the bytes... */
	}
	else
		from_ptr = from->data;

	if(to->flags.reset_needed) {
		memcpy(to->data, from_ptr, qht_size);
		to->flags.reset_needed = false;
	} else
		memand(to->data, from_ptr, qht_size);
}

struct qht_fragment *g2_qht_frag_alloc(size_t len)
{
	struct qht_fragment *ret_val;

	ret_val = malloc(sizeof(*ret_val) + len);
	if(ret_val) {
		ret_val->next   = NULL;
		ret_val->length = len;
	}
	return ret_val;
}

void g2_qht_frag_free(struct qht_fragment *to_free)
{
	if(!to_free)
		return;
	g2_qht_frag_free(to_free->next);
	free(to_free);
}

struct qht_fragment *g2_qht_diff_get_frag(const struct qhtable *org, const struct qhtable *new)
{
	struct qht_fragment *ret_val = NULL, *n = NULL, *t;
	struct zpad *zpad;
	unsigned char *tmp_ptr;
	size_t len, pos, frag_len, frags = 0;
	int res;

	if(unlikely(!new))
		return NULL;

// TODO: we assume that fragment generating QHTs are not compressed (master qht)

	len = org ? org->data_length : new->data_length;
	len = new->data_length < len ? new->data_length : len;

	switch(server.settings.qht.compression)
	{
	default:
		logg_develd("funky compression: %i\n", server.settings.qht.compression);
	case COMP_NONE:
		pos = 0;
		do
		{
			frag_len = len > (NORM_BUFF_CAPACITY - QHT_PATCH_HEADER_BYTES) ?
			           NORM_BUFF_CAPACITY : len + QHT_PATCH_HEADER_BYTES;
			t = g2_qht_frag_alloc(frag_len);
			if(!t)
				break;
			t->nr = ++frags;
			tmp_ptr = t->data + QHT_PATCH_HEADER_BYTES;
			frag_len -= QHT_PATCH_HEADER_BYTES;
			len -= frag_len;

			if(!org)
				memneg(tmp_ptr, new->data + pos, frag_len);
			else
				memxorcpy(tmp_ptr, org->data + pos, new->data + pos, frag_len);
			qht_sdump(tmp_ptr, frag_len);
			pos += frag_len;
			t->next = n;
			n = t;
		} while(len);
		if(n) {
			t = n->next;
			n->next = NULL;
			ret_val = n;
			ret_val->count = frags;
			ret_val->compressed = COMP_NONE;
		}
		while(t)
		{
			n = t;
			t = t->next;
			n->next = ret_val;
			ret_val = n;
			ret_val->count = frags;
			ret_val->compressed = COMP_NONE;
		}
		break;
	case COMP_DEFLATE:
		tmp_ptr = qht_get_scratch1(len);
		zpad = qht_get_zpad();

		if(!(tmp_ptr && zpad)) {
			logg_devel("patch mem failed\n");
			break;
		}

		if(!org)
			memneg(tmp_ptr, new->data, len);
		else
			memxorcpy(tmp_ptr, org->data, new->data, len);
		qht_sdump(tmp_ptr, len);

		zpad->z.next_in = tmp_ptr;
		zpad->z.avail_in = len;

		if(Z_OK != deflateInit(&zpad->z, Z_DEFAULT_COMPRESSION)) {
			deflateEnd(&zpad->z);
			logg_devel("failure initiliasing patch compressor\n");
			break;
		}
		do
		{
			t = g2_qht_frag_alloc(NORM_BUFF_CAPACITY);
			if(!t)
				break;
			t->nr = ++frags;

			zpad->z.next_out = t->data + QHT_PATCH_HEADER_BYTES;
			zpad->z.avail_out = t->length - QHT_PATCH_HEADER_BYTES;
			res = deflate(&zpad->z, Z_FINISH);
			if(Z_OK == res || Z_STREAM_END == res) {
				t->length -= zpad->z.avail_out;
				t->next = n;
				n = t;
			} else {
				logg_develd("failure compressing patch: %i\n", res);
				break;
			}
		} while(Z_STREAM_END != res);
		deflateEnd(&zpad->z);
		if(n) {
			t = n->next;
			n->next = NULL;
			ret_val = n;
			ret_val->count = frags;
			ret_val->compressed = COMP_DEFLATE;
		}
		while(t)
		{
			n = t;
			t = t->next;
			n->next = ret_val;
			ret_val = n;
			ret_val->count = frags;
			ret_val->compressed = COMP_DEFLATE;
		}
	}

	return ret_val;
}

int g2_qht_add_frag(struct qhtable *ttable, struct qht_fragment *frag, uint8_t *data)
{
	struct qht_fragment **t, *n;
	size_t total_len, legal_len;

	if(!ttable)
		return -1;

	if(frag->compressed != COMP_NONE && frag->compressed != COMP_DEFLATE) {
		logg_develd("unknown compression %i in qht fragment\n", frag->compressed);
		return -1;
	}

	t = &ttable->fragments;
	n = *t;
	if(n)
	{
		if(frag->count != n->count) {
			logg_devel("fragment-count changed!\n");
			return -1;
		}
		if(frag->compressed != n->compressed) {
			logg_devel("compression changed!\n");
			return -1;
		}
	} else if(1 != frag->nr) {
		logg_devel("fragment not starting at one?\n");
		return -1;
	}

	total_len = 0;
	for(; n; n = (*t)->next, t = &(*t)->next)
		total_len += n->length;

	if(n && frag->nr != (n->nr + 1)) {
		logg_devel("fragment missed\n");
		return -1;
	}

	legal_len = DIV_ROUNDUP(ttable->entries, BITS_PER_CHAR);
// TODO: If the patch is compressed, we are just guessing a max length
	if(frag->compressed)
		legal_len /= 2;
	/*
	 * ATM check is sane, we are checking the real sizes of the
	 * really recieved data, not user supplied data...
	 */
	if((total_len + frag->length) > legal_len) {
		logg_devel("patch to big\n");
		return -1;
	}

	memcpy(frag->data, data, frag->length);
	*t = frag;

	logg_develd_old("nr: %u\tcount: %u\n", frag->nr, frag->count);
	return frag->nr >= frag->count;
}

bool g2_qht_reset(struct qhtable **ttable, uint32_t qht_ent, bool try_compress)
{
	size_t w_size, bits;
	struct qhtable *tmp_table;

	if(!ttable) {
		logg_devel("called with NULL ttable?\n");
		return true;
	}

	w_size = DIV_ROUNDUP((size_t)qht_ent, BITS_PER_CHAR);
	if(!qht_ent || w_size > MAX_BYTES_QHT) {
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
	if(!tmp_table)
	{
		tmp_table = malloc(sizeof(*tmp_table));
		if(!tmp_table) {
			logg_errno(LOGF_DEBUG, "allocating qh-table");
			return true;
		}
		atomic_set(&tmp_table->refcnt, 1);
		tmp_table->fragments = NULL;
		tmp_table->data = NULL;
		tmp_table->data_length = 0;
	}
	if(!try_compress)
	{
		if(tmp_table->data_length < w_size)
		{
			uint8_t *ttable_data;
			if(atomic_read(&tmp_table->refcnt) > 1)
				logg_develd("WARNING: reset on qht with refcnt > 1!! %p %i\n", (void*)tmp_table, atomic_read(&tmp_table->refcnt));
			ttable_data = realloc(tmp_table->data, w_size);
			if(ttable_data) {
				tmp_table->data = ttable_data;
				tmp_table->data_length = w_size;
			} else {
				logg_errno(LOGF_DEBUG, "reallocating qh-table data");
				qht_ent = tmp_table->entries;
				/* control flow will resurect old table, only wiped */
			}
		}
	} else if(tmp_table->data) {
		hzp_deferfree((struct hzp_free *)tmp_table->data, tmp_table->data, free);
		tmp_table->data = NULL;
		tmp_table->data_length = 0;
	}

	_g2_qht_clean(tmp_table, true);
	tmp_table->entries = qht_ent;
	tmp_table->bits = bits;
	tmp_table->time_stamp = local_time_now;
	/* bring back the table */
	*ttable = tmp_table;
	return false;
}

#ifdef QHT_DUMP
static int qht_tdump_fd, qht_pdump_fd, qht_sdump_fd;
static void qht_dump_init(void)
{
	char tmp_nam[sizeof("./G2QHTtdump.bin") + 12];
	my_snprintf(tmp_nam, sizeof(tmp_nam), "./G2QHTtdump%lu.bin", (unsigned long)getpid());
	if(0 > (qht_tdump_fd = open(tmp_nam, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)))
		logg_errno(LOGF_ERR, "opening QHT-table-file");
	my_snprintf(tmp_nam, sizeof(tmp_nam), "./G2QHTpdump%lu.bin", (unsigned long)getpid());
	if(0 > (qht_pdump_fd = open(tmp_nam, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)))
		logg_errno(LOGF_ERR, "opening QHT-patch-file");
	my_snprintf(tmp_nam, sizeof(tmp_nam), "./G2QHTsdump%lu.bin", (unsigned long)getpid());
	if(0 > (qht_sdump_fd = open(tmp_nam, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)))
		logg_errno(LOGF_ERR, "opening QHT-spatch-file");
}

static void qht_dump_deinit(void)
{
	if(0 <= qht_tdump_fd)
		close(qht_tdump_fd);
	if(0 <= qht_pdump_fd)
		close(qht_pdump_fd);
	if(0 <= qht_sdump_fd)
		close(qht_sdump_fd);
}

static void qht_sdump(void *patch, size_t len)
{
	write(qht_sdump_fd, patch, len);
}

static void qht_dump(void *table, void *patch, size_t len)
{
	write(qht_tdump_fd, table, len);
	write(qht_pdump_fd, patch, len);
}
#endif

static char const rcsid_q[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
