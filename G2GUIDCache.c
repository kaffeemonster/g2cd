/*
 * G2GUIDCache.c
 * known GUID cache for routing
 *
 * Copyright (c) 2009 Jan Seiffert
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
/* System */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#endif
#include <errno.h>
#include <pthread.h>
/* Own */
#define _G2GUIDCACHE_C
#include "lib/other.h"
#include "G2GUIDCache.h"
#include "G2MainServer.h"
#include "lib/log_facility.h"
#include "lib/combo_addr.h"
#include "lib/hlist.h"
#include "lib/rbtree.h"
#include "lib/my_bitops.h"
#include "lib/ansi_prng.h"

#define GUID_CACHE_SHIFT 14
#define GUID_CACHE_SIZE (1 << GUID_CACHE_SHIFT)
#define GUID_CACHE_HTSIZE (GUID_CACHE_SIZE/64)
#define GUID_CACHE_HTMASK (GUID_CACHE_HTSIZE-1)

#define GUID_DUMP_IDENT "G2CDGUIDDUMP\n"
#define GUID_DUMP_ENDIAN_MARKER 0x12345678
#define GUID_DUMP_VERSION 0

/*
 * Lets start with some unportable hacks...
 */
union guid_fast
{
	uint8_t g[GUID_SIZE];
	uint32_t d[GUID_SIZE/4];
};

struct guid_cache_entry
{
	struct rbnode rb; /* keep first */
	bool used; /* fill possible hole */
	struct hlist_node node;
	struct guid_entry e;
};

static struct {
	pthread_mutex_t lock;
	int num;
	uint32_t ht_seed;
	struct guid_cache_entry *next_free;
	struct guid_cache_entry *last_search;
	struct rbtree tree;
	struct hlist_head ht[GUID_CACHE_HTSIZE];
	struct guid_cache_entry entrys[GUID_CACHE_SIZE];
} cache;

/*
 * Functs
 */
bool g2_guid_init(void)
{
	FILE *guid_dump;
	uint32_t *signature;
	const char *data_root_dir;
	char *name, *buff;
	size_t name_len = 0;

	if(pthread_mutex_init(&cache.lock, NULL)) {
		logg_errno(LOGF_ERR, "initialising GUID cache lock");
		return false;
	}
	random_bytes_get(&cache.ht_seed, sizeof(cache.ht_seed));

	/* try to read a guid dump */
	if(!server.settings.guid.dump_fname)
		return true;

	if(server.settings.data_root_dir)
		data_root_dir = server.settings.data_root_dir;
	else
		data_root_dir = "./";


	name_len  = strlen(data_root_dir);
	name_len += strlen(server.settings.guid.dump_fname);
	name = alloca(name_len + 2);

	buff = strpcpy(name, data_root_dir);
	*buff++ = '/';
	strcpy(buff, server.settings.guid.dump_fname);

	guid_dump = fopen(name, "rb");
	if(!guid_dump) {
		logg_errno(LOGF_INFO, "couldn't open guid dump");
		return true;
	}

	buff = alloca(500);
	if(str_size(GUID_DUMP_IDENT) != fread(buff, 1, str_size(GUID_DUMP_IDENT), guid_dump)) {
		logg_posd(LOGF_INFO, "couldn't read ident from guid dump \"%s\"\n", name);
		goto out;
	}

	if(strncmp(buff, GUID_DUMP_IDENT, str_size(GUID_DUMP_IDENT))) {
		logg_posd(LOGF_INFO, "\"%s\" has wrong guid dump ident\n", name);
		goto out;
	}

	signature = (uint32_t *) buff;
	if(3 != fread(signature, sizeof(uint32_t), 3, guid_dump)) {
		logg_posd(LOGF_INFO, "couldn't read signature from guid dump \"%s\"\n", name);
		goto out;
	}

	if(signature[0] != GUID_DUMP_ENDIAN_MARKER) {
		logg_posd(LOGF_WARN, "guid dump \"%s\" has wrong %s, don't copy dumps around!\n", name, "endianess");
		goto out;
	}
	if(signature[1] != sizeof(struct guid_entry)) {
		logg_posd(LOGF_WARN, "guid dump \"%s\" has wrong %s, don't copy dumps around!\n", name, "datasize");
		goto out;
	}
	if(signature[2] != GUID_DUMP_VERSION) {
		logg_posd(LOGF_DEBUG, "guid dump \"%s\" has wrong %s, don't copy dumps around!\n", name, "version");
		goto out;
	}

	/* we don't care if we read 0 or 1 or GUID_CACHE_SIZE elements */
	do
	{
		struct guid_entry *e = (void *) buff;
		name_len = fread(e, sizeof(*e), 1, guid_dump);
		if(name_len)
			g2_guid_add(e->guid, &e->na, e->when, e->type);
	} while(name_len);

out:
	fclose(guid_dump);
	return true;
}

void g2_guid_end(void)
{
	FILE *guid_dump;
	uint32_t signature[3];
	const char *data_root_dir;
	char *name, *wptr;
	size_t name_len, i;

	/* try to write a guid dump */
	if(!server.settings.guid.dump_fname)
		return;

	if(server.settings.data_root_dir)
		data_root_dir = server.settings.data_root_dir;
	else
		data_root_dir = "./";

	name_len  = strlen(data_root_dir);
	name_len += strlen(server.settings.guid.dump_fname);
	name = alloca(name_len + 1);

	wptr = strpcpy(name, data_root_dir);
	strcpy(wptr, server.settings.guid.dump_fname);

	guid_dump = fopen(name, "wb");
	if(!guid_dump) {
		logg_errno(LOGF_INFO, "couldn't open guid dump");
		return;
	}

	fputs(GUID_DUMP_IDENT, guid_dump);

	signature[0] = GUID_DUMP_ENDIAN_MARKER;
	signature[1] = sizeof(struct guid_entry);
	signature[2] = GUID_DUMP_VERSION;

	fwrite(signature, sizeof(signature[0]), 3, guid_dump);

	for(i = 0; i < GUID_CACHE_SIZE; i++) {
		if(cache.entrys[i].used)
			fwrite(&cache.entrys[i].e, sizeof(cache.entrys[0].e), 1, guid_dump);
	}
	fclose(guid_dump);
}


static struct guid_cache_entry *guid_cache_entry_alloc(void)
{
	struct guid_cache_entry *e;
	unsigned i;

	if(GUID_CACHE_SIZE <= cache.num)
		return NULL;

	if(cache.next_free)
	{
		e = cache.next_free;
		cache.next_free = NULL;
		if(likely(!e->used))
			goto check_next_free;
	}

	for(i = 0, e = NULL; i < GUID_CACHE_SIZE; i++)
	{
		if(!cache.entrys[i].used) {
			e = &cache.entrys[i];
			break;
		}
	}

	if(e)
	{
check_next_free:
		e->used = true;
		cache.num++;
		if(e < &cache.entrys[GUID_CACHE_SIZE-1]) {
			if(!e[1].used)
				cache.next_free = &e[1];
		}
	}
	return e;
}

static void guid_cache_entry_free(struct guid_cache_entry *e)
{
	memset(e, 0, sizeof(*e));
	if(!cache.next_free)
		cache.next_free = e;
	cache.num--;
}

static uint32_t cache_ht_hash(const union guid_fast *g, enum guid_type gt)
{
	return hthash_4words(g->d[0], g->d[1], g->d[2], g->d[3], cache.ht_seed ^ gt);
}

static struct guid_cache_entry *cache_ht_lookup(const union guid_fast *g, enum guid_type gt, uint32_t h)
{
	struct hlist_node *n;
	struct guid_cache_entry *e;

	hlist_for_each_entry(e, n, &cache.ht[h & GUID_CACHE_HTMASK], node)
	{
		union guid_fast *x = (union guid_fast *)e->e.guid;

		if(g->d[0] != x->d[0])
			continue;
		if(g->d[1] != x->d[1])
			continue;
		if(g->d[2] != x->d[2])
			continue;
		if(g->d[3] != x->d[3])
			continue;
		if(gt != e->e.type)
			continue;
		return e;
	}

	return NULL;
}

static void cache_ht_add(struct guid_cache_entry *e, uint32_t h)
{
	hlist_add_head(&e->node, &cache.ht[h & GUID_CACHE_HTMASK]);
}

static void cache_ht_del(struct guid_cache_entry *e)
{
	hlist_del(&e->node);
}

static inline int rbnode_cache_eq(struct guid_cache_entry *a, struct guid_cache_entry *b)
{
	union guid_fast *ga = (union guid_fast *)a->e.guid;
	union guid_fast *gb = (union guid_fast *)b->e.guid;
	return ga->d[0]   == gb->d[0]   && ga->d[1]   == gb->d[1] &&
	       ga->d[2]   == gb->d[2]   && ga->d[3]   == gb->d[3] &&
	        a->e.type ==  b->e.type &&  a->e.when ==  b->e.when;
}

static inline int rbnode_cache_lt(struct guid_cache_entry *a, struct guid_cache_entry *b)
{
	union guid_fast *ga, *gb;

	/* we want to order by age  foremost*/
	int ret = a->e.when < b->e.when;
	if(ret)
		return ret;
	ret = a->e.when > b->e.when;
	if(ret)
		return !ret;

	ret = a->e.type > b->e.type;
	if(ret)
		return ret;
	ret = a->e.type < b->e.type;
	if(ret)
		return !ret;

	ga = (union guid_fast *)a->e.guid;
	gb = (union guid_fast *)b->e.guid;
	ret = ga->d[0] < gb->d[0];
	if(ret)
		return ret;
	ret = ga->d[0] > gb->d[0];
	if(ret)
		return !ret;
	ret = ga->d[1] < gb->d[1];
	if(ret)
		return ret;
	ret = ga->d[1] > gb->d[1];
	if(ret)
		return !ret;
	ret = ga->d[2] < gb->d[2];
	if(ret)
		return ret;
	ret = ga->d[2] > gb->d[2];
	if(ret)
		return !ret;
	ret = ga->d[3] < gb->d[3];
	if(ret)
		return ret;
	ret = ga->d[3] > gb->d[3];
	if(ret)
		return !ret;

	ret = a->e.na.s_fam < b->e.na.s_fam;
	if(ret)
		return ret;
	ret = a->e.na.s_fam > b->e.na.s_fam;
	if(ret)
		return !ret;

// TODO: when IPv6 is common, change it
	if(likely(AF_INET == a->e.na.s_fam))
	{
		ret = a->e.na.in.sin_addr.s_addr < b->e.na.in.sin_addr.s_addr;
		if(ret)
			return ret;
		ret = a->e.na.in.sin_addr.s_addr > b->e.na.in.sin_addr.s_addr;
		if(ret)
			return !ret;
		return a->e.na.in.sin_port < b->e.na.in.sin_port;
	}
	else
	{
		ret =  a->e.na.in6.sin6_addr.s6_addr32[0] < b->e.na.in6.sin6_addr.s6_addr32[0] ||
		       a->e.na.in6.sin6_addr.s6_addr32[1] < b->e.na.in6.sin6_addr.s6_addr32[1] ||
		       a->e.na.in6.sin6_addr.s6_addr32[2] < b->e.na.in6.sin6_addr.s6_addr32[2] ||
		       a->e.na.in6.sin6_addr.s6_addr32[3] < b->e.na.in6.sin6_addr.s6_addr32[3];
		if(ret)
			return ret;
		ret =  a->e.na.in6.sin6_addr.s6_addr32[0] > b->e.na.in6.sin6_addr.s6_addr32[0] ||
		       a->e.na.in6.sin6_addr.s6_addr32[1] > b->e.na.in6.sin6_addr.s6_addr32[1] ||
		       a->e.na.in6.sin6_addr.s6_addr32[2] > b->e.na.in6.sin6_addr.s6_addr32[2] ||
		       a->e.na.in6.sin6_addr.s6_addr32[3] > b->e.na.in6.sin6_addr.s6_addr32[3];
		if(ret)
			return !ret;
		return a->e.na.in6.sin6_port < b->e.na.in6.sin6_port;
	}
}

static inline void rbnode_cache_cp(struct guid_cache_entry *dest, struct guid_cache_entry *src)
{
	/*
	 * The whole trick here is:
	 * source is the physical entry which gets removed from the rb-tree,
	 * but still it may not be the logical entry which should get removed,
	 * so maybe the data from source has to be copied to dest.
	 * So we either have to remove src from the Hashtable, or
	 * we have to remove src & dest, move the data from src to dest,
	 * and finally reinsert dest into the hashtable.
	 */
	if(dest != src)
	{
/*		struct hlist_node *prev = (struct hlist_node *)src->node.pprev;*/

		__hlist_del(&src->node);
		INIT_HLIST_NODE(&src->node);
		__hlist_del(&dest->node);
		memcpy(&dest->used, &src->used,
		       sizeof(struct guid_cache_entry) - offsetof(struct guid_cache_entry, used));
		/* reinsert dest, src is now "removed" */
/*		hlist_add_after(prev, &dest->node);*/
// TODO: fix reinsertion
		/*
		 * we can not cheaply reinsert dest, somehow we are fucking
		 * up the hashtable (or more precise: the linked list).
		 * So pay CPU-cycles for our stupidness: cleanly reinsert
		 * from the start
		 */
		cache_ht_add(dest, cache_ht_hash((union guid_fast *)dest->e.guid, dest->e.type));
	}
	else
		cache_ht_del(src);
}

RBTREE_MAKE_FUNCS(cache, struct guid_cache_entry, rb);

bool g2_guid_lookup(const uint8_t guid_a[GUID_SIZE], enum guid_type gt, union combo_addr *addr)
{
	struct guid_cache_entry *e;
	uint32_t h;
	bool ret_val = false;
#ifndef UNALIGNED_OK
	union guid_fast guid_t;
	const union guid_fast *guid = &guid_t;
	memcpy(&guid_t, guid_a, GUID_SIZE);
#else
	const union guid_fast *guid = (const union guid_fast *)guid_a;
#endif

	h = cache_ht_hash(guid, gt);
	/*
	 * Prevent the compiler from moving the calcs
	 * into the critical section
	 */
	barrier();

	if(unlikely(pthread_mutex_lock(&cache.lock)))
		return false;

	/* already in the cache? */
	e = cache_ht_lookup(guid, gt, h);
	if(!e)
		goto out_unlock;

	if(addr)
		*addr = e->e.na;
	ret_val = true;

out_unlock:
	if(unlikely(pthread_mutex_unlock(&cache.lock)))
		diedie("gnarf, GUID cache lock stuck, bye!");

	return ret_val;
}

bool g2_guid_add(const uint8_t guid_a[GUID_SIZE], const union combo_addr *addr, time_t when, enum guid_type gt)
{
	struct guid_cache_entry *e, *t;
	uint32_t h;
	bool ret_val = false;
#ifndef UNALIGNED_OK
	union guid_fast guid_t;
	const union guid_fast *guid = &guid_t;
	memcpy(&guid_t, guid_a, GUID_SIZE);
#else
	const union guid_fast *guid = (const union guid_fast *)guid_a;
#endif

	/* check for bogus addresses */
	if(unlikely(!combo_addr_is_public(addr))) {
		logg_develd("addr %pI is privat, not added\n", addr);
		return ret_val;
	}
//TODO: check for own guid
	logg_develd("adding: %p#G -> %p#I, %u\n", guid, addr, (unsigned)when);

	h = cache_ht_hash(guid, gt);

	/*
	 * Prevent the compiler from moving the calcs
	 * into the critical section
	 */
	barrier();

	if(unlikely(pthread_mutex_lock(&cache.lock)))
		return ret_val;

	/* already in the cache? */
	e = cache_ht_lookup(guid, gt, h);
	if(e)
	{
		ret_val = true;
		/* entry newer? */
		if(e->e.when >= when)
			goto out_unlock;
		t = rbtree_cache_remove(&cache.tree, e);
		if(t)
			e = t;
		else {
			logg_devel("remove failed\n");
			goto life_tree_error; /* something went wrong... */
		}
		/*
		 * The (physical) entry removed is maybe not the
		 * one we asked for, it is just an entry. We have to
		 * reinit it to reinsert it.
		 */
// TODO: what to do on type change?
		e->e.type = gt;
	}
	else
	{
		t = rbtree_cache_lowest(&cache.tree);
		if(likely(t) &&
		   t->e.when < when &&
		   t->e.type > gt &&
		   GUID_CACHE_SIZE <= cache.num)
		{
			logg_devel_old("found older entry\n");
			if(!(t = rbtree_cache_remove(&cache.tree, t)))
				goto out_unlock; /* the tree is amiss? */
			e = t;
		}
		else
			e = guid_cache_entry_alloc();
		if(!e)
			goto out_unlock;

		e->e.type = gt;
	}
	memcpy(e->e.guid, guid, sizeof(e->e.guid));
	e->e.na = *addr;
	e->e.when = when;

	if(!rbtree_cache_insert(&cache.tree, e)) {
		logg_devel("insert failed\n");
life_tree_error:
		guid_cache_entry_free(e);
	}
	else
		cache_ht_add(e, h);

out_unlock:
	if(unlikely(pthread_mutex_unlock(&cache.lock)))
		diedie("gnarf, GUID cache lock stuck, bye!");
	return ret_val;
}

/*@unused@*/
static char const rcsid_guid[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
