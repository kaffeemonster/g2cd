/*
 * G2ConRegistry.c
 * Central G2Connection registry
 *
 * Copyright (c) 2008-2012 Jan Seiffert
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
#include <stdbool.h>
#include <time.h>
#include "lib/my_pthread.h"
#include <unistd.h>
#ifdef DRD_ME
# include <valgrind/drd.h>
#endif
/* own */
#define _G2CONREGISTRY_C
#include "G2ConRegistry.h"
#include "G2QHT.h"
#include "G2Connection.h"
#include "lib/log_facility.h"
#include "lib/hlist.h"
#include "lib/hthash.h"
#include "lib/combo_addr.h"
#include "lib/my_bitops.h"
#include "lib/hzp.h"

/* WARNING, long prosa ahead...
 * Reading this will not make you a better whatever, surely waste
 * your time, but at least you hopefully know why things look the
 * way they look afterwards.
 */
/* "Make my day!"
 * ...
 * After scratching my head for years how to solve this, finaly
 * a solution came to me one morning after a sleepless night.
 * But one thing after the another, lets start at the beginning:
 *
 * One main point for the g2cd design is to strive for scalability.
 * What's a "server only" implementation good for if managing
 * connections, lots of connections, is not its primary goal.
 *
 * Till now i tried to achieve this with breaking up work at high-
 * level entities and not build miniscully parallelism (a multi-
 * threaded copy loop, wow, that will save the day...). This has
 * to be supported by minimal locking dependencies to not introduce
 * bottlenecks (individual parts can "do their thing" with minimal
 * friction to make forward progress).
 *
 * In g2cd's incomplete state this has worked quite well so far.
 * A little bit thread local storage here, some atomic ops there,
 * HZP (RCU is a PITA from userspace...), all complex, complacting
 * the build, raising the dependencies, but worth it.
 *
 * Unfortunatly, the G2Connection registry is the place where this
 * scheme could break down.
 * Now you may ask, why build one? Till now you didn't need one.
 * Yeah, and till now g2cd is incomplete...
 * And hey, why this should be a scalability problem?
 *
 * The G2 protocol "demands" a central connection registry for a
 * server for 4 purposes:
 * 1) Access to all connections QHT to answer search querys.
 * 2) Mapping UDP packets to existing connections.
 * 3) Protocol implied broadcasts (n other nodes).
 * 4) Prevent multiple connections from the same client.
 *
 * Suddenly nearly all our "individual parts" are stomping on each
 * others toes...
 * Thats the problem!
 *
 * For 2) and 4) we need to search connections by IP.
 * For 1) we need to search for a bit in the QHTs.
 * For 3) we need some kind of iterator (which also comes in handy
 * for debugging: who is connected)
 *
 * With the scalability thing in mind now the question is:
 * Which data structure to use?
 *
 * You can iterate nearly everything, so this is not a concern.
 * (if you keep the count low or only iterate a few nodes)
 *
 * The IP as the search key can be served by a tree or a hash(map).
 *
 * This leaves us with the QHTs.
 * QHTs, being the spine of the whole content lookup thing, are a
 * quite efficient thing on their own, until you want to check lots
 * of them (as in a server...). But again QHTs for the rescue:
 * Since you can combine QHTs to build "super QHTs" and a
 * "master QHT" you can apply "divide & conquer" tactics to your
 * searches (From the find-fast-"Am i responsible for this"-POV).
 *
 * So this all screams out loud for a tree. The key being the IP,
 * building combined QHTs on the tree levels.
 * All Fine. Yippie!
 * No!
 * No?
 * No...
 *
 * We didn't took the multithreading into account.
 * Traversing trees makes locking a horror (QHT "sidewards" search,
 * both leaves having a matching bit, or up and down, brrr), guarding
 * against insert and esp. removal (nodes *will* disconnect...).
 * But thats not all, the intermediate QHTs want to be maintained.
 *
 * Things go downhill from here, esp. in our many reader, modest
 * writer situation. (many search request (read) and many UDP
 * packets (read), but modest fluctuation on connected nodes
 * (write) and modest to high updates to the QHTs (write)).
 *
 * To make things more uneasy, the IP as the key will degrade your
 * tree to something fucked up if you do not apply any rebalancing,
 * (the QHT as a key is unusable and the IP does not have this
 * stacking "property") being it forced/sheduled or "self balacing".
 * But rebalancing makes locking even harder, and we need to
 * regenerate the intermediate QHTs, again with proper locking.
 * So much for "self balancing trees" ;-(
 * I smell writers starving readers or never finishing "incremental
 * writers", deadlocks, "use after free", data corruption.
 *
 * Maybe i'm simply to stupid for this, a little untalented
 * scriptkiddy and my lack of a real CS-degree is the reason
 * for my inability to solve this instead of breaking out in
 * tears the more i think about the interlocking/data dependencies.
 * This kept at least me sometimes insomniac the last years...
 * (and prolonged multithreaded thinking makes you mental unhealthy)
 *
 * So lets attack this from a more simple vector.
 * We can put the QHTs aside for a moment. What ever we choose, we can
 * still build one master QHT which at least should give us this
 * "one-look no-not-for-us" thing. Divide and conquer aproaches are an
 * additional optimazition.
 * So we can choose a simple hashtable which brings us 2) and 4) (with
 * solvable locking), can provide 3) (also solvable locking), and 1) on
 * top of 3) (not nice, but hey, works).
 *
 * If we then step back a little bit how a traditional genric hashtable
 * looks, where we have the n table slots (hash buckets) keying of the
 * hash chains (linked lists) to handle hash collision, who says a hash
 * bucket has to key of a hash chain and not another hashtable?
 * This way we can generate a "tree'ed" hashtable, giving us again
 * the possibility to generate a Master QHT, per-bucket QHT, with still
 * sane locking.
 *
 * Lets make a little picture of a 3 level tree'ed hashtable with 16
 * buckets per level (4 bit) giving 12 bit hash total:
 *
 * h(IP) - MQHT
 *       |
 *       v
 * [  b0 - BQHT ]
 * [  b1 - BQHT ]
 *     ...
 * [ b15 - BQHT ] -> [ b0 - BQHT ], [ b1 - BQHT ], ... [ b15 - BQHT]
 *                        \
 *                         v
 *                    [ b0 - BQHT ]
 *                      [ b1 - BQHT ] -> (Cn - QHT) -> (Cn+1 - QHT)
 *                        ...
 *                          [ b15 - BQHT ]
 *
 * The table structure is static and doesn't need any locking. The final
 * bucket is a contention point, but should deliver decent performance,
 * since the load is spread to ex. 1024 buckets.
 * QHT update can be handled laziely, by a bucket-dirty flag. You ask why
 * this is suddenly possible? Because in such a fixed tree QHTs may be
 * outdated (yes yes, can be seen as wrong), but never totaly wrong, so
 * false positives (scanning even if client is not avail. any more)
 * should be rare and false negetives (requested info avail, but not
 * found) a short transient failure cheaply be rectified shortly later.
 *
 * NB: call this premature optimization, and you know what:
 * I'm a big fan of premature optimization.
 * Yupp, it's hindering and "not worth the time", unfortunatly, it's
 * not all black and white. We are all doing it all the time.
 * Taking it literetly, you would smash together the most simple
 * working solution and then mesure where to optimize. Which sometimes
 * means a little change, sometimes heavy (error introducing) refactoring.
 * Most of the time someone aplies a first round of common sense while
 * writing and chooses algorithms known from a wise textbook (good libs
 * making it sometimes a no-brainer, so why not do it).
 * But "throwing" multithreading at a program as an afterthought is
 * in my opinion useless. Data dependencies have to be taken into
 * account and suddenly what seemed to be simple and good after a first
 * run over the code is a bottleneck. So you need another run over it,
 * propably more painful then the first, with still unknown success and
 * then facing the mostly feared "multithreading bugs".
 * Spending some brain beforehand can be beneficial to prevent you from
 * rearchitekture the COMPLETE program flow, burning lot of time and
 * fucking it all up IMHO.
 * And funily a "microoptimization" like atomic instructions open up
 * the posibility to a hole new set of algorithms.
 * Yes, it bothers me i could not make elaborate mesurement before making
 * decisions, esp. since the whole server code is not in a generaly
 * working state (haha), but still i think i'm on the right track.
 * At least you still learn a lot when you feel a knot in your head
 * and make no forward progress on other duties. Finaly you can use this
 * knowledge when every cycle counts (uC), and can suddenly give input
 * to the design decissions of others...
 *
 */

#define LEVEL_COUNT CONREG_LEVEL_COUNT
#define LEVEL_SHIFT 2
#define LEVEL_SIZE (1 << LEVEL_SHIFT)
#define LEVEL_MASK (LEVEL_SIZE - 1)
#define CHAIN_FILL_FACTOR 8
#define TOTAL_SIZE (1 << (LEVEL_SHIFT * LEVEL_COUNT))
#define UPDATE_INTERVAL (60)

 /* Types */
struct g2_ht_chain
{
	struct hlist_head list;
	struct qhtable   *qht;
	pthread_rwlock_t  lock;
	bool dirty;
};

struct g2_ht_bucket
{
	struct qhtable *qht;
	bool dirty;
	union
	{
		struct g2_ht_bucket *b[LEVEL_SIZE];
		struct g2_ht_chain  *c[LEVEL_SIZE];
	} d;
};

/* Vars */
static struct g2_ht_bucket *raw_bucket_storage, ht_root;
static struct g2_ht_chain *raw_chain_storage;
static struct list_head neighbour_hubs;
static pthread_rwlock_t neighbour_hubs_lock;
static uint32_t ht_seed;

/* Funcs */
static noinline __init struct g2_ht_bucket *init_alloc_bucket(struct g2_ht_bucket *fill, struct g2_ht_bucket *from, struct g2_ht_chain **from_c, unsigned level)
{
	unsigned i = 0;

	if(level < (LEVEL_COUNT-1))
	{
		for(; i < LEVEL_SIZE; i++)
		{
/*			logg_develd_old("%p, %p, %p, %p, %.*s%u\n", fill, from, raw_bucket_storage, raw_bucket_storage + count_b, level, "\t\t\t\t\t\t", i); */
			from->qht = NULL;
			from->dirty = false;
#ifdef DRD_ME
			DRD_IGNORE_VAR(from->dirty);
#endif
			fill->d.b[i] = from;
			from = init_alloc_bucket(from, from + 1, from_c, level + 1);
		}
	}
	else
	{
		struct g2_ht_chain *f_c = *from_c;

		for(; i < LEVEL_SIZE; i++, f_c++)
		{
			INIT_HLIST_HEAD(&f_c->list);
			f_c->qht = NULL;
			pthread_rwlock_init(&f_c->lock, NULL);
			f_c->dirty = false;
#ifdef DRD_ME
			DRD_IGNORE_VAR(f_c->dirty);
#endif
			fill->d.c[i] = f_c;
		}
		*from_c = f_c;
	}
	return from;
}

void __init g2_conreg_init(void)
{
	size_t count_b = 0, count_c = 0, i = 0;
	struct g2_ht_chain *tc;

	count_c = server.settings.max_connection_sum / CHAIN_FILL_FACTOR;
	count_c += count_c == 0; /* at least one */
	count_c = roundup_power_of_2(count_c);
	/* take the int sqrt */
	count_c  = flsst(count_c) - 1;
/*	count_c  = DIV_ROUNDUP(count_c, LEVEL_SHIFT); */
	count_c  = (count_c / LEVEL_SHIFT) + (count_c % LEVEL_SHIFT >= LEVEL_SHIFT / 2);
	count_c += count_c == 0; /* at least one level */
	/* make sure our hash has enough bits */
	if(count_c * LEVEL_SHIFT > sizeof(uint32_t) * BITS_PER_CHAR)
		count_c = (sizeof(uint32_t) * BITS_PER_CHAR) / LEVEL_SHIFT;
	g2_conreg_level_count = count_c;

	INIT_LIST_HEAD(&neighbour_hubs);
	pthread_rwlock_init(&neighbour_hubs_lock, NULL);

	for(; i < (LEVEL_COUNT-1); i++)
		count_b += 1 << (LEVEL_SHIFT * (i + 1));
	count_c = TOTAL_SIZE;
	raw_bucket_storage = malloc(count_b * sizeof(struct g2_ht_bucket));
	raw_chain_storage = malloc(count_c * sizeof(struct g2_ht_chain));
	if(!(raw_bucket_storage && raw_chain_storage))
		diedie("allocating connection registry bucket&chain mem");

	logg(LOGF_INFO, "G2ConRegistry: Will use %u Levels, %zu Buckets, %zu Chains, for %i Connections, approx. %zu kib QHT mem\n",
	     g2_conreg_level_count, count_b, count_c, server.settings.max_connection_sum, (count_b + count_c) * 128);
	tc = raw_chain_storage;
	init_alloc_bucket(&ht_root, raw_bucket_storage, &tc, 0);
	ht_seed = (uint32_t) time(NULL);

	/* our ht_root carries the global master QHT */
	if(g2_qht_reset(&ht_root.qht, 1 << QHT_DEFAULT_BITS, false))
		diedie("couldn't create global QHT");
	/* the only qht where lazy init harms (at least ATM) */
	memset(ht_root.qht->data, ~0, ht_root.qht->data_length);
	ht_root.qht->flags.reset_needed = false;
#ifdef DRD_ME
	DRD_IGNORE_VAR(ht_root.dirty);
#endif
}

static noinline void free_bucket(struct g2_ht_bucket *b, unsigned level)
{
	struct qhtable *t;
	unsigned i = 0;

	for(; i < LEVEL_SIZE; i++) {
		if(level < (LEVEL_COUNT-1))
			free_bucket(b->d.b[i], level + 1);
		else {
			t = b->d.c[i]->qht;
			b->d.c[i]->qht = NULL;
			if(t) {
				/* we are exiting */
				atomic_set(&t->refcnt, 1);
				g2_qht_put(t);
			}
			pthread_rwlock_destroy(&b->d.c[i]->lock);
		}
	}

	/* swap in the new qht */
	t = b->qht;
	b->qht = NULL;

	if(t) {
		/* we are exiting */
		atomic_set(&t->refcnt, 1);
		/* bring out the gimp */
		g2_qht_put(t);
	}
}

static void g2_conreg_deinit(void)
{
	free_bucket(&ht_root, 0);
	free(raw_bucket_storage);
	free(raw_chain_storage);
}

static struct g2_ht_chain *g2_conreg_find_chain(const union combo_addr *addr)
{
	struct g2_ht_bucket *b = &ht_root;
	uint32_t h = combo_addr_hash_ip(addr, ht_seed);
	unsigned i = 0;

	for(; i < (LEVEL_COUNT-1); i++, h >>= LEVEL_SHIFT)
		b = b->d.b[h & LEVEL_MASK];
	return b->d.c[h & LEVEL_MASK];
}

static void find_chain_and_mark_dirty_rec(struct g2_ht_bucket *b, void (*callback)(struct g2_ht_chain *, g2_connection_t *), g2_connection_t *connec, unsigned i, uint32_t h)
{
	if(i < (LEVEL_COUNT-1))
		find_chain_and_mark_dirty_rec(b->d.b[h & LEVEL_MASK], callback, connec, i+1, h >> LEVEL_SHIFT);
	else
		callback(b->d.c[h & LEVEL_MASK], connec);
	/*
	 * Mark dirty when ascending from recursion.
	 * This is important to set the dirtyness in a "sticky"
	 * fashion since we race with the scrubbing.
	 */
	b->dirty = true;
// TODO: we maybe need a MB here...
}

static void g2_conreg_find_chain_and_mark_dirty(g2_connection_t *connec, void (*callback)(struct g2_ht_chain *, g2_connection_t *))
{
	find_chain_and_mark_dirty_rec(&ht_root, callback, connec, 0, combo_addr_hash_ip(&connec->remote_host, ht_seed));
}

static void chain_callback_dirty(struct g2_ht_chain *c, g2_connection_t *connec GCC_ATTR_UNUSED_PARAM)
{
	pthread_rwlock_rdlock(&c->lock);
	c->dirty = true;
	/* MB: the lock should build a barrier here */
	pthread_rwlock_unlock(&c->lock);
}

void g2_conreg_mark_dirty(g2_connection_t *connec)
{
	if(unlikely(hlist_unhashed(&connec->registry)))
		return;

	g2_conreg_find_chain_and_mark_dirty(connec, chain_callback_dirty);
}

static void chain_callback_add(struct g2_ht_chain *c, g2_connection_t *connec)
{
	pthread_rwlock_wrlock(&c->lock);
	hlist_add_head(&connec->registry, &c->list);
	c->dirty = true;
	/* MB: the lock should build a barrier here */
	pthread_rwlock_unlock(&c->lock);
}

bool g2_conreg_add(g2_connection_t *connec)
{
	if(unlikely(!hlist_unhashed(&connec->registry)))
		return false;

	g2_conreg_find_chain_and_mark_dirty(connec, chain_callback_add);
	return true;
}

static void chain_callback_remove(struct g2_ht_chain *c, g2_connection_t *connec)
{
	pthread_rwlock_wrlock(&c->lock);
	hlist_del(&connec->registry);
	c->dirty = true;
	/* MB: the lock should build a barrier here */
	pthread_rwlock_unlock(&c->lock);
}

bool g2_conreg_remove(g2_connection_t *connec)
{
	if(unlikely(hlist_unhashed(&connec->registry)))
		return false;

	if(unlikely(!list_empty(&connec->hub_list)))
	{
		pthread_rwlock_wrlock(&neighbour_hubs_lock);
		if(likely(!list_empty(&connec->hub_list)))
			list_del_init(&connec->hub_list);
		pthread_rwlock_unlock(&neighbour_hubs_lock);
		atomic_dec(&server.status.act_hub_sum);
		INIT_LIST_HEAD(&connec->hub_list);
	}
	g2_conreg_find_chain_and_mark_dirty(connec, chain_callback_remove);

	INIT_HLIST_NODE(&connec->registry);
	return true;
}

bool g2_conreg_promote_hub(g2_connection_t *connec)
{
	if(unlikely(!list_empty(&connec->hub_list)))
		return false;

	if(atomic_inc_return(&server.status.act_hub_sum) >= server.settings.max_hub_sum) {
		atomic_dec(&server.status.act_hub_sum);
		return false;
	}
	pthread_rwlock_wrlock(&neighbour_hubs_lock);
	list_add_tail(&connec->hub_list, &neighbour_hubs);
	pthread_rwlock_unlock(&neighbour_hubs_lock);
	return true;
}

void g2_conreg_demote_hub(g2_connection_t *connec)
{
	if(unlikely(list_empty(&connec->hub_list)))
		return;

	pthread_rwlock_wrlock(&neighbour_hubs_lock);
	if(likely(!list_empty(&connec->hub_list)))
		list_del_init(&connec->hub_list);
	pthread_rwlock_unlock(&neighbour_hubs_lock);
	atomic_dec(&server.status.act_hub_sum);
}

bool g2_conreg_is_neighbour_hub(const union combo_addr *addr)
{
	struct g2_ht_chain *c;
	struct hlist_node *n;
	g2_connection_t *node;

	c = g2_conreg_find_chain(addr);
	pthread_rwlock_rdlock(&c->lock);
	hlist_for_each_entry(node, n, &c->list, registry)
	{
		if(combo_addr_eq(&node->remote_host, addr)) {
			bool ret_val = node->flags.upeer;
			pthread_rwlock_unlock(&c->lock);
			return ret_val;
		}
	}
	pthread_rwlock_unlock(&c->lock);

	return false;
}

static intptr_t do_callback_level(struct g2_ht_bucket *b, intptr_t (*callback)(g2_connection_t *, void *), void *carg, unsigned level)
{
	unsigned i;
	intptr_t res = 0;

	for(i = 0; i < LEVEL_SIZE; i++)
	{
		if(level < (LEVEL_COUNT-1))
			res |= do_callback_level(b->d.b[i], callback, carg, level + 1);
		else
		{
			struct g2_ht_chain *c = b->d.c[i];
			struct hlist_node *n;
			g2_connection_t *node;

			pthread_rwlock_rdlock(&c->lock);
			hlist_for_each_entry(node, n, &c->list, registry) {
				res |= callback(node, carg);
			}
			pthread_rwlock_unlock(&c->lock);
		}
	}
	return res;
}

intptr_t g2_conreg_all_con(intptr_t (*callback)(g2_connection_t *, void *), void *carg)
{
	return do_callback_level(&ht_root, callback, carg, 0);
}


intptr_t g2_conreg_all_hub(const union combo_addr *filter, intptr_t (*callback)(g2_connection_t *, void *), void *carg)
{
	g2_connection_t *con;
	intptr_t ret_val = 0;

	pthread_rwlock_rdlock(&neighbour_hubs_lock);
	list_for_each_entry(con, &neighbour_hubs, hub_list) {
		if(!(filter && combo_addr_eq(&con->remote_host, filter)))
			ret_val |= callback(con, carg);
	}
	pthread_rwlock_unlock(&neighbour_hubs_lock);
	return ret_val;
}

intptr_t g2_conreg_random_hub(const union combo_addr *filter, intptr_t (*callback)(g2_connection_t *, void *), void *carg)
{
	g2_connection_t *con;

// TODO: make it random...

	pthread_rwlock_rdlock(&neighbour_hubs_lock);
	list_for_each_entry(con, &neighbour_hubs, hub_list)
	{
		if(!(filter && combo_addr_eq(&con->remote_host, filter))) {
			intptr_t ret_val = callback(con, carg);
			pthread_rwlock_unlock(&neighbour_hubs_lock);
			return ret_val;
		}
	}
	pthread_rwlock_unlock(&neighbour_hubs_lock);
	return 0;
}

intptr_t g2_conreg_for_addr(const union combo_addr *addr, intptr_t (*callback)(g2_connection_t *, void *), void *carg)
{
	struct g2_ht_chain *c;
	struct hlist_node *n;
	g2_connection_t *node;

	c = g2_conreg_find_chain(addr);
	pthread_rwlock_rdlock(&c->lock);
	hlist_for_each_entry(node, n, &c->list, registry)
	{
		if(combo_addr_eq(&node->remote_host, addr)) {
			intptr_t ret_val = callback(node, carg);
			pthread_rwlock_unlock(&c->lock);
			return ret_val;
		}
	}
	pthread_rwlock_unlock(&c->lock);

	return 0;
}

intptr_t g2_conreg_for_ip(const union combo_addr *addr, intptr_t (*callback)(g2_connection_t *, void *), void *carg)
{
	struct g2_ht_chain *c;
	struct hlist_node *n;
	g2_connection_t *node;

	c = g2_conreg_find_chain(addr);
	pthread_rwlock_rdlock(&c->lock);
	hlist_for_each_entry(node, n, &c->list, registry)
	{
		if(combo_addr_eq_ip(&node->remote_host, addr)) {
			intptr_t ret_val = callback(node, carg);
			pthread_rwlock_unlock(&c->lock);
			return ret_val;
		}
	}
	pthread_rwlock_unlock(&c->lock);

	return 0;
}

bool g2_conreg_have_ip(const union combo_addr *addr)
{
	struct g2_ht_chain *c;
	struct hlist_node *n;
	g2_connection_t *ret_val;

	c = g2_conreg_find_chain(addr);
	pthread_rwlock_rdlock(&c->lock);
	hlist_for_each_entry(ret_val, n, &c->list, registry)
	{
		if(combo_addr_eq_ip(&ret_val->remote_host, addr)) {
			pthread_rwlock_unlock(&c->lock);
			return true;
		}
	}
	pthread_rwlock_unlock(&c->lock);

	return false;
}

static void do_cleanup_level(struct g2_ht_bucket *b, unsigned level)
{
	unsigned i;

	for(i = 0; i < LEVEL_SIZE; i++)
	{
		struct qhtable *t;

		if(level < (LEVEL_COUNT-1))
		{
			if(!b->d.b[i]->qht)
				continue;
			t = b->d.b[i]->qht;
			b->d.b[i]->qht = NULL;
			atomic_set(&t->refcnt, 1);
			g2_qht_put(t);
		}
		else
		{
			struct hlist_node *n, *e;
			g2_connection_t *connec;

			if(!b->d.c[i]->qht)
				continue;
			t = b->d.c[i]->qht;
			b->d.c[i]->qht = NULL;
			atomic_set(&t->refcnt, 1);
			g2_qht_put(t);

			hlist_for_each_entry_safe(connec, n, e, &b->d.c[i]->list, registry)
			{
				hlist_del(n);
				INIT_HLIST_NODE(n);
				close(connec->com_socket);
				g2_con_free_glob(connec);
			}
		}

		if(level < (LEVEL_COUNT-1))
			do_cleanup_level(b->d.b[i], level + 1);
	}
}

void g2_conreg_cleanup(void)
{
	do_cleanup_level(&ht_root, 0);
	atomic_set(&ht_root.qht->refcnt, 1);
	g2_qht_put(ht_root.qht);
	ht_root.qht = NULL;
	g2_conreg_deinit();
}

/*
 * functions for the global qht
 */
static noinline void do_global_search_chain(struct qht_search_walk *qsw, struct g2_ht_chain *c)
{
	struct hlist_node *n;
	g2_connection_t *connec;

	if(pthread_rwlock_rdlock(&c->lock))
		return;
	/*
	 * now no connection can vanish under us,
	 * we can "safely" read the qht, if we check
	 * for NULL (assume empty qht)
	 * We race with concurrent qht updates, but thats OK.
	 */
	hlist_for_each_entry(connec, n, &c->list, registry)
	{
		read_barrier_depends();
		/* no qht? Busy... */
		if(!connec->qht)
			continue;
		/* nothing to find in empty qhts */
		if(connec->qht->flags.reset_needed)
			continue;
		/* do not search hubs again */
		if(connec->flags.upeer)
			continue;

		g2_qht_global_search_chain(qsw, connec);
	}

	pthread_rwlock_unlock(&c->lock);
}

static void do_global_search(struct qht_search_walk *qsw, struct g2_ht_bucket *b, unsigned level)
{
	unsigned i;

	for(i = 0; i < LEVEL_SIZE; i++)
	{
		struct qhtable *t;
		struct qht_search_walk lqsw;

		if(level < (LEVEL_COUNT-1))
		{
			if(!b->d.b[i]->qht)
				continue;
			do {
				mb();
				hzp_ref(HZP_QHT, t = b->d.b[i]->qht);
			} while(t != b->d.b[i]->qht);
		}
		else
		{
			if(!b->d.c[i]->qht)
				continue;
			do {
				mb();
				hzp_ref(HZP_QHT, t = b->d.c[i]->qht);
			} while(t != b->d.c[i]->qht);
		}

		if(!t)
			continue;

		lqsw = *qsw;
		/* search_bucket unrefs qht */
		if(!g2_qht_global_search_bucket(&lqsw, t))
			continue;

		if(level < (LEVEL_COUNT-1))
				do_global_search(&lqsw, b->d.b[i], level + 1);
		else
				do_global_search_chain(&lqsw, b->d.c[i]);
	}
}

void g2_qht_global_search(struct qht_search_walk *qsw)
{
	do_global_search(qsw, &ht_root, 0);
}

static noinline void do_global_update_chain(struct qhtable *new_master, struct g2_ht_chain *c)
{
	struct qhtable *new_sub = NULL, *old_sub = NULL;

	if(pthread_rwlock_rdlock(&c->lock))
		return;
	/*
	 * now no connection can vanish under us,
	 * we can "safely" read the qht, if we check
	 * for NULL (assume empty qht)
	 * We race with concurrent qht updates, but they
	 * should mark everything dirty again, afterwards.
	 *
	 */
	/* chain empty? don't create a qht */
	if(hlist_empty(&c->list))
	{
		old_sub = c->qht;
		c->qht = new_sub;
		goto out_unlock;
	}

	if(c->dirty)
	{
		struct hlist_node *n;
		g2_connection_t *connec;

		if(g2_qht_reset(&new_sub, 1 << QHT_DEFAULT_BITS, false)) {
			logg_devel("preparing a new chain qht failed?");
			goto out_unlock;
		}

		c->dirty = false;
		hlist_for_each_entry(connec, n, &c->list, registry)
		{
			if(!connec->qht)
				continue;

			/* keep empty qht out of master qht */
			/* keep other hubs out of master qht */
			/* keep too full qhts out of master qht */
			if(!connec->qht->flags.reset_needed &&
			   !connec->flags.upeer &&
			    connec->qht->used < server.settings.qht.max_promille)
				g2_qht_aggregate(new_sub, connec->qht);
		}
		old_sub = c->qht;
		c->qht = new_sub;
	}
	else
		new_sub = c->qht;

out_unlock:
	pthread_rwlock_unlock(&c->lock);

	/* bring out the gimp */
	g2_qht_put(old_sub);

	if(new_sub && !new_sub->flags.reset_needed) {
		if(new_master->flags.reset_needed) {
			my_memcpy(new_master->data, new_sub->data, new_master->data_length);
			new_master->flags.reset_needed = false;
		} else
			memand(new_master->data, new_sub->data, new_master->data_length);
	}
}

static noinline void do_global_update(struct qhtable *new_master, struct g2_ht_bucket *b, unsigned level)
{
	struct qhtable *new_sub = NULL;

	if(b->dirty)
	{
		struct qhtable *old_sub;
		unsigned i;

// TODO: error handling
		g2_qht_reset(&new_sub, 1 << QHT_DEFAULT_BITS, false);

		/*
		 * resetting the dirty state is racy,
		 * but this is OK.
		 * We want to scan and sweep. If we sweep some times
		 * more, thats not bad, as long as we don't miss an update.
		 */
		b->dirty = false;
		for(i = 0; i < LEVEL_SIZE; i++) {
			if(level < (LEVEL_COUNT-1))
				do_global_update(new_sub, b->d.b[i], level + 1);
			else
				do_global_update_chain(new_sub, b->d.c[i]);
		}

		if(!new_master && new_sub->flags.reset_needed) {
			/* the only qht where lazy init harms (at least ATM) */
			memset(new_sub->data, ~0, new_sub->data_length);
			new_sub->flags.reset_needed = false;
		}

		/* swap in the new qht */
		old_sub = atomic_px(new_sub, (atomicptr_t *)(uintptr_t)&b->qht);
		/* bring out the gimp */
		g2_qht_put(old_sub);
	}
	if(new_sub && !new_sub->flags.reset_needed && new_master)
	{
		if(new_master->flags.reset_needed) {
			my_memcpy(new_master->data, new_sub->data, new_master->data_length);
			new_master->flags.reset_needed = false;
		} else
			memand(new_master->data, new_sub->data, new_master->data_length);
	}
}

static mutex_t global_update_lock;

GCC_ATTR_CONSTRUCT static void init_global_update_lock(void)
{
	if((errno = mutex_init(&global_update_lock)))
		diedie("couldn't init global update lock");
}

void g2_qht_global_update(void)
{
	static time_t last_update;
	long tdiff;

	/* catch updates to last_update */
	rmb();
	tdiff = local_time_now - last_update;
	tdiff = tdiff >= 0 ? tdiff : -tdiff;
	if(tdiff < UPDATE_INTERVAL)
		return;

	/* only one updater at a time */
	if(mutex_trylock(&global_update_lock))
		return;
	/* catch updates to last_update, the lock being the MB */

	/* since we now have the lock, recheck last_update now race free */
	tdiff = local_time_now - last_update;
	tdiff = tdiff >= 0 ? tdiff : -tdiff;
	if(tdiff < UPDATE_INTERVAL)
		goto OUT_UNLOCK;

	last_update = local_time_now;
	/* flush out last_update */
	mb();
	/* catch the dirty marks to this point */
	do_global_update(NULL, &ht_root, 0);
	wmb(); /* put all qht writes into "place" */
// TODO: When the QHT gets updated, push change activly to other hubs

OUT_UNLOCK:
	if(unlikely(tdiff = mutex_unlock(&global_update_lock))) {
		errno = tdiff;
		diedie("Huuarg, ConReg update lock stuck, bye!");
	}
}

size_t g2_qht_global_get_ent(void)
{
	return ht_root.qht->entries;
}

struct qhtable *g2_qht_global_get(void)
{
	struct qhtable *ret_val;

retry:
	rmb();
	ret_val = ht_root.qht;
	if(unlikely(0 == atomic_inc_return(&ret_val->refcnt))) {
		/* yikes, we zombied a qht which died some µs ago...  */
		atomic_set(&ret_val->refcnt, 0);
		goto retry;
	}

	return ret_val;
}

struct qhtable *g2_qht_global_get_hzp(void)
{
	struct qhtable *ret_val;

	do {
		mb();
		hzp_ref(HZP_QHT, ret_val = ht_root.qht);
	} while(ret_val != ht_root.qht);

	return ret_val;
}

/*@unused@*/
static char const rcsid_cr[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
