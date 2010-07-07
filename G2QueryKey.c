/*
 * G2QueryKey.c
 * Query Key syndication
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* includes */
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
/* own */
#include "lib/other.h"
#define _G2QUERYKEY_C
#include "G2QueryKey.h"
#include "G2MainServer.h"
#include "lib/log_facility.h"
#include "lib/hthash.h"
#include "lib/hlist.h"
#include "lib/rbtree.h"
#include "lib/combo_addr.h"
#include "lib/ansi_prng.h"
#include "lib/guid.h"

/*
 * The G2 protocol implements a "lightweight" walker-like search
 * algorithm.
 * The idea is that a searcher sends querys to hubs (what we are)
 * for things it searches. Searches are not forwarded/broadcastet
 * most of the time, instead the searcher gets some "look there"
 * hints he has to itterate on it's own.
 * This should prevent the network from being flooded with "wandering"
 * search requests (they "die off" if the searches does not invest
 * energy in them).
 * Searches can be submited without a connection by UDP, which
 * makes the whole process somewhat "lightweight".
 *
 * But to prevent a traffic-amplification attacks due to UDPs
 * connectionless nature (easly spoofable), search querys need
 * a kind of association before hand (origin of sender proof).
 *
 * This is what query keys are used for.
 * A query is only valid if a valid query key for its source is
 * supplied.
 *
 * A searcher has to get a query key before he can submit querys.
 * This can be done by UDP, but generly should not result in a
 * traffic amplification. (It is still possible to reflect traffic
 * this way, which can only be "stoped" by a qeury key request
 * throttle for a source address)
 *
 * Query keys are only valid for a particular source. This is
 * typically achived by connecting it to the source address.
 * Query keys "dekay" after a timeout, which is "long" (which is
 * exactly how long?).
 *
 * One possability would be to hand out "uniqe" IDs, store them,
 * match them, age them, destroy them.
 * Another hashtable foo, sounds like fun...
 *
 * Instead we could go the cryptographic route.
 * OK, sounds elaborated, but its quite easy.
 * We have a set of input parameters: source address, relay address,
 * port.
 * We could hash that and hand it out as query key. This operation
 * can be repeated every time a query key needs to be validated.
 *
 * Problem: An attacker could also do the hash.
 * Solution: We add a secret (only known to us) salt to the hash.
 * Now an attacker also needs the secret salt to generate a query
 * key.
 *
 * Problem: Given enough time, an attacker could guess the salt
 *          (or do a cryptographic attack to get to the salt).
 * Solution: We create a ringbuffer of salts whoms actual index gets
 *           resalted and forwared from time to time. The querykey
 *           contains a hint to which ringbuffer "time slot" he belongs.
 * This is very elegant, because now we kill two birds with one stone.
 * We get fresh salts every now and then, plus we age the query keys
 * automagicly. When the orginal time slot a query key belongs to gets
 * new salt, he does not match any longer and is dekayed.
 *
 * This is the basic concept.
 *
 * Unfortunatly, query keys are 32 bit. So the key space is small.
 * We further reduce it by the time slot hint (for example 3 bit
 * for 8 time slots). And we map a lot of info into it.
 * They could have at least choosen 64 bit...
 *
 * Thats why we choose a simple hash table hash, not a secure hash.
 * Because 32 Bit are not such a large key space even distribution
 * is more welcome. And we want validation to be cheap. (Secure
 * hashes are expensive and yield more bits we could ever use/only
 * use the wrong way (is truncation ok?))
 *
 * This has its downside: The hash may be "simply" reversed to get
 * to the salt and then generate valid query keys at free will.
 * Time for the next solution: An attacker would need several
 * samples of (known) input <-> hash to get to the salt (due to
 * collision problem, we map more info on a smaller space). This
 * needs to be "foreign input" (man-in-the-middle, controled zombie),
 * because a request for its own "credentials" would lead to the same
 * query key for this time slot.
 * If we use different salts within one time slot choosen by
 * hash(addr, time_slot_master_salt), the attacker would need more
 * input to guess one salt, because he can not identfy for which
 * salt a key is valid for (he has to attack all salts and the master
 * salt).
 *
 * If everthing fails we can still use a secure hash, or something
 * else, it should be invisible for our clients.
 *
 * Unfortunatly this is all fucked up by the greater internet evils:
 * NAT and UDP-Firewalls.
 *
 * So there is an additional search (key) proxy mode which is to
 * be implemented by hubs (us...). And boy, this sucks big time...
 */

/* 8 == 3 bit of key */
#define TIME_SLOT_COUNT 8
#define TIME_SLOT_COUNT_MASK (~(TIME_SLOT_COUNT - 1UL))
/* 8 slots at 1hour per slot == 8 hours key dekay */
#define TIME_SLOT_SECS (60 * 60)
#define TIME_SLOT_ELEM 64

/*
 * Query Key Cache
 */
#define QK_CACHE_SHIFT 12
#define QK_CACHE_SIZE (1 << QK_CACHE_SHIFT)
#define QK_CACHE_HTSIZE (QK_CACHE_SIZE/8)
#define QK_CACHE_HTMASK (QK_CACHE_HTSIZE-1)

 /* Types */
struct qk_entry
{
	union combo_addr na;
	uint32_t qk;
	time_t when;
};

struct qk_cache_entry
{
	struct rb_node rb;
	struct hlist_node node;
	struct qk_entry e;
};

struct g2_qk_salts
{
	uint32_t salts[TIME_SLOT_COUNT][TIME_SLOT_ELEM + 1][2];
	volatile unsigned act_salt;
	time_t last_update;
} GCC_ATTR_ALIGNED(16);

/* Vars */
static struct g2_qk_salts g2_qk_s;
static struct
{
	pthread_mutex_t lock;
	uint32_t ht_seed;
	struct hlist_head free_list;
	struct rb_root tree;
	struct hlist_head ht[QK_CACHE_HTSIZE];
	struct qk_cache_entry entrys[QK_CACHE_SIZE];
} cache;

/* Protos */
	/* You better not kill this proto, or it wount work ;) */
static void qk_init(void) GCC_ATTR_CONSTRUCT;
static void qk_deinit(void) GCC_ATTR_DESTRUCT;

/* Funcs */
static noinline void check_salt_vals(unsigned j)
{
	unsigned char r[16], *slt;
	unsigned r_i = sizeof(r);
	unsigned i;

	slt = (unsigned char *)g2_qk_s.salts[j];
	i   = sizeof(g2_qk_s.salts[0]);
	/* make sure no unpleasant values are in the array */
	for(; i--; slt++)
	{
		/*
		 * For a nice play on this we could check we have enough
		 * ham weight (popcount) on the full scale 32 bit word.
		 * Mainly we want to exclude 0 because it has no entropy
		 * at the mixing level.
		 * On the fullscale 32 Bit salt 0 would be deadly, 1 is
		 * also not welcome.
		 * With looking for 0 at a byte level we are way ahead,
		 * preventing a lot of "odd" values, on the other side
		 * we are excluding a lot of values...
		 * Pseudo crypto, whatever you do, you are fucking it
		 * up.
		 * At least this is a feedback into our PRNG. For every
		 * "burst" of 16 zeros in a salt slot it generates (which
		 * you do not see afterwards, thanks to this ;) its
		 * internal state advances a tick, making it even harder
		 * to guess its internal state, because it relys on
		 * previous internal state.
		 */
		while(unlikely(0 == *slt))
		{
			if(r_i >= sizeof(r)) {
				random_bytes_get(r, sizeof(r));
				r_i %=  sizeof(r);
			}
			*slt = r[r_i++];
		}
	}
}

void g2_qk_init(void)
{
	unsigned i;

	random_bytes_get(g2_qk_s.salts, sizeof(g2_qk_s.salts));
	for(i = 0; i < anum(g2_qk_s.salts); i++)
		check_salt_vals(i);
	random_bytes_get(&cache.ht_seed, sizeof(cache.ht_seed));
	random_bytes_get(&i, sizeof(g2_qk_s.act_salt));
	g2_qk_s.act_salt = i % TIME_SLOT_COUNT;
	g2_qk_s.last_update = time(NULL);
	/* also init the guid generator */
	guid_init();
}

void g2_qk_tick(void)
{
	unsigned n_salt, t;
	long t_diff;

	t_diff = local_time_now - g2_qk_s.last_update;
	/*
	 * on local_time_now wrap:
	 * unsigned to 0:
	 *	sudden large diff -> action -> short period
	 * singed to neg TIME_MAX:
	 *	sudden large diff -> action -> short period
	 */
	t_diff = t_diff >= 0 ? t_diff : -t_diff;
	if(t_diff < TIME_SLOT_SECS)
		return;

	n_salt = (g2_qk_s.act_salt + 1) % TIME_SLOT_COUNT;

	random_bytes_get(g2_qk_s.salts[n_salt], sizeof(g2_qk_s.salts[n_salt]));
	check_salt_vals(n_salt);
	/*
	 * reseed the libc random number generator every tick,
	 * for the greater good of the whole server
	 */
	random_bytes_get(&t, sizeof(t));
	srand(t);

	barrier();
	g2_qk_s.act_salt = n_salt;
	g2_qk_s.last_update = local_time_now;
	/*
	 * when we generate a new set of salts,
	 * time to re-key the guid generator
	 */
	guid_tick();
}

static uint32_t addr_hash_generate(const union combo_addr *source, unsigned salt2use)
{
	uint32_t h, s1, s2, w[4]; // 8
	unsigned len = 0;

	len += combo_addr_lin_ip(&w[len], source);
//	len += combo_addr_lin_ip(&w[len], host);

	s1 = g2_qk_s.salts[salt2use][TIME_SLOT_ELEM][0];
	s2 = g2_qk_s.salts[salt2use][TIME_SLOT_ELEM][1];
	h = hthash32_mod(w, len, s1, s2);
	/* move entropy to low nibble */
	h ^= h >> 16;
	h ^= h >>  8;
	h ^= h >>  4;
	h %= TIME_SLOT_ELEM;
	s1 = g2_qk_s.salts[salt2use][h][0];
	s2 = g2_qk_s.salts[salt2use][h][1];
	h = hthash32_mod(w, len, s1, s2);

	return h;
}

uint32_t g2_qk_generate(const union combo_addr *source)
{
	unsigned act_salt = g2_qk_s.act_salt;
	uint32_t h;

	barrier();
	h = addr_hash_generate(source, act_salt);
	return (h & TIME_SLOT_COUNT_MASK) | act_salt;
}

bool g2_qk_check(const union combo_addr *source, uint32_t key)
{
	uint32_t h;

	h = addr_hash_generate(source, key & ~TIME_SLOT_COUNT_MASK);
	return (h & TIME_SLOT_COUNT_MASK) == (key & TIME_SLOT_COUNT_MASK);
}

/********************* Query Key Cache funcs ********************/

static void qk_init(void)
{
	size_t i;

	if(pthread_mutex_init(&cache.lock, NULL))
		diedie("initialising qk cache lock");

	/* shuffle all entrys in the free list */
	for(i = 0; i < QK_CACHE_SIZE; i++)
		hlist_add_head(&cache.entrys[i].node, &cache.free_list);
}

static void qk_deinit(void)
{
	pthread_mutex_destroy(&cache.lock);
}

static struct qk_cache_entry *qk_cache_entry_alloc(void)
{
	struct qk_cache_entry *e;
	struct hlist_node *n;

	if(hlist_empty(&cache.free_list))
		return NULL;

	n = cache.free_list.first;
	e = hlist_entry(n, struct qk_cache_entry, node);
	hlist_del(&e->node);
	INIT_HLIST_NODE(&e->node);
	RB_CLEAR_NODE(&e->rb);
	return e;
}

static uint32_t cache_ht_hash(const union combo_addr *addr)
{
	return combo_addr_hash(addr, cache.ht_seed);
}

static void qk_cache_entry_free(struct qk_cache_entry *e)
{
	memset(e, 0, sizeof(*e));
	hlist_add_head(&e->node, &cache.free_list);
}

static struct qk_cache_entry *cache_ht_lookup(const union combo_addr *addr, uint32_t h)
{
	struct hlist_node *n;
	struct qk_cache_entry *e;

// TODO: check for mapped ip addr?
// TODO: when IPv6 is common, change it
// TODO: leave out port?
	if(likely(addr->s.fam == AF_INET))
	{
		hlist_for_each_entry(e, n, &cache.ht[h & QK_CACHE_HTMASK], node)
		{
			if(e->e.na.s.fam != AF_INET)
				continue;
			if(e->e.na.in.sin_addr.s_addr == addr->in.sin_addr.s_addr &&
			   e->e.na.in.sin_port == addr->in.sin_port)
				return e;
		}
	}
	else
	{
		hlist_for_each_entry(e, n, &cache.ht[h & QK_CACHE_HTMASK], node)
		{
			if(e->e.na.s.fam != AF_INET6)
				continue;
			if(IN6_ARE_ADDR_EQUAL(&e->e.na.in6.sin6_addr, &addr->in6.sin6_addr) &&
			   e->e.na.in.sin_port == addr->in.sin_port)
				return e;
		}
	}

	return NULL;
}

static void cache_ht_add(struct qk_cache_entry *e, uint32_t h)
{
	hlist_add_head(&e->node, &cache.ht[h & QK_CACHE_HTMASK]);
}

static void cache_ht_del(struct qk_cache_entry *e)
{
	hlist_del(&e->node);
}

static int qk_entry_cmp(struct qk_cache_entry *a, struct qk_cache_entry *b)
{
	int ret = (long)a->e.when - (long)b->e.when;
	if(ret)
		return ret;
	if((ret = (int)a->e.na.s.fam - (int)b->e.na.s.fam))
		return ret;
	if(likely(AF_INET == a->e.na.s.fam))
	{
		if((ret = (long)a->e.na.in.sin_addr.s_addr - (long)b->e.na.in.sin_addr.s_addr))
			return ret;
		return (int)a->e.na.in.sin_port - (int)b->e.na.in.sin_port;
	}
	else
	{
		if((ret = (long)a->e.na.in6.sin6_addr.s6_addr32[0] - (long)b->e.na.in6.sin6_addr.s6_addr32[0]))
			return ret;
		if((ret = (long)a->e.na.in6.sin6_addr.s6_addr32[1] - (long)b->e.na.in6.sin6_addr.s6_addr32[1]))
			return ret;
		if((ret = (long)a->e.na.in6.sin6_addr.s6_addr32[2] - (long)b->e.na.in6.sin6_addr.s6_addr32[2]))
			return ret;
		if((ret = (long)a->e.na.in6.sin6_addr.s6_addr32[3] - (long)b->e.na.in6.sin6_addr.s6_addr32[3]))
			return ret;
		return (int)a->e.na.in6.sin6_port - (int)b->e.na.in6.sin6_port;
	}
}

static noinline bool qk_rb_cache_insert(struct qk_cache_entry *e)
{
	struct rb_node **p = &cache.tree.rb_node;
	struct rb_node *parent = NULL;

	while(*p)
	{
		struct qk_cache_entry *n = rb_entry(*p, struct qk_cache_entry, rb);
		int result = qk_entry_cmp(e, n);

		parent = *p;
		if(result < 0)
			p = &((*p)->rb_left);
		else if(result > 0)
			p = &((*p)->rb_right);
		else
			return false;
	}
	rb_link_node(&e->rb, parent, p);
	rb_insert_color(&e->rb, &cache.tree);

	return true;
}

static noinline bool qk_rb_cache_remove(struct qk_cache_entry *e)
{
	struct rb_node *n = &e->rb;

	if(RB_EMPTY_NODE(n))
		return false;

	rb_erase(n, &cache.tree);
	RB_CLEAR_NODE(n);
	cache_ht_del(e);

	return true;
}

static struct qk_cache_entry *qk_cache_last(void)
{
	struct rb_node *n = rb_last(&cache.tree);
	if(!n)
		return NULL;
	return rb_entry(n, struct qk_cache_entry, rb);
}

void g2_qk_add(uint32_t qk, const union combo_addr *addr)
{
	struct qk_cache_entry *e;
	uint32_t h;
	time_t when;

	if(unlikely(!combo_addr_is_public(addr))) {
		logg_develd_old("addr %pI is private, not added\n", addr);
		return;
	}
	h = cache_ht_hash(addr);
	when = local_time_now;

	/*
	 * Prevent the compiler from moving the calcs into
	 * the critical section
	 */
	barrier();

	if(unlikely(pthread_mutex_lock(&cache.lock)))
		return;
	/* already in the cache? */
	e = cache_ht_lookup(addr, h);
	if(e)
	{
		/* entry newer? */
		if(e->e.when >= when)
			goto out_unlock;
		if(!qk_rb_cache_remove(e)) {
			logg_devel("remove failed\n");
			goto life_tree_error; /* something went wrong... */
		}
	}
	else
	{
		e = qk_cache_last();
		if(likely(e) &&
		   e->e.when < when &&
		   hlist_empty(&cache.free_list))
		{
			logg_devel_old("found older entry\n");
			if(!qk_rb_cache_remove(e))
				goto out_unlock; /* the tree is amiss? */
		}
		else
			e = qk_cache_entry_alloc();
		if(!e)
			goto out_unlock;

		e->e.na = *addr;
	}
	e->e.qk = qk;
	e->e.when = when;

	if(!qk_rb_cache_insert(e)) {
		logg_devel("insert failed\n");
life_tree_error:
		qk_cache_entry_free(e);
	}
	else
		cache_ht_add(e, h);

out_unlock:
	if(unlikely(pthread_mutex_unlock(&cache.lock)))
		diedie("ahhhh, QK cache lock stuck, bye!");
}

bool g2_qk_lookup(uint32_t *qk, const union combo_addr *addr)
{
	struct qk_cache_entry *e;
	uint32_t h;
	bool ret_val;

	if(unlikely(!combo_addr_is_public(addr)))
		return false;
	h = cache_ht_hash(addr);
	ret_val = false;

	/*
	 * Prevent the compiler from moving the calcs into
	 * the critical section
	 */
	barrier();

	if(unlikely(pthread_mutex_lock(&cache.lock)))
		return false;
	/* already in the cache? */
	e = cache_ht_lookup(addr, h);
	if(e) {
		if(qk)
			*qk = e->e.qk;
		ret_val = true;
	}
	if(unlikely(pthread_mutex_unlock(&cache.lock)))
		diedie("ahhhh, QK cache lock stuck, bye!");
	return ret_val;
}

/*@unused@*/
static char const rcsid_qk[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
