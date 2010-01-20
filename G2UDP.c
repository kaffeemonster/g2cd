/*
 * G2UDP.c
 * thread to handle the UDP-part of the G2-Protocol
 *
 * Copyright (c) 2004-2010 Jan Seiffert
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
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
/* System net-includes */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
#include "lib/combo_addr.h"
#include "lib/ansi_prng.h"

#define UDP_MTU        500
#define UDP_RELIABLE_LENGTH 8
#define UDP_MAX_PARTS (sizeof(uint32_t) * BITS_PER_CHAR)

#define FLAG_DEFLATE   (1 << 0)
#define FLAG_ACK       (1 << 1)
#define FLAG_CRITICAL  ((~(FLAG_DEFLATE|FLAG_ACK)) & 0x0F)

/*
 * UDP reassambly Cache
 */
#define UDP_CACHE_SHIFT 8
#define UDP_CACHE_SIZE (1 << UDP_CACHE_SHIFT)
#define UDP_CACHE_HTSIZE (UDP_CACHE_SIZE/8)
#define UDP_CACHE_HTMASK (UDP_CACHE_HTSIZE-1)
#define UDP_REAS_TIMEOUT 30

/* Types */
struct udp_reas_entry
{
	uint32_t parts_recv;
	uint32_t parts_consumed;

	uint16_t part_count;
	uint16_t sequence;
	time_t   when;
	bool     deflate;
	union combo_addr na;

	struct norm_buff *parts_buf[UDP_MAX_PARTS];
	g2_packet_t *result_packet;
};

struct udp_reas_cache_entry
{
	struct rb_node rb;
	struct hlist_node node;
	struct udp_reas_entry e;
};

/* internal prototypes */
static ssize_t udp_sock_send(struct norm_buff *, const union combo_addr *, int);
static inline bool handle_udp_sock(struct epoll_event *, struct norm_buff *, union combo_addr *, union combo_addr *, int);
static noinline bool handle_udp_packet(struct norm_buff **, union combo_addr *, union combo_addr *, int);
static inline bool init_con_u(int *, union combo_addr *);

#define G2UDP_FD_SUM 3

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
static int udp_outfd_ipv4;
static int udp_outfd_ipv6;
static struct simple_gup udp_sos[2];

#ifndef HAVE___THREAD
static pthread_key_t key2udp_lsb;
static pthread_key_t key2udp_lub;
#else
static __thread struct norm_buff *udp_lsbuffer;
static __thread struct norm_buff *udp_lubuffer;
#endif
/* protos */
	/* you better not kill this prot, our it won't work ;) */
static void udp_init(void) GCC_ATTR_CONSTRUCT;
static void udp_deinit(void) GCC_ATTR_DESTRUCT;

static void udp_init(void)
{
	size_t i;
#ifndef HAVE___THREAD
	if(pthread_key_create(&key2udp_lsb, (void(*)(void *))recv_buff_free))
		diedie("couln't create TLS key for local UDP send buffer");
	if(pthread_key_create(&key2udp_lub, (void(*)(void *))recv_buff_free))
		diedie("couln't create TLS key for local UDP uncompress buffer");
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
		buffer_clear(*ret_buf);
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

static struct norm_buff *udp_get_lubuf(void)
{
	struct norm_buff *ret_buf;
#ifndef HAVE___THREAD
	ret_buf = pthread_getspecific(key2udp_lub);
#else
	ret_buf = udp_lubuffer;
#endif
	if(likely(ret_buf)) {
		buffer_clear(*ret_buf);
		return ret_buf;
	}

	ret_buf = recv_buff_alloc();

	if(!ret_buf) {
		logg_devel("no local udp uncompress buffer\n");
		return NULL;
	}

#ifndef HAVE___THREAD
	pthread_setspecific(key2udp_lub, ret_buf);
#else
	udp_lubuffer = ret_buf;
#endif

	return ret_buf;
}

/****************** UDP reassambly Cache funcs ******************/

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
	struct udp_reas_entry *u = &e->e;
	size_t i;

	for(i = 0; i < UDP_MAX_PARTS; i++) {
		if(u->parts_buf)
			recv_buff_local_ret(u->parts_buf[i]);
	}
	g2_packet_free(e->e.result_packet);
	memset(e, 0, sizeof(*e));
	hlist_add_head(&e->node, &cache.free_list);
}

static void udp_reas_cache_entry_free_glob(struct udp_reas_cache_entry *e)
{
	struct udp_reas_entry *u = &e->e;
	size_t i;

	for(i = 0; i < UDP_MAX_PARTS; i++) {
		if(u->parts_buf)
			recv_buff_free(u->parts_buf[i]);
	}
	g2_packet_free_glob(e->e.result_packet);
	memset(e, 0, sizeof(*e));
	hlist_add_head(&e->node, &cache.free_list);
}

static uint32_t cache_ht_hash(const union combo_addr *addr, uint16_t seq)
{
	uint32_t h;

// TODO: when IPv6 is common, change it
	if(likely(addr->s_fam == AF_INET))
		h = hthash_3words(addr->in.sin_addr.s_addr, addr->in.sin_port, seq, cache.ht_seed);
	else
		h = hthash_6words(addr->in6.sin6_addr.s6_addr32[0],
		                  addr->in6.sin6_addr.s6_addr32[1],
		                  addr->in6.sin6_addr.s6_addr32[2],
		                  addr->in6.sin6_addr.s6_addr32[3],
		                  addr->in6.sin6_port, seq, cache.ht_seed);
	return h;
}

static struct udp_reas_cache_entry *cache_ht_lookup(const union combo_addr *addr, uint16_t seq, uint32_t h)
{
	struct hlist_node *n;
	struct udp_reas_cache_entry *e;

// TODO: check for mapped ip addr?
// TODO: when IPv6 is common, change it
// TODO: leave out port?
	if(likely(addr->s_fam == AF_INET))
	{
		hlist_for_each_entry(e, n, &cache.ht[h & UDP_CACHE_HTMASK], node)
		{
			if(e->e.na.s_fam != AF_INET)
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
			if(e->e.na.s_fam != AF_INET6)
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
	if((ret = (int)a->e.na.s_fam - (int)b->e.na.s_fam))
		return ret;
	if(likely(AF_INET == a->e.na.s_fam))
	{
		if((ret = (long)a->e.na.in.sin_addr.s_addr - (long)b->e.na.in.sin_addr.s_addr))
			return ret;
		if((ret = (int)a->e.na.in.sin_port - (int)b->e.na.in.sin_port))
			return ret;
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
		if((ret = (int)a->e.na.in6.sin6_port - (int)b->e.na.in6.sin6_port))
			return ret;
	}
	return (int)a->e.sequence - (int)b->e.sequence;
}

static noinline bool udp_reas_rb_cache_insert(struct udp_reas_cache_entry *e)
{
	struct rb_node **p = &cache.tree.rb_node;
	struct rb_node *parent = NULL;

	while(*p)
	{
		struct udp_reas_cache_entry *n = rb_entry(*p, struct udp_reas_cache_entry, rb);
		int result = udp_reas_entry_cmp(e, n);

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

void g2_udp_reas_timeout(void)
{
	struct udp_reas_cache_entry *e;
	time_t to_time;

	to_time = local_time_now - UDP_REAS_TIMEOUT;
	if(unlikely(pthread_mutex_lock(&cache.lock)))
		return;

	for(e = udp_reas_cache_last(); e; e = udp_reas_cache_last())
	{
		if(e->e.when < to_time) {
			udp_reas_rb_cache_remove(e);
			udp_reas_cache_entry_free_glob(e);
		} else
			break;
	}

	if(unlikely(pthread_mutex_unlock(&cache.lock)))
		diedie("udp reassambly lock stuck, sh**!");
}

static noinline g2_packet_t *g2_udp_reas_add(gnd_packet_t *p, struct norm_buff **d_hold, const union combo_addr *addr, g2_packet_t *st_pack)
{
	struct udp_reas_cache_entry *e;
	g2_packet_t *ret_val = NULL, *g_packet;
	struct norm_buff *d_hold_u = NULL;
	struct zpad *zp = NULL;
	uint32_t h;
	uint16_t seq;
	int i;
	time_t when;

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
			goto out_free; /* free and out here */
		}
		if(e->e.part_count != p->count) {
			logg_devel("udp packet changed part count?\n");
			goto out_free; /* free and out here */
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
//TODO: add blacklist entry for salvaged reas
				logg_devel_old("found older entry\n");
				if(!udp_reas_rb_cache_remove(e))
					goto out_unlock; /* the tree is amiss? */
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
	e->e.when = when;

	e->e.parts_recv |= 1 << (p->part - 1);
	e->e.parts_buf[p->part - 1] = *d_hold;
	*d_hold = NULL;
	if(((1 << e->e.part_count) - 1) ^ e->e.parts_recv)
		goto out_insert;

	/* all parts received */
	g_packet = st_pack;
	if(e->e.deflate)
	{
		logg_devel("compressed packet recevied\n");
		d_hold_u = udp_get_lubuf();
		if(!d_hold_u)
			goto out_free;

		/*
		 * because we do not want to receive QHTs by UDP, we can
		 * share the zlib TLS alloc stuff with the QHT handling.
		 * And even if, it's no problem, inflating should be finished
		 * (at least ATM) afterwards and QHT can reuse the pad.
		 */
		zp = qht_get_zpad();
		if(!zp)
			goto out_free;

		zp->z.next_in  = NULL;
		zp->z.avail_in = 0;
		if(inflateInit(&zp->z) != Z_OK) {
			logg_devel("infalteInit failed\n");
			goto out_zfree;
		}
	}

	for(i = 0; i < e->e.part_count; i++)
	{
		struct norm_buff *src_r, *src;

		src_r = e->e.parts_buf[i];
		do
		{
			if(e->e.deflate)
			{
				int z_status;

				src = d_hold_u;
				buffer_clear(*src);
				zp->z.next_in   = (Bytef *)buffer_start(*src_r);
				zp->z.avail_in  = buffer_remaining(*src_r);
				zp->z.next_out  = (Bytef *)buffer_start(*src);
				zp->z.avail_out = buffer_remaining(*src);

				z_status = inflate(&zp->z, Z_SYNC_FLUSH);
				switch(z_status)
				{
				case Z_OK:
				case Z_STREAM_END:
					src_r->pos += buffer_remaining(*src_r) - zp->z.avail_in;
					src->pos += buffer_remaining(*src) - zp->z.avail_out;
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
					logg_devel("inflate was not Z_OK\n");
					goto out_zfree;
				}
				buffer_flip(*src);
			}
			else
				src = src_r;
			/*
			 * Look at the packet received
			 */
			if(!g2_packet_extract_from_stream(src, g_packet, server.settings.default_max_g2_packet_length, true)) {
				logg_devel("packet extract failed\n");
				goto out_zfree;
			}

			if(DECODE_FINISHED == g_packet->packet_decode ||
			   PACKET_EXTRACTION_COMPLETE == g_packet->packet_decode) {
				ret_val = g_packet;
				goto out_zfree;
			}
			if(buffer_remaining(*src)) {
				logg_devel("couldn't remove all data from buffer\n");
				goto out_zfree;
			}
		} while(buffer_remaining(*src_r));
		recv_buff_local_ret(e->e.parts_buf[i]);
		e->e.parts_buf[i] = NULL;
	}
	logg_devel("incomplete packet in reas\n");
	goto out_zfree;

out_insert:
	if(!udp_reas_rb_cache_insert(e))
	{
		logg_devel("insert failed\n");
out_zfree:
		if(e->e.deflate && zp)
			inflateEnd(&zp->z);
out_free:
life_tree_error:
		udp_reas_cache_entry_free(e);
	}
	else
		cache_ht_add(e, h);

out_unlock:
	if(unlikely(pthread_mutex_unlock(&cache.lock)))
		diedie("ahhhh, udp reassembly cache lock stuck, bye!");
	return ret_val;
}

/************************* UDP work funcs ***********************/

void handle_udp(struct epoll_event *ev, struct norm_buff **d_hold_sp,  int epoll_fd)
{
	/* other variables */
	struct simple_gup *sg = ev->data.ptr;
	struct norm_buff *d_hold = *d_hold_sp;
	union combo_addr from, to;

	buffer_clear(*d_hold);
	if(!handle_udp_sock(ev, d_hold, &from, &to, sg->fd)) {
		/* bad things */ ;
// TODO: handle bad things
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

	buffer_flip(*d_hold);
	/* if we reach here, we know that there is at least no error or the logic above failed... */
	handle_udp_packet(d_hold_sp, &from, &to, sg->fd);
	if(*d_hold_sp)
		buffer_clear(**d_hold_sp);
}

bool init_udp(int epoll_fd)
{
	char tmp_nam[sizeof("./G2UDPincomming.bin") + 12];
	bool ipv4_ready;
	bool ipv6_ready;

	/* set the cache seed */
	random_bytes_get(&cache.ht_seed, sizeof(cache.ht_seed));

	udp_outfd_ipv4 = udp_sos[0].fd = -1;
	udp_outfd_ipv6 = udp_sos[1].fd = -1;
	/* Setting up the UDP Socket */
	ipv4_ready = server.settings.bind.use_ip4 ? init_con_u(&udp_sos[0].fd, &server.settings.bind.ip4) : false;
	ipv6_ready = server.settings.bind.use_ip6 ? init_con_u(&udp_sos[1].fd, &server.settings.bind.ip6) : false;
	udp_outfd_ipv4 = udp_sos[0].fd;
	udp_outfd_ipv6 = udp_sos[1].fd;
	udp_sos[0].gup = GUP_UDP;
	udp_sos[1].gup = GUP_UDP;

	if(server.settings.bind.use_ip4 && !ipv4_ready) {
		clean_up_udp();
		return false;
	}
	if(server.settings.bind.use_ip6 && !ipv6_ready)
	{
		if(ipv4_ready)
			logg(LOGF_ERR, "Error starting IPv6, but will keep going!\n");
		else {
			clean_up_udp();
			return false;
		}
	}
	if(udp_sos[0].fd > -1)
	{
		struct epoll_event tmp_ev;
		tmp_ev.events = (uint32_t)(EPOLLIN | EPOLLERR | EPOLLONESHOT);
		tmp_ev.data.u64 = 0;
		tmp_ev.data.ptr = &udp_sos[0];
		if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_sos[0].fd, &tmp_ev))
		{
			logg_errno(LOGF_ERR, "adding udp-socket to epoll");
			clean_up_udp();
			return false;
		}
	}
	if(udp_sos[1].fd > -1)
	{
		struct epoll_event tmp_ev;
		tmp_ev.events = (uint32_t)(EPOLLIN | EPOLLERR | EPOLLONESHOT);
		tmp_ev.data.u64 = 0;
		tmp_ev.data.ptr = &udp_sos[1];
		if(0 > my_epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_sos[1].fd, &tmp_ev))
		{
			logg_errno(LOGF_ERR, "adding udp-socket to epoll");
			clean_up_udp();
			return false;
		}
	}

	my_snprintf(tmp_nam, sizeof(tmp_nam), "./G2UDPincomming%lu.bin", (unsigned long)getpid());
	if(0 > (out_file = open(tmp_nam, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR))) {
		logg_errno(LOGF_ERR, "opening UDP-file");
		clean_up_udp();
		return false;
	}

	return true;
}

static uint8_t *gnd_buff_prep(struct norm_buff *d_hold, uint16_t seq, uint8_t part, uint8_t count)
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
// TODO: generate flags
	*buffer_start(*d_hold) = 0;
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

static void udp_writeout_packet(const union combo_addr *to, int fd, g2_packet_t *p, struct norm_buff *d_hold)
{
	static atomic_t internal_sequence;
	ssize_t result;
	unsigned int num_udp_packets, i;
	uint16_t sequence_number;
	uint8_t *num_ptr;

	result = g2_packet_serialize_prep_min(p);
	if(-1 == result) {
		logg_devel("serialize prepare failed\n");
		return;
	}
	num_udp_packets = DIV_ROUNDUP(result, (UDP_MTU - UDP_RELIABLE_LENGTH));
	sequence_number = ((unsigned)atomic_inc_return(&internal_sequence)) & 0x0000FFFF;
	num_ptr = gnd_buff_prep(d_hold, sequence_number, 1, num_udp_packets);

	for(i = 0; i < num_udp_packets; i++)
	{
		d_hold->pos   = UDP_RELIABLE_LENGTH;
		d_hold->limit = UDP_MTU;
		*num_ptr = i+1;

		g2_packet_serialize_to_buff(p, d_hold);
		udp_sock_send(d_hold, to, fd);
	}

	if(p->packet_encode != ENCODE_FINISHED)
		logg_devel("encode not finished!\n");
}

static bool handle_udp_packet(struct norm_buff **d_hold_sp, union combo_addr *from, union combo_addr *to, int answer_fd)
{
	gnd_packet_t tmp_packet;
	g2_packet_t g_packet_store, *g_packet;
	struct ptype_action_args parg;
	struct list_head answer;
	struct norm_buff *d_hold = *d_hold_sp;
	size_t res_byte;
	uint8_t tmp_byte;

	if(!buffer_remaining(*d_hold))
		return true;

	/* is it long enough to be a GNutella Datagram? */
	if(UDP_RELIABLE_LENGTH > buffer_remaining(*d_hold)) {
		logg_devel("really short packet recieved\n");
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
	{
	size_t old_remaining;
	ssize_t result;

	buffer_compact(*d_hold);
	old_remaining = d_hold->pos;

	res_byte = (size_t) my_snprintf(buffer_start(*d_hold), buffer_remaining(*d_hold),
		"\n----------\nseq: %u\tpart: %u/%u\ndeflate: %s\tack_me: %s\nsa_family: %i\nsin_addr:sin_port: %p#I\n----------\n",
		(unsigned)tmp_packet.sequence, (unsigned)tmp_packet.part, (unsigned)tmp_packet.count,
		(tmp_packet.flags.deflate) ? "true" : "false", (tmp_packet.flags.ack_me) ? "true" : "false", 
		from->s_fam, from);

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
		else
		{
			//putchar('+');
			//p_entry->events &= ~POLLIN;
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
/************ DEBUG *****************/

	if(!tmp_packet.count) {
// TODO: Do an appropriate action on a recived ACK
		logg_devel_old("ACK recived!\n");
		return true;
	}

	if(buffer_remaining(*d_hold)) /* is there really any data */
		goto out;

	if(tmp_packet.count > UDP_MAX_PARTS) {
		logg_devel("to many parts to reassamble packet\n");
		goto out;
	}

	if(tmp_packet.count < tmp_packet.part) {
		logg_devel("broken UDP packet part nr. > part count \n");
		goto out;
	}

	if(tmp_packet.flags.ack_me)
	{
		struct norm_buff *t_hold = udp_get_lsbuf();
		if(t_hold) {
			gnd_buff_prep(t_hold, tmp_packet.sequence, tmp_packet.part, 0);
			udp_sock_send(t_hold, from, answer_fd);
			tmp_packet.flags.ack_me = false;
		}
	}

// TODO: Handle multipart UDP packets
	g2_packet_init_on_stack(&g_packet_store);
	if(tmp_packet.count > 1)
	{
		logg_devel("multipart UDP packet, needs reassamble\n");
		g_packet = g2_udp_reas_add(&tmp_packet, d_hold_sp, from, &g_packet_store);
		if(!g_packet)
			goto out;
	}
	else
	{
		if(tmp_packet.flags.deflate)
		{
			struct norm_buff *d_hold_n;
			struct zpad *zp;
			int z_status;

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

			zp->z.next_in = (Bytef *)buffer_start(*d_hold);
			zp->z.avail_in = buffer_remaining(*d_hold);
			zp->z.next_out = (Bytef *)buffer_start(*d_hold_n);
			zp->z.avail_out = buffer_remaining(*d_hold_n);

			if(inflateInit(&zp->z) != Z_OK) {
				logg_devel("infalteInit failed\n");
				goto out;
			}
			z_status = inflate(&zp->z, Z_SYNC_FLUSH);
			inflateEnd(&zp->z);
			switch(z_status)
			{
			case Z_OK:
			case Z_STREAM_END:
				d_hold->pos += buffer_remaining(*d_hold) - zp->z.avail_in;
				d_hold_n->pos += buffer_remaining(*d_hold_n) - zp->z.avail_out;
				break;
			case Z_NEED_DICT:
				logg_devel("Z_NEED_DICT\n");
				goto out;
			case Z_DATA_ERROR:
				logg_devel("Z_DATA_ERROR\n");
				goto out;
			case Z_STREAM_ERROR:
				logg_devel("Z_STREAM_ERROR\n");
				goto out;
			case Z_MEM_ERROR:
				logg_devel("Z_MEM_ERROR\n");
				goto out;
			case Z_BUF_ERROR:
				logg_devel("Z_BUF_ERROR\n");
				goto out;
			default:
				logg_devel("inflate was not Z_OK\n");
				goto out;
			}
			buffer_flip(*d_hold_n);
			if(buffer_remaining(*d_hold))
				logg_develd("not all data consumed! %zu", buffer_remaining(*d_hold));
			d_hold = d_hold_n;
		}

		/*
		 * Look at the packet received
		 */
		g_packet = &g_packet_store;
		if(!g2_packet_extract_from_stream(d_hold, g_packet, buffer_remaining(*d_hold), false)) {
			logg_devel("packet extract failed\n");
			goto out;
		}

		if(!(DECODE_FINISHED == g_packet->packet_decode ||
		     PACKET_EXTRACTION_COMPLETE == g_packet->packet_decode)) {
			logg_devel("packet extract stuck in wrong state\n");
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
		if(t_hold) {
			gnd_buff_prep(t_hold, tmp_packet.sequence, tmp_packet.part, 0);
			udp_sock_send(t_hold, from, answer_fd);
		}
	}

	return true;
}

void g2_udp_send(const union combo_addr *to, struct list_head *answer)
{
	struct list_head *e, *n;
	struct norm_buff *d_hold;
	int answer_fd = AF_INET == to->s_fam ? udp_outfd_ipv4 : udp_outfd_ipv6;

	if(-1 == answer_fd) {
		logg_devel("trying to send to a address we have no fd for!?");
		goto out_err;
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

static ssize_t udp_sock_send(struct norm_buff *d_hold, const union combo_addr *to, int fd)
{
	ssize_t result;

	buffer_flip(*d_hold);
	do
	{
		errno = 0;
/* ssize_t sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen); */
		result = sendto(fd,
			buffer_start(*d_hold),
			buffer_remaining(*d_hold),
			0,
			casac(to),
			sizeof(*to));
	} while(-1 == result && EINTR == errno);

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
			if(EAGAIN != errno) {
				logg_posd(LOGF_ERR, "%s ERRNO=%i\tFDNum: %i\tFromIp: %p#I\n",
				         "error writing?!", errno, fd, to);
				return -1;
			} else
				logg_devel("Nothing to write!\n");
		}
		else
		{
			//putchar('+');
			//p_entry->events &= ~POLLIN;
		}
		break;
	case -1:
		if(EAGAIN != errno) {
			logg_errnod(LOGF_ERR, "%s ERRNO=%i\tFDNum: %i",
			            "sendto:", errno, fd);
			return -1;
		}
		break;
	}
	return result;
}

static inline bool handle_udp_sock(struct epoll_event *udp_poll, struct norm_buff *d_hold, union combo_addr *from, union combo_addr *to, int from_fd)
{
	ssize_t result;

	if(udp_poll->events & ~(POLLIN|POLLOUT)) {
		/* If our udp sock is not ready reading or writing -> fuzz */
		logg_pos(LOGF_ERR, "udp_so NVAL|ERR|HUP\n");
		return false;
	}

	if(!(udp_poll->events & POLLIN))
		return true;

	do
	{
		socklen_t from_len = sizeof(*from), to_len = sizeof(*to);
		errno = 0;
/* ssize_t  recvfromto(int s, void *buf, size_t len, int flags,
 *                     struct sockaddr *from, socklen_t *fromlen
 *                     struct sockaddr *to, socklen_t *tolen); */
		result = recvfromto(from_fd, buffer_start(*d_hold), buffer_remaining(*d_hold),
		                    0, casa(from), &from_len, casa(to), &to_len);
	} while(-1 == result && EINTR == errno);

	switch(result)
	{
	default:
		d_hold->pos += result;
		logg_devel_old("Data read from socket\n");
		break;
	case  0:
		if(buffer_remaining(*d_hold))
		{
			if(EAGAIN != errno) {
				logg_posd(LOGF_ERR, "%s ERRNO=%i\tFDNum: %i\tFromIp: %p#I\n",
				          "error reading?!", errno, from_fd, from);
				return false;
			} else
				logg_devel("Nothing to read!\n");
		}
		else
		{
			//putchar('+');
			//p_entry->events &= ~POLLIN;
		}
		break;
	case -1:
		if(EAGAIN != errno) {
			logg_errnod(LOGF_ERR, "%s ERRNO=%i\tFDNum: %i",
			            "recvfrom:", errno, from_fd);
			return false;
		}
		break;
	}
	return true;
}

#define OUT_ERR(x)	do {e = x; goto out_err;} while(0)
static inline bool init_con_u(int *udp_so, union combo_addr *our_addr)
{
	const char *e;
	int yes = 1; /* for setsockopt() SO_REUSEADDR, below */

	if(-1 == (*udp_so = socket(our_addr->s_fam, SOCK_DGRAM, 0))) {
		logg_errno(LOGF_ERR, "creating socket");
		return false;
	}

	if(udpfromto_init(*udp_so, our_addr->s_fam))
		logg_errno(LOGF_ERR, "preparing UDP ip recv");

	/* lose the pesky "address already in use" error message */
	if(setsockopt(*udp_so, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)))
		OUT_ERR("setsockopt reuse");

#ifdef HAVE_IPV6_V6ONLY
	if(AF_INET6 == our_addr->s_fam && server.settings.bind.use_ip4) {
		if(setsockopt(*udp_so, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)))
			OUT_ERR("setsockopt V6ONLY");
	}
#endif

	if(bind(*udp_so, casa(our_addr),
	        AF_INET == our_addr->s_fam ? sizeof(our_addr->in) : sizeof(our_addr->in6)))
		OUT_ERR("bindding udp fd");

#if 0
	/*
	 * UDP and non-blocking?? that's two points of unrelaiability,
	 * no, thanks
	 */
	if(fcntl(*udp_so, F_SETFL, O_NONBLOCK))
		OUR_ERR("udp non-blocking");
#endif
	return true;

out_err:
	logg_errno(LOGF_ERR, e);
	close(*udp_so);
	*udp_so = -1;
	return false;
}

void clean_up_udp(void)
{
	if(0 <= udp_sos[0].fd)
		close(udp_sos[0].fd);
	if(0 <= udp_sos[1].fd)
		close(udp_sos[1].fd);
	if(0 <= out_file)
		close(out_file);
}

/*@unsused@*/
static char const rcsid_u[] GCC_ATTR_USED_VAR = "$Id: G2UDP.c,v 1.16 2005/11/05 10:03:50 redbully Exp redbully $";
/* EOF */
