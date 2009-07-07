/*
 * G2QueryKey.c
 * Query Key syndication
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
#include "lib/combo_addr.h"
#include "lib/ansi_prng.h"

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

 /* Types */
struct g2_qk_salts
{
	uint32_t salts[TIME_SLOT_COUNT][TIME_SLOT_ELEM + 1][2];
	volatile unsigned act_salt;
	time_t last_update;
} GCC_ATTR_ALIGNED(16);

/* Vars */
static struct g2_qk_salts g2_qk_s;

/* Protos */

/* Funcs */
static void check_salt_vals(unsigned j)
{
	unsigned i;

	/* make sure no unpleasant values are in the array */
	for(i = 0; i < anum(g2_qk_s.salts[0]); i++)
	{
// TODO: check every byte
		while(0 == g2_qk_s.salts[j][i][0] || 1 == g2_qk_s.salts[j][i][0])
			g2_qk_s.salts[j][i][0] = rand();
		while(0 == g2_qk_s.salts[j][i][1] || 1 == g2_qk_s.salts[j][i][1])
			g2_qk_s.salts[j][i][1] = rand();
	}
}

void g2_qk_init(void)
{
	random_bytes_get(g2_qk_s.salts, sizeof(g2_qk_s.salts));
	check_salt_vals(0);
	g2_qk_s.last_update = time(NULL);
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
	/*
	 * reseed the libc random number generator every tick,
	 * for the greater good of the whole server
	 */
	random_bytes_get(&t, sizeof(t));
	srand(t);
	check_salt_vals(n_salt);

	g2_qk_s.act_salt = n_salt;
	g2_qk_s.last_update = local_time_now;
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

	h = addr_hash_generate(source, act_salt);
	return (h & TIME_SLOT_COUNT_MASK) | act_salt;
}

bool g2_qk_check(const union combo_addr *source, uint32_t key)
{
	uint32_t h;

	h = addr_hash_generate(source, key & ~TIME_SLOT_COUNT_MASK);
	return (h & TIME_SLOT_COUNT_MASK) == (key & TIME_SLOT_COUNT_MASK);
}

bool g2_qk_lookup(uint32_t *qk, const union combo_addr *addr)
{
	return false;
}

void g2_qk_add(uint32_t qk, const union combo_addr *addr)
{
}

/*@unused@*/
static char const rcsid_qk[] GCC_ATTR_USED_VAR = "$Id:$";
/* EOF */
