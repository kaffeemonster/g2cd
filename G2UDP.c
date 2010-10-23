/*
 * G2UDP.c
 * thread to handle the UDP-part of the G2-Protocol
 *
 * Copyright (c) 2004-2010 Jan Seiffert
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
 * $Id: G2UDP.c,v 1.16 2005/11/05 10:03:50 redbully Exp redbully $
 */

#define WANT_QHT_ZPAD
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* System includes */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "lib/my_pthread.h"
#include <errno.h>
#include <unistd.h>
/* System net-includes */
#include <sys/types.h>
#include <sys/stat.h>
#include "lib/my_epoll.h"
#include "lib/combo_addr.h"
#include <fcntl.h>
/* other */
#include "lib/other.h"
/* Own includes */
#define _G2UDP_C
#define _NEED_G2_P_TYPE
#include "G2Packet.h"
#include "G2UDP.h"
#include "gup.h"
#include "G2MainServer.h"
#include "G2PacketSerializer.h"
#include "G2QHT.h"
#include "lib/my_bitopsm.h"
#include "lib/atomic.h"
#include "lib/my_epoll.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/recv_buff.h"
#include "lib/udpfromto.h"
#include "lib/hzp.h"
#include "lib/rbtree.h"
#include "lib/hlist.h"
#include "lib/ansi_prng.h"

/* IPv4 defines a min MTU of 576 byte. With all header and a little safety */
#define UDP_MTU        500
/* IPv6 defines a min MTU of 1280. Since the addresses are longer more safety */
#define UDP6_MTU       1000
#define UDP_RELIABLE_LENGTH 8

#define FLAG_DEFLATE   (1 << 0)
#define FLAG_ACK       (1 << 1)
#define FLAG_CRITICAL  ((~(FLAG_DEFLATE|FLAG_ACK)) & 0x0F)

#define SEQUENCE_SPACE 16
/*
 * UDP reassambly Cache
 */
#define UDP_CACHE_SHIFT 13
#define UDP_CACHE_SIZE (1 << UDP_CACHE_SHIFT)
#define UDP_CACHE_HTSIZE (UDP_CACHE_SIZE/8)
#define UDP_CACHE_HTMASK (UDP_CACHE_HTSIZE-1)
#define UDP_REAS_TIMEOUT 30

/* Types */
struct reas_knot
{
	struct reas_knot *next;
	unsigned char nr;
	unsigned char end;
};

#define BPLONG (sizeof(unsigned long) * BITS_PER_CHAR)
struct udp_reas_entry
{
	unsigned long parts_recv[DIV_ROUNDUP(255, BPLONG)];
	struct reas_knot *first;
	time_t when;
	union combo_addr na;

	uint16_t sequence;
	uint8_t  part_count;
	bool     deflate;
};
#define TST_BIT(x, y) ((x)[(y)/BPLONG] & (1UL << ((y) % BPLONG)))
#define SET_BIT(x, y) ((x)[(y)/BPLONG] |= (1UL << ((y) % BPLONG)))

struct udp_reas_cache_entry
{
	struct rb_node rb;
	struct hlist_node node;
	struct udp_reas_entry e;
};

/* internal prototypes */
static ssize_t udp_sock_send(struct pointer_buff *, const union combo_addr *, some_fd);
static ssize_t handle_udp_sock(struct epoll_event *, struct norm_buff **, union combo_addr *, union combo_addr *, some_fd, unsigned);
static noinline bool handle_udp_packet(struct norm_buff **, union combo_addr *, union combo_addr *, some_fd);
static inline bool init_con_u(some_fd *, union combo_addr *);

/* Vars */
static struct
{
	pthread_mutex_t lock;
	uint32_t ht_seed;
	struct hlist_head free_list;
	struct rb_root tree;
	struct hlist_head ht[UDP_CACHE_HTSIZE];
	struct udp_reas_cache_entry entrys[UDP_CACHE_SIZE];
} cache;
static int out_file;
static uint32_t sequence_seed;
static struct
{
	enum guppies gup;
	some_fd fd;
	union combo_addr na;
} *udp_sos;
static unsigned udp_sos_num;

#ifndef HAVE___THREAD
static pthread_key_t key2udp_lsb;
static pthread_key_t key2udp_lub;
static pthread_key_t key2udp_lcb;
#else
static __thread struct norm_buff *udp_lsbuffer;
static __thread struct big_buff *udp_lubuffer;
static __thread struct big_buff *udp_lcbuffer;
#endif
/* protos */
	/* you better not kill this prot, our it won't work ;) */
static void udp_init(void) GCC_ATTR_CONSTRUCT;
static void udp_deinit(void) GCC_ATTR_DESTRUCT;

static __init void udp_init(void)
{
	size_t i;
#ifndef HAVE___THREAD
	if(pthread_key_create(&key2udp_lsb, (void(*)(void *))recv_buff_free))
		diedie("couln't create TLS key for local UDP send buffer");
	if(pthread_key_create(&key2udp_lub, (void(*)(void *))recv_buff_free))
		diedie("couln't create TLS key for local UDP uncompress buffer");
	if(pthread_key_create(&key2udp_lcb, (void(*)(void *))free))
		diedie("couln't create TLS key for local UDP compress buffer");
#endif
	if(pthread_mutex_init(&cache.lock, NULL))
		diedie("initialising udp cache lock");
	/* shuffle all entrys in the free list */
	for(i = 0; i < UDP_CACHE_SIZE; i++)
		hlist_add_head(&cache.entrys[i].node, &cache.free_list);
}

static void udp_deinit(void)
{
#ifndef HAVE___THREAD
	struct norm_buff *tmp_buf;

	if((tmp_buf = pthread_getspecific(key2udp_lsb))) {
		recv_buff_free(tmp_buf);
		pthread_setspecific(key2udp_lsb, NULL);
	}
	pthread_key_delete(key2udp_lsb);
	if((tmp_buf = pthread_getspecific(key2udp_lub))) {
		recv_buff_free(tmp_buf);
		pthread_setspecific(key2udp_lub, NULL);
	}
	pthread_key_delete(key2udp_lub);
	if((tmp_buf = pthread_getspecific(key2udp_lcb))) {
		free(tmp_buf); /* this is a big buffer */
		pthread_setspecific(key2udp_lcb, NULL);
	}
	pthread_key_delete(key2udp_lcb);
#endif
	pthread_mutex_destroy(&cache.lock);
}

static struct norm_buff *udp_get_lsbuf(void)
{
	struct norm_buff *ret_buf;
#ifndef HAVE___THREAD
	ret_buf = pthread_getspecific(key2udp_lsb);
#else
	ret_buf = udp_lsbuffer;
#endif
	if(likely(ret_buf)) {
		ret_buf->pos = 0;
		ret_buf->limit = ret_buf->capacity = sizeof(ret_buf->data);
		return ret_buf;
	}

	ret_buf = recv_buff_alloc();

	if(!ret_buf) {
		logg_devel("no local udp send buffer\n");
		return NULL;
	}

#ifndef HAVE___THREAD
	pthread_setspecific(key2udp_lsb, ret_buf);
#else
	udp_lsbuffer = ret_buf;
#endif

	return ret_buf;
}

static struct big_buff *udp_get_lubuf(void)
{
	struct big_buff *ret_buf;
#ifndef HAVE___THREAD
	ret_buf = pthread_getspecific(key2udp_lub);
#else
	ret_buf = udp_lubuffer;
#endif
	if(likely(ret_buf)) {
		ret_buf->pos = 0;
		ret_buf->limit = ret_buf->capacity = server.settings.max_g2_packet_length;
		return ret_buf;
	}

	ret_buf = malloc(sizeof(*ret_buf) + server.settings.max_g2_packet_length);

	if(!ret_buf) {
		logg_devel("no local udp uncompress buffer\n");
		return NULL;
	}
	ret_buf->pos = 0;
	ret_buf->limit = ret_buf->capacity = server.settings.max_g2_packet_length;

#ifndef HAVE___THREAD
	pthread_setspecific(key2udp_lub, ret_buf);
#else
	udp_lubuffer = ret_buf;
#endif

	return ret_buf;
}

static struct big_buff *udp_get_c_buff(size_t len)
{
	struct big_buff *ret_buf, *t;

#ifndef HAVE___THREAD
	ret_buf = pthread_getspecific(key2udp_lcb);
#else
	ret_buf = udp_lcbuffer;
#endif
	if(likely(ret_buf)) {
		if(len <= ret_buf->capacity) {
			buffer_clear(*ret_buf);
			return ret_buf;
		}
	}

	/* round up to a power of two */
	len += sizeof(*t);
	len--;
	len |= len >> 1;
	len |= len >> 2;
	len |= len >> 4;
	len |= len >> 8;
	len |= len >> 16;
	len++;
	len += len == 0;
	len = (len < 4096 ? 4096 : len);
	t = malloc(len);
	if(!t) {
		logg_devel("no local udp compress buffer\n");
		return NULL;
	}
	free(ret_buf);
	t->limit    = t->capacity = len - sizeof(*t);
	t->pos      = 0;

#ifndef HAVE___THREAD
	pthread_setspecific(key2udp_lcb, t);
#else
	udp_lcbuffer = t;
#endif

	return t;
}

/****************** UDP reassambly Cache funcs ******************/

static inline struct norm_buff *knot2buff(struct reas_knot *k)
{
	char *x = (char *)k;

	x -= NORM_BUFF_CAPACITY - sizeof(*k);
	return container_of(x, struct norm_buff, data);
}

static inline struct reas_knot *buff2knot(struct norm_buff *b)
{
	return (struct reas_knot *)((b->data + b->capacity) - sizeof(struct reas_knot));
}

static struct udp_reas_cache_entry *udp_reas_cache_entry_alloc(void)
{
	struct udp_reas_cache_entry *e;
	struct hlist_node *n;

	if(hlist_empty(&cache.free_list))
		return NULL;

	n = cache.free_list.first;
	e = hlist_entry(n, struct udp_reas_cache_entry, node);
	hlist_del(&e->node);
	INIT_HLIST_NODE(&e->node);
	RB_CLEAR_NODE(&e->rb);
	return e;
}

static void udp_reas_cache_entry_free(struct udp_reas_cache_entry *e)
{
	struct reas_knot *next;

	for(next = e->e.first; next;) {
		struct norm_buff *n = knot2buff(next);
		next = next->next;
		recv_buff_local_ret(n);
	}
	memset(e, 0, sizeof(*e));
	hlist_add_head(&e->node, &cache.free_list);
}

static void udp_reas_cache_entry_free_unlocked(struct udp_reas_cache_entry *e)
{
	struct reas_knot *next;

	for(next = e->e.first; next;) {
		struct norm_buff *n = knot2buff(next);
		next = next->next;
		recv_buff_local_ret(n);
	}
	memset(e, 0, sizeof(*e));

	pthread_mutex_lock(&cache.lock);
	hlist_add_head(&e->node, &cache.free_list);
	pthread_mutex_unlock(&cache.lock);
}

static void udp_reas_cache_entry_free_glob(struct udp_reas_cache_entry *e)
{
	struct reas_knot *next;

	for(next = e->e.first; next;) {
		struct norm_buff *n = knot2buff(next);
		next = next->next;
		recv_buff_free(n);
	}
	memset(e, 0, sizeof(*e));
	hlist_add_head(&e->node, &cache.free_list);
}

static uint32_t cache_ht_hash(const union combo_addr *addr, uint16_t seq)
{
	return combo_addr_hash_extra(addr, seq, cache.ht_seed);
}

static struct udp_reas_cache_entry *cache_ht_lookup(const union combo_addr *addr, uint16_t seq, uint32_t h)
{
	struct hlist_node *n;
	struct udp_reas_cache_entry *e;

// TODO: check for mapped ip addr?
// TODO: when IPv6 is common, change it
// TODO: leave out port?
	if(likely(addr->s.fam == AF_INET))
	{
		hlist_for_each_entry(e, n, &cache.ht[h & UDP_CACHE_HTMASK], node)
		{
			if(e->e.na.s.fam != AF_INET)
				continue;
			if(e->e.na.in.sin_addr.s_addr == addr->in.sin_addr.s_addr &&
			   e->e.na.in.sin_port == addr->in.sin_port &&
			   e->e.sequence == seq)
				return e;
		}
	}
	else
	{
		hlist_for_each_entry(e, n, &cache.ht[h & UDP_CACHE_HTMASK], node)
		{
			if(e->e.na.s.fam != AF_INET6)
				continue;
			if(IN6_ARE_ADDR_EQUAL(&e->e.na.in6.sin6_addr, &addr->in6.sin6_addr) &&
			   e->e.na.in.sin_port == addr->in.sin_port &&
			   e->e.sequence == seq)
				return e;
		}
	}

	return NULL;
}

static void cache_ht_add(struct udp_reas_cache_entry *e, uint32_t h)
{
	hlist_add_head(&e->node, &cache.ht[h & UDP_CACHE_HTMASK]);
}

static void cache_ht_del(struct udp_reas_cache_entry *e)
{
	hlist_del(&e->node);
}

static int udp_reas_entry_cmp(struct udp_reas_cache_entry *a, struct udp_reas_cache_entry *b)
{
	int ret = (long)a->e.when - (long)b->e.when;
	if(ret)
		return ret;
	if((ret = combo_addr_cmp(&a->e.na, &b->e.na)))
		return ret;
	return (int)a->e.sequence - (int)b->e.sequence;
}

static noinline bool udp_reas_rb_cache_insert(struct udp_reas_cache_entry *e)
{
	struct rb_node **p = &cache.tree.rb_node;
	struct rb_node *parent = NULL;

	while(*p)
	{
		struct udp_reas_cache_entry *n = rb_entry(*p, struct udp_reas_cache_entry, rb);
		int result = udp_reas_entry_cmp(n, e);

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

static noinline bool udp_reas_rb_cache_remove(struct udp_reas_cache_entry *e)
{
	struct rb_node *n = &e->rb;

	if(RB_EMPTY_NODE(n))
		return false;

	rb_erase(n, &cache.tree);
	RB_CLEAR_NODE(n);
	cache_ht_del(e);

	return true;
}

static struct udp_reas_cache_entry *udp_reas_cache_last(void)
{
	struct rb_node *n = rb_last(&cache.tree);
	if(!n)
		return NULL;
	return rb_entry(n, struct udp_reas_cache_entry, rb);
}

static struct udp_reas_cache_entry *udp_reas_cache_entry_prev(struct udp_reas_cache_entry *e)
{
	struct rb_node *n = rb_prev(&e->rb);
	if(!n)
		return NULL;
	return rb_entry(n, struct udp_reas_cache_entry, rb);
}

void g2_udp_reas_timeout(void)
{
	struct udp_reas_cache_entry *e;
	time_t to_time;

	to_time = local_time_now - UDP_REAS_TIMEOUT;
	if(unlikely(pthread_mutex_lock(&cache.lock)))
		return;

	for(e = udp_reas_cache_last(); e && e->e.when < to_time;)
	{
		struct udp_reas_cache_entry *t = e;
		e = udp_reas_cache_entry_prev(e);
		logg_devel_old("found a timeouter\n");
		udp_reas_rb_cache_remove(t);
		udp_reas_cache_entry_free_glob(t);
	}

	if(unlikely(pthread_mutex_unlock(&cache.lock)))
		diedie("udp reassambly lock stuck, sh**!");
}

static g2_packet_t *packet_reasamble(struct udp_reas_cache_entry *e, g2_packet_t *st_pack, struct norm_buff **d_hold)
{
	g2_packet_t *ret_val = NULL, *g_packet;
	struct big_buff *d_hold_u = NULL;
	struct zpad *zp = NULL;
	struct reas_knot *next = NULL, **start;
	bool first_data = true;

	g_packet = st_pack;
	if(e->e.deflate)
	{
		logg_devel_old("compressed packet recevied\n");
		d_hold_u = udp_get_lubuf();
		if(!d_hold_u)
			goto out_free_e;

		/*
		 * because we do not want to receive QHTs by UDP, we can
		 * share the zlib TLS alloc stuff with the QHT handling.
		 * And even if, it's no problem, inflating should be finished
		 * (at least ATM) afterwards and QHT can reuse the pad.
		 */
		zp = qht_get_zpad();
		if(!zp)
			goto out_free_e;

		zp->z.next_in   = NULL;
		zp->z.avail_in  = 0;
		zp->z.next_out  = NULL;
		zp->z.avail_out = 0;
		if(inflateInit(&zp->z) != Z_OK) {
			logg_devel("infalteInit failed\n");
			goto out_zfree;
		}
	}

	start = &e->e.first;
	for(next = *start; next; next = *start)
	{
		struct norm_buff *src_r;

		*start = (*start)->next;
		src_r = knot2buff(next);
		if(e->e.part_count > 2)
			logg_develd_old("nr: %zu\tend: %zu\tpos: %zu\tlimit: %zu\thead: %zu\t%c\n", next->nr, next->end, src_r->pos, src_r->limit, buffer_headroom(*src_r) - sizeof(struct reas_knot), e->e.deflate ? 't' : 'f');
		do
		{
			if(e->e.deflate)
			{
				int z_status;

				zp->z.next_in   = (Bytef *)buffer_start(*src_r);
				zp->z.avail_in  = buffer_remaining(*src_r);
				zp->z.next_out  = (Bytef *)buffer_start(*d_hold_u);
				zp->z.avail_out = buffer_remaining(*d_hold_u);

				z_status = inflate(&zp->z, Z_SYNC_FLUSH);
				switch(z_status)
				{
				case Z_OK:
				case Z_STREAM_END:
					src_r->pos += buffer_remaining(*src_r) - zp->z.avail_in;
					d_hold_u->pos += buffer_remaining(*d_hold_u) - zp->z.avail_out;
					break;
				case Z_NEED_DICT:
					logg_devel("Z_NEED_DICT\n");
					goto out_free;
				case Z_DATA_ERROR:
					logg_develd("Z_DATA_ERROR ai: %u ao: %u\n", zp->z.avail_in, zp->z.avail_out);
					goto out_free;
				case Z_STREAM_ERROR:
					logg_devel("Z_STREAM_ERROR\n");
					goto out_free;
				case Z_MEM_ERROR:
					logg_devel("Z_MEM_ERROR\n");
					goto out_free;
				case Z_BUF_ERROR:
					logg_devel("Z_BUF_ERROR\n");
					goto out_free;
				default:
					logg_devel("inflate was not Z_OK\n");
					goto out_free;
				}
				buffer_flip(*d_hold_u);
				if(first_data) {
					if(buffer_remaining(*d_hold_u) >= 3 &&
					   unlikely('G' == *buffer_start(*d_hold_u) &&
					            'N' == *(buffer_start(*d_hold_u)+1) &&
					            'D' == *(buffer_start(*d_hold_u)+2))) {
						logg_develd_old("received double GND from %pI# after reas+unpack\n", &e->e.na);
						goto out_free;
					}
					first_data = false;
				}
				/* Look at the packet received */
				if(!g2_packet_extract_from_stream_b(d_hold_u, g_packet, server.settings.max_g2_packet_length, false)) {
					logg_devel("packet extract failed\n");
					goto out_free;
				}
			}
			else
			{
				/* Look at the packet received */
				if(!g2_packet_extract_from_stream(src_r, g_packet, server.settings.max_g2_packet_length, true)) {
					logg_devel("packet extract failed\n");
					goto out_free;
				}
			}

			if(DECODE_FINISHED == g_packet->packet_decode ||
			   PACKET_EXTRACTION_COMPLETE == g_packet->packet_decode) {
				ret_val = g_packet;
				goto out_zfree;
			}

			if(e->e.deflate)
				buffer_compact(*d_hold_u);
			else
			{
				if(buffer_remaining(*src_r)) {
					logg_devel("couldn't remove all data from buffer\n");
					goto out_free;
				}
			}
		} while(buffer_remaining(*src_r));
		recv_buff_local_ret(src_r);
	}
	logg_devel("incomplete packet in reas\n");
out_free:
	g2_packet_free(g_packet);
out_zfree:
	if(next)
	{
		/*
		 * Since we may have data left lingering in the buffer
		 * if the whole packet fit's into the buffer, we have to
		 * make sure we do not free the wrong buffer (src_r).
		 * Depending on the where abouts we either have the data
		 * in the lbuffer (after inflate) or we didn't use the
		 * d_hold, so exchange d_hold for src_r.
		 */
		struct norm_buff *src_r = knot2buff(next);
		if(*d_hold)
		{
			if(e->e.deflate)
				recv_buff_local_ret(src_r);
			else {
				recv_buff_local_ret(*d_hold);
				*d_hold = src_r;
			}
		}
		else
			*d_hold = src_r;
	}
	if(e->e.deflate && zp)
		inflateEnd(&zp->z);
out_free_e:
	udp_reas_cache_entry_free_unlocked(e);
	return ret_val;
}

static noinline void reas_knots_optimize(struct reas_knot *x, struct norm_buff **d_hold)
{
	/*
	 * we want to copy buffers together, but that is fsck
	 * complicated.
	 * This way we can free recv buffers much earlier,
	 * we can cram up to 8 1/2 parts into one buffer.
	 * Nearby we linearize uncompressed packets so we can
	 * go back to the "packet data stays in recv buffer"
	 * scheme for decode.
	 * Linerazing even two buffers hopefully helps zlib
	 * to not stop inflating and twiddle around with the
	 * window (allocs, extra copies).
	 */
	for(; x && x->next; x = x->next)
	{
		struct norm_buff *b = knot2buff(x);
		buffer_compact(*b);
		b->limit -= sizeof(struct reas_knot); /* undo limit change by compact */
		if(buffer_remaining(*b) &&
		   (x->end + 1 == x->next->nr || x->end == x->next->nr))
		{
			struct norm_buff *c = knot2buff(x->next);
			size_t blen = buffer_remaining(*b);
			size_t clen = buffer_remaining(*c);

			blen = blen < clen ? blen : clen;
			my_memcpy(buffer_start(*b), buffer_start(*c), blen);
			b->pos += blen;
			c->pos += blen;
			buffer_compact(*c);
			buffer_flip(*c);
			if(buffer_remaining(*c))
				x->end = x->next->nr;
			else
			{
				x->end = x->next->end;
				x->next = x->next->next;
				if(*d_hold)
					recv_buff_local_ret(c);
				else
					*d_hold = c;
			}
		}
		buffer_flip(*b);
	}
}

static noinline g2_packet_t *g2_udp_reas_add(gnd_packet_t *p, struct norm_buff **d_hold, const union combo_addr *addr, g2_packet_t *st_pack)
{
	struct udp_reas_cache_entry *e;
	g2_packet_t *ret_val = NULL;
	uint32_t h;
	uint16_t seq;
	time_t when;

	if(sizeof(struct reas_knot) > buffer_headroom(**d_hold)) {
		logg_devel("buffer-with not enough headroom for reas???\n");
		return NULL;
	}
	seq = p->sequence;
	h = cache_ht_hash(addr, seq);
	when = local_time_now;
	/*
	 * Prevent the compiler from moving the calcs into
	 * the critical section
	 */
	barrier();

	if(unlikely(pthread_mutex_lock(&cache.lock)))
		return NULL;
	/* already in the cache? */
	e = cache_ht_lookup(addr, seq, h);
	if(e)
	{
		if(!udp_reas_rb_cache_remove(e)) {
			logg_devel("remove failed\n");
			goto life_tree_error; /* something went wrong... */
		}
		/* sanity checks */
//TODO: add blacklist entry if failed?
		if(e->e.deflate != p->flags.deflate) {
			logg_devel("udp packet changed compression?\n");
			goto out_free_e; /* free and out here */
		}
		if(e->e.part_count != p->count) {
			logg_devel_old("udp packet changed part count?\n");
			goto out_free_e; /* free and out here */
		}
	}
	else
	{
		if(likely(!hlist_empty(&cache.free_list)))
			e = udp_reas_cache_entry_alloc();
		else
		{
			e = udp_reas_cache_last();
			if(likely(e) &&
			   e->e.when < when)
			{
				struct reas_knot *next;
				static bool once;
				if(!once) {
					logg_devel("found older entry\n");
					once = true;
				}
				if(!udp_reas_rb_cache_remove(e))
					goto life_tree_error; /* the tree is amiss? */
//TODO: add blacklist entry for salvaged reas
				/* clean entry */
				for(next = e->e.first; next;) {
					struct norm_buff *n = knot2buff(next);
					next = next->next;
					recv_buff_local_ret(n);
				}
				memset(e, 0, sizeof(*e));
				INIT_HLIST_NODE(&e->node);
				RB_CLEAR_NODE(&e->rb);
			}
			else
				goto out_unlock;
		}
		if(!e)
			goto out_unlock;

		e->e.na = *addr;
		e->e.deflate = p->flags.deflate;
		e->e.sequence = p->sequence;
		e->e.part_count = p->count;
	}

	/*
	 * Look if we already have this part.
	 * In an ideal world we would never get the same ip/seq/part,
	 * so a simple insert sounds good.
	 * But since a client maybe didn't get the ack or resends a
	 * part for whatever reason, we check.
	 */
	if(!TST_BIT(e->e.parts_recv, p->part - 1))
	{
		unsigned long recv_mask;
		struct reas_knot *n_knot = buff2knot(*d_hold);
		struct reas_knot **cursor;
		unsigned i, pc;

		SET_BIT(e->e.parts_recv, p->part - 1);
		/* only update time if we know we want to keep this part */
		e->e.when = when;
		/* create "all parts received" mask */
		recv_mask = 0;
		for(pc = e->e.part_count, i = 0; pc >= BPLONG && i < anum(e->e.parts_recv); pc -= BPLONG, i++)
			recv_mask |= (~0UL) ^ e->e.parts_recv[i];
		if(i < anum(e->e.parts_recv)) {
			recv_mask |= ((1UL << pc) - 1) ^ e->e.parts_recv[i++];
			for(; i < anum(e->e.parts_recv); i++)
				recv_mask |= 0 ^ e->e.parts_recv[i];
		}
		/*
		 * If we have all parts, we can unlock the cache NOW!
		 *
		 * If the packet is complete, there are some heavy actions
		 * ahead, mainly deflating the received packet 95% of the
		 * time (lots of cpu cycles), but also the last two optimize
		 * runs and packet_extract_from_stream burns cpu cycles
		 * under the lock. Prevent that by unlocking now, we do not
		 * need any serialization after this point.
		 * This lock will still be the UDP contention point
		 * (assumed...), but this should help a lot.
		 */
		if(recv_mask == 0)
			pthread_mutex_unlock(&cache.lock);

		n_knot->nr   = p->part;
		n_knot->end  = p->part;
		n_knot->next = NULL;
		/* insert in order */
		for(cursor = &e->e.first; *cursor; cursor = &(*cursor)->next) {
			if((*cursor)->nr > n_knot->nr)
				break;
		}
		n_knot->next = *cursor;
		*cursor = n_knot;
		*d_hold = NULL;

		/* optimize */
		reas_knots_optimize(e->e.first, d_hold);

		if(recv_mask == 0)
		{
			/* take a last shot at concatinating */
			if(e->e.first->next)
				reas_knots_optimize(e->e.first, d_hold);
			ret_val = packet_reasamble(e, st_pack, d_hold);

			/*
			 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			 * We already unlocked and free'd the entry, get out!
			 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			 */
			return ret_val;
		}
	}

	if(!udp_reas_rb_cache_insert(e)) {
		logg_devel("insert failed\n");
		goto life_tree_error;
	}
	else
		cache_ht_add(e, h);
out_unlock:
	if(unlikely(pthread_mutex_unlock(&cache.lock)))
		diedie("ahhhh, udp reassembly cache lock stuck, bye!");

	return ret_val;
out_free_e:
life_tree_error:
	udp_reas_cache_entry_free(e);
	goto out_unlock;
}

/************************* UDP work funcs ***********************/
void handle_udp(struct epoll_event *ev, struct norm_buff *d_hold_sp[MULTI_RECV_NUM], some_fd epoll_fd)
{
	static int warned;
	/* other variables */
	struct simple_gup *sg = ev->data.ptr;
	union combo_addr from[MULTI_RECV_NUM], to[MULTI_RECV_NUM];
	ssize_t result;
	unsigned i, j;
	bool keep_going = true;

	/* move buffers to the array start, clear them, count them */
	for(i = 0, j = 0; i < MULTI_RECV_NUM; i++)
	{
		if(!d_hold_sp[i])
			continue;

		d_hold_sp[j] = d_hold_sp[i];
		buffer_clear(*(d_hold_sp[j]));
		j++;
	}
	for(i = j; i < MULTI_RECV_NUM; i++)
		d_hold_sp[i] = NULL;

// TODO: remove
	/* debug buffer health */
	if(!warned)
	{
		for(i = 0; i < j; i++) {
			if(d_hold_sp[i]->capacity != NORM_BUFF_CAPACITY) {
				logg_develd("small buffer!: %zu\n", d_hold_sp[i]->capacity);
				warned++;
			}
		}
	}

	if((result = handle_udp_sock(ev, d_hold_sp, from, to, sg->fd, j)) < 0) {
		/* bad things */
		keep_going = false;
	}
	/*
	 * We have extracted the data from the UDP socket, now work on it
	 * but first we return the UDP socket back into epoll, so other
	 * threads can receive the next packet.
	 */
	ev->events = (uint32_t)(EPOLLIN | EPOLLERR | EPOLLONESHOT);
	if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sg->fd, ev)) {
		logg_errno(LOGF_ERR, "reactivating udp-socket in epoll");
// TODO: handle bad things
	}

	if(keep_going)
	{
		/* if we reach here, we know that there is at least no error or the logic above failed... */
		for(i = 0; i < (unsigned)result; i++) {
			buffer_flip(*(d_hold_sp[i]));
			handle_udp_packet(&d_hold_sp[i], &from[i], &to[i], sg->fd);
		}
	}

	for(i = 0; i < j; i++)
	{
		struct norm_buff *t = d_hold_sp[i];
		if(!t)
			continue;
		buffer_clear(*t);
	}
}

bool init_udp(some_fd epoll_fd)
{
	unsigned i, sos_num, idx;
	bool ipv4_ready = false;
	bool ipv6_ready = false;

	/* set the cache seed */
	random_bytes_get(&cache.ht_seed, sizeof(cache.ht_seed));
	/* set the sequence seed */
	random_bytes_get(&sequence_seed, sizeof(sequence_seed));

	sos_num = server.settings.bind.ip4.num + server.settings.bind.ip6.num;
	udp_sos = malloc(sizeof(*udp_sos) * sos_num);
	if(!udp_sos) {
		logg_errno(LOGF_ERR, "couldn't allocate udp gups");
		return false;
	}

	/* sock-things */
	for(i = 0; i < sos_num; i++) {
		udp_sos[i].fd  = -1;
		udp_sos[i].gup = GUP_UDP;
		memset(&udp_sos[i].na, 0, sizeof(udp_sos[i].na));
	}

	idx = 0;
	/* Setting up the UDP Socket */
	if(server.settings.bind.use_ip4 && server.settings.bind.ip4.num)
	{
		ipv4_ready = true;
		for(i = 0; i < server.settings.bind.ip4.num; i++)
		{
			bool res = init_con_u(&udp_sos[idx].fd, &server.settings.bind.ip4.a[i]);
			if(res)
				memcpy(&udp_sos[idx++].na, &server.settings.bind.ip4.a[i], sizeof(udp_sos[0].na));
			ipv4_ready = ipv4_ready && res;
			if(!ipv4_ready)
				break;
		}
		if(!ipv4_ready)
		{
			while(i--) {
				my_epoll_closesocket(udp_sos[--idx].fd);
				memset(&udp_sos[idx].na, 0, sizeof(udp_sos[idx].na));
				udp_sos[idx].fd = -1;
			}
		}
	}
	if(server.settings.bind.use_ip6 && server.settings.bind.ip6.num)
	{
		ipv6_ready = true;
		for(i = 0; i < server.settings.bind.ip6.num; i++)
		{
			bool res = init_con_u(&udp_sos[idx].fd, &server.settings.bind.ip6.a[i]);
			if(res)
				memcpy(&udp_sos[idx++].na, &server.settings.bind.ip6.a[i], sizeof(udp_sos[0].na));
			ipv6_ready = ipv6_ready && res;
			if(!ipv6_ready)
				break;
		}
		if(!ipv6_ready)
		{
			while(i--) {
				my_epoll_closesocket(udp_sos[--idx].fd);
				memset(&udp_sos[idx].na, 0, sizeof(udp_sos[idx].na));
				udp_sos[idx].fd = -1;
			}
		}
	}

	if(server.settings.bind.use_ip4 && !ipv4_ready) {
		clean_up_udp();
		return false;
	}
	if(server.settings.bind.use_ip6 && !ipv6_ready)
	{
		if(ipv4_ready) {
			logg(LOGF_ERR, "Error starting IPv6, but will keep going!\n");
			server.settings.bind.use_ip6 = false;
		} else {
			clean_up_udp();
			return false;
		}
	}

	udp_sos_num = idx;
	/* Setting first entry to be polled, our Acceptsocket */
	for(i = 0; i < idx; i++)
	{
		struct epoll_event tmp_ev;

		if(udp_sos[i].fd == -1)
			continue; /* should not happen, break? */
		tmp_ev.events = (uint32_t)(EPOLLIN | EPOLLERR | EPOLLONESHOT);
		tmp_ev.data.u64 = 0;
		tmp_ev.data.ptr = &udp_sos[i];
		if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_sos[i].fd, &tmp_ev))
		{
			logg_errno(LOGF_ERR, "adding udp-socket to epoll");
			clean_up_udp();
			return false;
		}
	}

#ifdef UDP_DUMP
	{
	char tmp_nam[sizeof("./G2UDPincomming.bin") + 12];
	my_snprintf(tmp_nam, sizeof(tmp_nam), "./G2UDPincomming%lu.bin", (unsigned long)getpid());
	if(0 > (out_file = open(tmp_nam, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR))) {
		logg_errno(LOGF_ERR, "opening UDP-file");
		clean_up_udp();
		return false;
	}
	}
#endif

	return true;
}

static uint8_t *gnd_buff_prep(struct norm_buff *d_hold, uint16_t seq, uint8_t part, uint8_t count, uint8_t flags)
{
	uint8_t *num_ptr;

	buffer_clear(*d_hold);
	/* Tag */
	*buffer_start(*d_hold) = 'G';
	d_hold->pos++;
	*buffer_start(*d_hold) = 'N';
	d_hold->pos++;
	*buffer_start(*d_hold) = 'D';
	d_hold->pos++;
	/* flags */
	*buffer_start(*d_hold) = flags;
	d_hold->pos++;
	/* sequence */
	put_unaligned(seq, ((uint16_t *)buffer_start(*d_hold)));
	d_hold->pos += 2;
	/* part */
	num_ptr = (uint8_t *)buffer_start(*d_hold);
	*num_ptr = part;
	d_hold->pos++;
	/* count */
	*buffer_start(*d_hold) = (char)count;
	d_hold->pos++;

	return num_ptr;
}

static noinline uint16_t get_next_sequence_number(const union combo_addr *to)
{
	static atomica_t internal_sequence[SEQUENCE_SPACE];
	uint32_t h;

	/*
	 * If we just use a simple sequence counter, we "wrap" around very fast
	 * because the GND seq. is only 16 bit. With enough traffic this means we
	 * use the same sequence number again.
	 * On the one hand this is not a problem, sequence numbers are "extended"
	 * by our sending address. So if client A sees our-ip.1234 and client B sees
	 * our-ip.1234, they both can distinguish seq. 1234 from all other arriving
	 * at their interface by the IP.
	 * It only gets problematic if we send the SAME sequence to the SAME client
	 * in short succession. If external malicius traffic forces us into this
	 * situation we are screwed...
	 * If we pick the counter depending on the client address, we "spread" the
	 * increments (hopefully...) to get a slower increasing counter.
	 * But to not make them increase to slow (or you could guess the counter
	 * easily, because we do not have fancy per IP counter), we better do not
	 * use to much.
	 * With 16 counter we expand the counter to 20 bit.
	 */
// TODO: we need a cryptografically "strong" sequence number...
	/* every IP should have it's own independent sequence */
	h = combo_addr_hash(to, sequence_seed);
	return ((unsigned)atomic_inca_return(&internal_sequence[h % SEQUENCE_SPACE])) & 0x0000FFFF;
}

static noinline void udp_writeout_packet_uc(const union combo_addr *to, int fd, g2_packet_t *p, struct norm_buff *d_hold, size_t res_len)
{
	struct pointer_buff t_hold;
	unsigned int num_udp_packets, i;
	uint16_t sequence_number;
	uint8_t *num_ptr;
	bool is_v6 = combo_addr_is_v6(to);

	num_udp_packets = DIV_ROUNDUP(res_len, ((is_v6 ? UDP6_MTU : UDP_MTU) - UDP_RELIABLE_LENGTH));
	sequence_number = get_next_sequence_number(to);
	num_ptr = gnd_buff_prep(d_hold, sequence_number, 1, num_udp_packets, 0);

	t_hold.capacity = d_hold->capacity;
	t_hold.data     = d_hold->data;
	for(i = 0; i < num_udp_packets; i++)
	{
		d_hold->pos   = UDP_RELIABLE_LENGTH;
		d_hold->limit = is_v6 ? UDP6_MTU : UDP_MTU;
		*num_ptr = i+1;

		g2_packet_serialize_to_buff(p, d_hold);
		buffer_flip(*d_hold);
		t_hold.pos   = d_hold->pos;
		t_hold.limit = d_hold->limit;
		if(0 >= udp_sock_send(&t_hold, to, fd)) {
			logg_devel("UDP send loop failed\n");
			/* no fancy recovery, just break out of loop */
			break;
		}
	}

	if(p->packet_encode != ENCODE_FINISHED)
		logg_devel("encode not finished!\n");
}

static noinline void udp_writeout_packet_c(const union combo_addr *to, int fd, g2_packet_t *p, struct norm_buff *d_hold, size_t res_len)
{
	struct pointer_buff t_hold;
	unsigned int num_udp_packets, i;
	uint16_t sequence_number;
	uint8_t *num_ptr;
	size_t d_len, c_len;
	struct big_buff *c_data;
	struct zpad *zp = NULL;
	bool is_v6;

	zp = qht_get_zpad();
	if(!zp) {
		udp_writeout_packet_uc(to, fd, p, d_hold, res_len);
		return;
	}

	zp->z.next_in   = NULL;
	zp->z.avail_in  = 0;
	zp->z.next_out  = NULL;
	zp->z.avail_out = 0;
	if(deflateInit(&zp->z, Z_DEFAULT_COMPRESSION) != Z_OK) {
		logg_devel("defalteInit failed\n");
		udp_writeout_packet_uc(to, fd, p, d_hold, res_len);
		return;
	}

	c_data = udp_get_c_buff(deflateBound(&zp->z, res_len));
	if(!c_data) {
		deflateEnd(&zp->z);
		udp_writeout_packet_uc(to, fd, p, d_hold, res_len);
		return;
	}

	/* compress it */
	do
	{
		g2_packet_serialize_to_buff(p, d_hold);
		buffer_flip(*d_hold);
		zp->z.next_in    = (Bytef *)buffer_start(*d_hold);
		zp->z.avail_in   = buffer_remaining(*d_hold);
		zp->z.next_out   = (Bytef *)buffer_start(*c_data);
		zp->z.avail_out  = buffer_remaining(*c_data);

		switch(deflate(&zp->z, Z_NO_FLUSH))
		{
		case Z_OK:
			d_hold->pos += (buffer_remaining(*d_hold) - zp->z.avail_in);
			c_data->pos += (buffer_remaining(*c_data) - zp->z.avail_out);
			break;
		case Z_STREAM_END:
			logg_devel("Z_STREAM_END\n");
			goto out_zfree;
		case Z_NEED_DICT:
			logg_devel("Z_NEED_DICT\n");
			goto out_zfree;
		case Z_DATA_ERROR:
			logg_devel("Z_DATA_ERROR\n");
			goto out_zfree;
		case Z_STREAM_ERROR:
			logg_devel("Z_STREAM_ERROR\n");
			goto out_zfree;
		case Z_MEM_ERROR:
			logg_devel("Z_MEM_ERROR\n");
			goto out_zfree;
		case Z_BUF_ERROR:
			logg_devel("Z_BUF_ERROR\n");
			goto out_zfree;
		default:
			logg_devel("deflate was not Z_OK\n");
			goto out_zfree;
		}
		buffer_compact(*d_hold);
	} while(ENCODE_FINISHED != p->packet_encode);
	switch(deflate(&zp->z, Z_FINISH))
	{
	case Z_STREAM_END:
	case Z_OK:
		c_data->pos += (buffer_remaining(*c_data) - zp->z.avail_out);
		break;
	case Z_NEED_DICT:
		logg_devel("Z_NEED_DICT\n");
		goto out_zfree;
	case Z_DATA_ERROR:
		logg_devel("Z_DATA_ERROR\n");
		goto out_zfree;
	case Z_STREAM_ERROR:
		logg_devel("Z_STREAM_ERROR\n");
		goto out_zfree;
	case Z_MEM_ERROR:
		logg_devel("Z_MEM_ERROR\n");
		goto out_zfree;
	case Z_BUF_ERROR:
		logg_devel("Z_BUF_ERROR\n");
		goto out_zfree;
	default:
		logg_devel("deflate was not Z_OK\n");
		goto out_zfree;
	}
	buffer_flip(*c_data);
	buffer_clear(*d_hold);
	logg_develd_old("sending compressed %zu:%zu\n", res_len, buffer_remaining(*c_data));
	res_len = buffer_remaining(*c_data);

	is_v6 = combo_addr_is_v6(to);
	/* send it */
	num_udp_packets = DIV_ROUNDUP(res_len, ((is_v6 ? UDP6_MTU : UDP_MTU) - UDP_RELIABLE_LENGTH));
	sequence_number = get_next_sequence_number(to);
	num_ptr = gnd_buff_prep(d_hold, sequence_number, 1, num_udp_packets, FLAG_DEFLATE);

	i = 1;
	d_hold->pos   = UDP_RELIABLE_LENGTH;
	d_hold->limit = is_v6 ? UDP6_MTU : UDP_MTU;
	*num_ptr = i;

	d_len = buffer_remaining(*d_hold);
	c_len = buffer_remaining(*c_data);
	d_len = d_len < c_len ? d_len : c_len;
	my_memcpy(buffer_start(*d_hold), buffer_start(*c_data), d_len);
	d_hold->pos += d_len;
	c_data->pos += d_len;
	buffer_flip(*d_hold);
	t_hold.pos      = d_hold->pos;
	t_hold.limit    = d_hold->limit;
	t_hold.capacity = d_hold->capacity;
	t_hold.data     = d_hold->data;
	udp_sock_send(&t_hold, to, fd);

	if(i < num_udp_packets)
	{
		size_t o_limit;

		d_hold->pos     = 0;
		d_hold->limit   = UDP_RELIABLE_LENGTH;
		o_limit         = c_data->limit;
		t_hold.pos      = c_data->pos;
		t_hold.limit    = c_data->limit;
		t_hold.capacity = c_data->capacity;
		t_hold.data     = c_data->data;
		for(; i < num_udp_packets; i++)
		{
			*num_ptr = i+1;
			t_hold.pos -= UDP_RELIABLE_LENGTH;
			memcpy(buffer_start(t_hold), buffer_start(*d_hold), UDP_RELIABLE_LENGTH);

			t_hold.limit = o_limit;
			if(buffer_remaining(t_hold) > (is_v6 ? UDP6_MTU : UDP_MTU))
				t_hold.limit = t_hold.pos + (is_v6 ? UDP6_MTU : UDP_MTU);
			if(0 >= udp_sock_send(&t_hold, to, fd)) {
				logg_devel("compressed UDP send loop failed\n");
				/* no fancy recovery, just break out of loop to prevent underflow */
				break;
			}
		}
	}
out_zfree:
	deflateEnd(&zp->z);
}

static void udp_writeout_packet(const union combo_addr *to, int fd, g2_packet_t *p, struct norm_buff *d_hold)
{
	ssize_t result;

	result = g2_packet_serialize_prep_min(p);
	if(-1 == result) {
		logg_devel("serialize prepare failed\n");
		return;
	}
	if(result > 320)
		udp_writeout_packet_c(to, fd, p, d_hold, result);
	else
		udp_writeout_packet_uc(to, fd, p, d_hold, result);
}

static bool handle_udp_packet(struct norm_buff **d_hold_sp, union combo_addr *from, union combo_addr *to, some_fd answer_fd)
{
	gnd_packet_t tmp_packet;
	g2_packet_t g_packet_store, *g_packet;
	struct ptype_action_args parg;
	struct list_head answer;
	struct norm_buff *d_hold = *d_hold_sp;
	uint8_t tmp_byte;

	if(combo_addr_is_forbidden(from))
		return true;

	if(!buffer_remaining(*d_hold))
		return true;

	/* is it long enough to be a GNutella Datagram? */
	if(UDP_RELIABLE_LENGTH > buffer_remaining(*d_hold)) {
		logg_devel_old("really short packet recieved\n");
		return true;
	}

	/* is it a GND? */
	if('G' != *buffer_start(*d_hold) || 'N' != *(buffer_start(*d_hold)+1) || 'D' != *(buffer_start(*d_hold)+2))
		return true;
	d_hold->pos += 3;

	/* get the flags */
	tmp_byte = *buffer_start(*d_hold);
	d_hold->pos++;

	/*
	 * according to the specs, if there is a bit
	 * set in the lower nibble we can't assume
	 * any meaning too, discard the packet
	 */
	if(tmp_byte & FLAG_CRITICAL) {
		logg_develd("packet with other critical flags: 0x%02X\n", tmp_byte);
		return true;
	}
	tmp_packet.flags.deflate = (tmp_byte & FLAG_DEFLATE) ? true : false;
	tmp_packet.flags.ack_me = (tmp_byte & FLAG_ACK) ? true : false;

	/*
	 * get the packet-sequence-number,
	 * fortunately byte-sex doesn't matter, the
	 * numbers must only be different or same
	 */
	tmp_packet.sequence = get_unaligned((uint16_t *)buffer_start(*d_hold));
	d_hold->pos += sizeof(uint16_t);

	/* part */
	tmp_packet.part = *buffer_start(*d_hold);
	d_hold->pos++;

	/* count */
	tmp_packet.count = *buffer_start(*d_hold);
	d_hold->pos++;

/************ DEBUG *****************/
#ifdef UDP_DUMP
	{
	size_t old_remaining;
	size_t res_byte;
	ssize_t result;

	buffer_compact(*d_hold);
	old_remaining = d_hold->pos;

	res_byte = (size_t) my_snprintf(buffer_start(*d_hold), buffer_remaining(*d_hold),
		"\n----------\nseq: %u\tpart: %u/%u\ndeflate: %s\tack_me: %s\nsa_family: %i\nsin_addr:sin_port: %p#I\n----------\n",
		(unsigned)tmp_packet.sequence, (unsigned)tmp_packet.part, (unsigned)tmp_packet.count,
		(tmp_packet.flags.deflate) ? "true" : "false", (tmp_packet.flags.ack_me) ? "true" : "false", 
		from->s.fam, from);

		d_hold->pos += (res_byte > buffer_remaining(*d_hold)) ? buffer_remaining(*d_hold) : res_byte;

	buffer_flip(*d_hold);
	do
	{
		errno = 0;
		result = write(out_file, buffer_start(*d_hold), buffer_remaining(*d_hold));
	} while(-1 == result && EINTR == errno);

	switch(result)
	{
	default:
		d_hold->limit = old_remaining;
		break;
	case  0:
		if(buffer_remaining(*d_hold))
		{
			if(EAGAIN != errno)
			{
				logg_errnod(LOGF_ERR, "%s ERRNO=%i\tFDNum: %i",
					"EOF reached!",
					errno,
					out_file);
				return false;
			}
			else
				logg_devel("Nothing to write!\n");
		}
		return true;
	case -1:
		if(EAGAIN != errno)
		{
			logg_errnod(LOGF_ERR, "%s ERRNO=%i\tFDNum: %i",
				"write",
				errno,
				out_file);
			return false;
		}
		return true;
	}
	}
#endif
/*********** /DEBUG *****************/

	if(!tmp_packet.count) {
// TODO: Do an appropriate action on a recived ACK
		logg_devel_old("ACK recived!\n");
		return true;
	}

	if(!buffer_remaining(*d_hold)) /* is there really any data */
		goto out;

	if(buffer_remaining(*d_hold) >= 3 &&
	   unlikely('G' == *buffer_start(*d_hold) &&
	            'N' == *(buffer_start(*d_hold)+1) &&
	            'D' == *(buffer_start(*d_hold)+2))) {
		logg_develd_old("received double GND from %pI#\n", from);
		return true;
	}

	if(tmp_packet.count < tmp_packet.part) {
		logg_devel_old("broken UDP packet part nr. > part count \n");
		return true;
	}

	/* ack early to not run into timeouts */
	if(tmp_packet.flags.ack_me)
	{
		struct norm_buff *t_hold = udp_get_lsbuf();
		if(t_hold)
		{
			struct pointer_buff x_hold;
			gnd_buff_prep(t_hold, tmp_packet.sequence, tmp_packet.part, 0, 0);
			buffer_flip(*t_hold);
			x_hold.pos      = t_hold->pos;
			x_hold.limit    = t_hold->limit;
			x_hold.capacity = t_hold->capacity;
			x_hold.data     = t_hold->data;
			udp_sock_send(&x_hold, from, answer_fd);
			tmp_packet.flags.ack_me = false;
		}
	}

	g2_packet_init_on_stack(&g_packet_store);
	if(tmp_packet.count > 1)
	{
		logg_devel_old("multipart UDP packet, needs reassamble\n");

		g_packet = g2_udp_reas_add(&tmp_packet, d_hold_sp, from, &g_packet_store);
		if(!g_packet)
			goto out;
	}
	else
	{
		g_packet = &g_packet_store;
		if(tmp_packet.flags.deflate)
		{
			struct big_buff *d_hold_n;
			struct zpad *zp;
			int z_status;
			bool first_data = true;

			d_hold_n = udp_get_lubuf();
			if(!d_hold_n)
				goto out;

			/*
			 * because we do not want to receive QHTs by UDP, we can
			 * share the zlib TLS alloc stuff with the QHT handling.
			 * And even if, it's no problem, inflating should be finished
			 * (at least ATM) afterwards and QHT can reuse the pad.
			 */
			zp = qht_get_zpad();
			if(!zp)
				goto out;

			zp->z.next_in   = (Bytef *)buffer_start(*d_hold);
			zp->z.avail_in  = buffer_remaining(*d_hold);
			zp->z.next_out  = NULL;
			zp->z.avail_out = 0;

			if(inflateInit(&zp->z) != Z_OK) {
				logg_devel("infalteInit failed\n");
				goto out;
			}

			do
			{
				zp->z.next_out  = (Bytef *)buffer_start(*d_hold_n);
				zp->z.avail_out = buffer_remaining(*d_hold_n);
				z_status = inflate(&zp->z, Z_SYNC_FLUSH);
				switch(z_status)
				{
				case Z_OK:
				case Z_STREAM_END:
					d_hold->pos += buffer_remaining(*d_hold) - zp->z.avail_in;
					d_hold_n->pos += buffer_remaining(*d_hold_n) - zp->z.avail_out;
					break;
				case Z_NEED_DICT:
					logg_devel("Z_NEED_DICT\n");
					goto out_free;
				case Z_DATA_ERROR:
					logg_devel("Z_DATA_ERROR\n");
					goto out_free;
				case Z_STREAM_ERROR:
					logg_devel("Z_STREAM_ERROR\n");
					goto out_free;
				case Z_MEM_ERROR:
					logg_devel("Z_MEM_ERROR\n");
					goto out_free;
				case Z_BUF_ERROR:
					logg_devel("Z_BUF_ERROR\n");
					goto out_free;
				default:
					logg_devel("inflate was not Z_OK\n");
					goto out_free;
				}
				buffer_flip(*d_hold_n);
				if(first_data) {
					if(buffer_remaining(*d_hold_n) >= 3 &&
					   unlikely('G' == *buffer_start(*d_hold_n) &&
					            'N' == *(buffer_start(*d_hold_n)+1) &&
					            'D' == *(buffer_start(*d_hold_n)+2))) {
						logg_develd_old("received double GND from %pI# after unpack\n", from);
						goto out;
					}
					first_data = false;
				}

				/* Look at the packet received */
				if(!g2_packet_extract_from_stream_b(d_hold_n, g_packet, server.settings.max_g2_packet_length, false)) {
					logg_devel("packet extract failed\n");
					goto out_free;
				}

				if(DECODE_FINISHED == g_packet->packet_decode ||
				   PACKET_EXTRACTION_COMPLETE == g_packet->packet_decode) {
					break;
				}
				buffer_compact(*d_hold_n);
			} while(buffer_remaining(*d_hold));
			inflateEnd(&zp->z);
		}
		else
		{
			/* Look at the packet received */
			if(!g2_packet_extract_from_stream(d_hold, g_packet, buffer_remaining(*d_hold), false)) {
				logg_devel("packet extract failed\n");
				goto out;
			}

		}

		if(!(DECODE_FINISHED == g_packet->packet_decode ||
		     PACKET_EXTRACTION_COMPLETE == g_packet->packet_decode)) {
			logg_develd("packet extract stuck in wrong state: \"%s\"\n",
			            g2_packet_decoder_state_name(g_packet->packet_decode));
			goto out_free;
		}
	}

	INIT_LIST_HEAD(&answer);
	parg.connec      = NULL;
	parg.father      = NULL;
	parg.source      = g_packet;
	parg.src_addr    = from;
	parg.dst_addr    = to;
	parg.target_lock = NULL;
	parg.target      = &answer;
	parg.opaque      = NULL;
	if(g2_packet_decide_spec(&parg, g2_packet_dict_udp))
	{
		struct list_head *e, *n;
		struct norm_buff *s_buff;

// TODO: we already acked the packet, OK, but what to do with PI?
// TODO: check for failure
		s_buff = udp_get_lsbuf();
		 /* try to answer */
		list_for_each_safe(e, n, &answer)
		{
			g2_packet_t *entry = list_entry(e, g2_packet_t, list);
			buffer_clear(*s_buff);
			udp_writeout_packet(from, answer_fd, entry, s_buff);
			list_del(e);
			g2_packet_free(entry);
		}
	}

out_free:
	g2_packet_free(g_packet);
out:
	if(tmp_packet.flags.ack_me)
	{
		struct norm_buff *t_hold = udp_get_lsbuf();
		if(t_hold)
		{
			struct pointer_buff x_hold;
			gnd_buff_prep(t_hold, tmp_packet.sequence, tmp_packet.part, 0, 0);
			buffer_flip(*t_hold);
			x_hold.pos      = t_hold->pos;
			x_hold.limit    = t_hold->limit;
			x_hold.capacity = t_hold->capacity;
			x_hold.data     = t_hold->data;
			udp_sock_send(&x_hold, from, answer_fd);
		}
	}

	return true;
}

void g2_udp_send(const union combo_addr *to, const union combo_addr *from, struct list_head *answer)
{
	struct list_head *e, *n;
	struct norm_buff *d_hold;
	some_fd answer_fd = -1;
	some_fd fallback_fd = -1;
	some_fd fallback_fallback_fd = -1;
	unsigned i;

	/* find a suitable send fd */
	for(i = 0; i < udp_sos_num; i++)
	{
		if(udp_sos[i].na.s.fam != to->s.fam)
			continue;
		if(combo_addr_eq_ip(&udp_sos[i].na, from)) {
			answer_fd = udp_sos[i].fd;
			break;
		}
		if(combo_addr_eq_any(&udp_sos[i].na) && -1 != udp_sos[i].fd)
			fallback_fd = udp_sos[i].fd;
		if(-1 != udp_sos[i].fd)
			fallback_fallback_fd = udp_sos[i].fd;
	}

	if(-1 == answer_fd)
	{
		if(-1 == fallback_fd)
		{
			if(-1 == fallback_fallback_fd) {
				/* now we are really screwed */
				logg_devel("trying to send to an address we have no fd for!?");
				goto out_err;
			}
			/* we have grabed _some_ fd, it may not be suitable */
			fallback_fd = fallback_fallback_fd; /* but at least we tried... */
		}
		answer_fd = fallback_fd;
	}

	d_hold = udp_get_lsbuf();
	if(!d_hold)
		goto out_err;

	 /* try to send answer */
	list_for_each_safe(e, n, answer)
	{
		g2_packet_t *entry = list_entry(e, g2_packet_t, list);
		buffer_clear(*d_hold);
		udp_writeout_packet(to, answer_fd, entry, d_hold);
		list_del_init(e);
		g2_packet_free(entry);
	}
	return;

out_err:
	list_for_each_safe(e, n, answer) {
		g2_packet_t *entry = list_entry(e, g2_packet_t, list);
		list_del_init(e);
		g2_packet_free(entry);
	}
}

static ssize_t udp_sock_send(struct pointer_buff *d_hold, const union combo_addr *to, some_fd fd)
{
	ssize_t result;
	union combo_addr to_st;

	if(unlikely(combo_addr_is_forbidden(to))) {
		buffer_skip(*d_hold);
		return 0;
	}
	if(unlikely(!combo_addr_port(to))) {
// TODO: investigate this phenomenom
		logg_develd_old("addr %p#I without port! Fallback...\n", to);
		to_st = *to;
		combo_addr_set_port(&to_st, htons(6346));
		to = &to_st;
	}
	do
	{
		set_s_errno(0);
/* ssize_t sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen); */
		result = my_epoll_sendto(fd,
			buffer_start(*d_hold),
			buffer_remaining(*d_hold),
			0,
			casac(to),
			sizeof(*to));
	} while(unlikely(-1 == result) && EINTR == s_errno);

	switch(result)
	{
	default:
		d_hold->pos += result;
		if(buffer_remaining(*d_hold))
			logg_devel("partial write");
		break;
	case  0:
		if(buffer_remaining(*d_hold))
		{
			if(EAGAIN != s_errno) {
				logg_posd(LOGF_ERR, "%s ERRNO=%i\tFDNum: %i\tFromIp: %p#I\n",
				         "error writing?!", s_errno, fd, to);
				return -1;
			} else
				logg_devel("Nothing to write!\n");
		}
		break;
	case -1:
		if(EAGAIN != s_errno) {
			logg_serrnod(LOGF_ERR, "%s ERRNO=%i\tFDNum: %i to: %p#I",
			            "sendto:", s_errno, fd, to);
			return -1;
		}
		break;
	}
	return result;
}

static inline ssize_t handle_udp_sock(struct epoll_event *udp_poll, struct norm_buff **d_hold, union combo_addr *from, union combo_addr *to, some_fd from_fd, unsigned l)
{
	struct mfromto m[l];
	ssize_t result;
	unsigned i;

	if(udp_poll->events & ~(EPOLLIN|EPOLLOUT)) {
		/* If our udp sock is not ready reading or writing -> fuzz */
		logg_pos(LOGF_ERR, "udp_so NVAL|ERR|HUP\n");
		return -1;
	}

	if(!(udp_poll->events & EPOLLIN))
		return 0;

	for(i = 0; i < l; i++)
	{
		casalen_ib(&from[i]); casalen_ib(&to[i]);
		m[i].iov.iov_base = buffer_start(*(d_hold[i]));
		m[i].iov.iov_len  = buffer_remaining(*(d_hold[i]));
		m[i].from         = casa(&from[i]);
		m[i].to           = casa(&to[i]);
		m[i].from_len     = sizeof(from[0]);
		m[i].to_len       = sizeof(to[0]);
	}

	do
	{
		set_s_errno(0);
		/* ssize_t recvmfromto(int s, struct mfromto *info, size_t len, int flags) */
		result = recvmfromto(from_fd, m, l, 0);
	} while(unlikely(-1 == result) && EINTR == s_errno);

	if(result < 0) {
		logg_serrnod(LOGF_ERR, "%s ERRNO=%i\tFDNum: %i", "recvmfromto:", s_errno, from_fd);
		result = -(result + 1);
	}

	for(i = 0; i < (unsigned)result; i++) {
		logg_devel_old("Data read from socket\n");
		d_hold[i]->pos += m[i].iov.iov_len;
	}
	return result;
}

#define OUT_ERR(x)	do {e = x; goto out_err;} while(0)
static inline bool init_con_u(some_fd *udp_so, union combo_addr *our_addr)
{
	const char *e;
	int yes = 1; /* for setsockopt() SO_REUSEADDR, below */

	if(-1 == (*udp_so = my_epoll_socket(our_addr->s.fam, SOCK_DGRAM, 0))) {
		logg_serrno(LOGF_ERR, "creating socket");
		return false;
	}

	if(udpfromto_init(*udp_so, our_addr->s.fam))
		OUT_ERR("preparing UDP ip recv");

	/* lose the pesky "address already in use" error message */
	if(my_epoll_setsockopt(*udp_so, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(int)))
		OUT_ERR("setsockopt reuse");

#if HAVE_DECL_IPV6_V6ONLY == 1
	if(AF_INET6 == our_addr->s.fam && server.settings.bind.use_ip4) {
		if(my_epoll_setsockopt(*udp_so, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)))
			OUT_ERR("setsockopt V6ONLY");
	}
#endif

	if(my_epoll_bind(*udp_so, casa(our_addr), casalen(our_addr)))
		OUT_ERR("bindding udp fd");

	/*
	 * UDP and non-blocking??
	 * Isn't fantastic, but we hope we only get whole msg.
	 * This way we can "cycle" to pull several msg at once
	 * from the socket.
	 */
	if(my_epoll_fcntl(*udp_so, F_SETFL, O_NONBLOCK))
		OUT_ERR("udp non-blocking");

	return true;

out_err:
	logg_serrnod(LOGF_ERR, "%s", e);
	my_epoll_closesocket(*udp_so);
	*udp_so = -1;
	return false;
}

void clean_up_udp(void)
{
	unsigned i;

	for(i = 0; i < udp_sos_num; i++) {
		if(udp_sos[i].fd > 2) /* do not close fd 0,1,2 by accident */
			my_epoll_closesocket(udp_sos[i].fd);
	}

	if(0 <= out_file)
		close(out_file);
}

/*@unsused@*/
static char const rcsid_u[] GCC_ATTR_USED_VAR = "$Id: G2UDP.c,v 1.16 2005/11/05 10:03:50 redbully Exp redbully $";
/* EOF */
