/*
 * G2UDP.c
 * thread to handle the UDP-part of the G2-Protocol
 *
 * Copyright (c) 2004-2009 Jan Seiffert
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
#include "G2MainServer.h"
#include "G2PacketSerializer.h"
#include "lib/atomic.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/recv_buff.h"
#include "lib/udpfromto.h"
#include "lib/hzp.h"

#define UDP_MTU        500
#define UDP_RELIABLE_LENGTH 8

#define FLAG_DEFLATE   (1 << 0)
#define FLAG_ACK       (1 << 1)
#define FLAG_CRITICAL  ((~(FLAG_DEFLATE|FLAG_ACK)) & 0x0F)

/* internal prototypes */
static inline ssize_t udp_sock_send(struct norm_buff *, const union combo_addr *, int);
static inline void handle_udp_sock(struct pollfd *, struct norm_buff *, union combo_addr *, union combo_addr *);
static noinline void handle_udp_packet(struct norm_buff *, union combo_addr *, union combo_addr *, int);
//static inline bool init_memory();
static inline bool init_con_u(int *, union combo_addr *);
static inline void clean_up_u(int, struct pollfd[2]);

#define G2UDP_FD_SUM 3

/* data-structures */
static int out_file;
static int udp_outfd_ipv4;
static int udp_outfd_ipv6;
static struct pollfd poll_me[G2UDP_FD_SUM];
static struct worker_sync {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	volatile bool wait;
	volatile bool keep_going;
} worker;


#ifndef HAVE___THREAD
static pthread_key_t key2udp_lsb;
/* protos */
	/* you better not kill this prot, our it won't work ;) */
static void udp_init(void) GCC_ATTR_CONSTRUCT;
static void udp_deinit(void) GCC_ATTR_DESTRUCT;

static void udp_init(void)
{
	if(pthread_key_create(&key2udp_lsb, (void(*)(void *))recv_buff_free))
		diedie("couln't create TLS key for logging");
}

static void udp_deinit(void)
{
	struct norm_buff *tmp_buf;

	if((tmp_buf = pthread_getspecific(key2udp_lsb))) {
		recv_buff_free(tmp_buf);
		pthread_setspecific(key2udp_lsb, NULL);
	}
	pthread_key_delete(key2udp_lsb);
}
#else
static __thread struct norm_buff *udp_lsbuffer;
#endif

static struct norm_buff *udp_get_lsbuf(void)
{
	struct norm_buff *ret_buf;
#ifndef HAVE___THREAD
	ret_buf = pthread_getspecific(key2udp_lsb);
	if(likely(ret_buf))
		return ret_buf;
#else
	ret_buf = udp_lsbuffer;
	if(likely(ret_buf))
		return ret_buf;
#endif

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

static void *G2UDP_loop(void *param)
{
	struct norm_buff d_hold = { .pos = 0, .limit = NORM_BUFF_CAPACITY, .capacity = NORM_BUFF_CAPACITY, .data = {0}};
	/* other variables */
	union combo_addr from, to;
	int answer_fd = -1;

	/* make our hzp ready */
	hzp_alloc();

	my_snprintf(d_hold.data, d_hold.limit, OUR_PROC " UDP %ti", (intptr_t)param);
	g2_set_thread_name(d_hold.data);

	while(worker.keep_going)
	{
		bool repoll;
		/*
		 * let only one thread at a time poll and recieve a packet
		 *
		 * We could keep them out with a simple lock, but to avoid
		 * a "thundering herd" Problem, when the reciever leaves the
		 * section, we put them on a conditional (in the hope,
		 * cond_signal to wake one is implemented sensible...)
		 */
		pthread_mutex_lock(&worker.lock);
		while(worker.wait)
			pthread_cond_wait(&worker.cond, &worker.lock);
		worker.wait = worker.keep_going ? true : false;
		pthread_mutex_unlock(&worker.lock);
		if(!worker.keep_going) {
			pthread_cond_broadcast(&worker.cond);
			break;
		}

		do
		{
			int num_poll = poll(poll_me, G2UDP_FD_SUM, 9000);
			repoll = false;
			switch(num_poll)
			{
			default:
				if(poll_me[0].revents)
				{
// TODO: Check for a proper stop-sequence ??
					/*
					 * everything but 'ready for write' means:
					 * we're finished...
					 */
					if(poll_me[0].revents & ~POLLOUT)
						worker.keep_going = false;
					/* mask out any write interrest */
					poll_me[0].events &= ~POLLOUT;
					if(!--num_poll)
						break;
				}
				if(poll_me[1].revents) {
					handle_udp_sock(&poll_me[1], &d_hold, &from, &to);
					answer_fd = poll_me[1].fd;
					if(!--num_poll)
						break;
				} else if(poll_me[2].revents) {
					answer_fd = poll_me[2].fd;
					handle_udp_sock(&poll_me[2], &d_hold, &from, &to);
					if(!--num_poll)
						break;
				}
				logg_pos(LOGF_ERR, "too much in poll\n");
				worker.keep_going = false;
				break;
			/* Something bad happened */
			case -1:
				if(EINTR != errno)
				{
					/* Print what happened */
					logg_errno(LOGF_ERR, "poll");
					/* and get out here (at the moment) */
					worker.keep_going = false;
					break;
				}
			/* Nothing happened (or just the Timeout) */
			case 0:
				repoll = true;
				break;
			}
		} while(repoll);

		pthread_mutex_lock(&worker.lock);
		worker.wait = false;
		pthread_cond_signal(&worker.cond);
		pthread_mutex_unlock(&worker.lock);
		if(!worker.keep_going)
			break;

		buffer_flip(d_hold);
		/* if we reach here, we know that there is at least no error or the logik above failed... */
		handle_udp_packet(&d_hold, &from, &to, answer_fd);
		buffer_clear(d_hold);
	}

	/* clean up our hzp */
	hzp_free();

	if(param)
		pthread_exit(NULL);
	/* only the creator thread should get out here */
	return NULL;
}

void *G2UDP(void *param)
{
	static pthread_t *helper;
// TODO: get number of helper threads
	size_t num_helper = 1, i;

	poll_me[0].fd = *((int *)param);
	logg(LOGF_INFO, "UDP:\t\tOur SockFD -> %d\tMain SockFD -> %d\n", poll_me[0].fd, *(((int *)param)-1));

	helper = malloc(num_helper*sizeof(*helper));
	if(!helper) {
		logg_errno(LOGF_CRIT, "No mem for UDP helper threads, will run with one");
		num_helper = 0;
	}
	/* Setup locks */
	if(pthread_mutex_init(&worker.lock, NULL))
		goto out_lock;
	if(pthread_cond_init(&worker.cond, NULL))
		goto out_cond;
	{
		char tmp_nam[sizeof("./G2UDPincomming.bin") + 12];
		bool ipv4_ready;
		bool ipv6_ready;

		poll_me[2].fd = poll_me[1].fd = -1;
		/* Setting up the UDP Socket */
		ipv4_ready = server.settings.bind.use_ip4 ? init_con_u(&poll_me[1].fd, &server.settings.bind.ip4) : false;
		ipv6_ready = server.settings.bind.use_ip6 ? init_con_u(&poll_me[2].fd, &server.settings.bind.ip6) : false;
		udp_outfd_ipv4 = poll_me[1].fd;
		udp_outfd_ipv6 = poll_me[2].fd;
	
		if(server.settings.bind.use_ip4 && !ipv4_ready)
			goto out;
		if(server.settings.bind.use_ip6 && !ipv6_ready)
		{
			if(ipv4_ready)
				logg(LOGF_ERR, "Error starting IPv6, continueing, but maybe not what you want!\n");
			else
				goto out;
		}

		my_snprintf(tmp_nam, sizeof(tmp_nam), "./G2UDPincomming%lu.bin", (unsigned long)getpid());
		if(0 > (out_file = open(tmp_nam, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR))) {
			logg_errno(LOGF_ERR, "opening UDP-file");
			goto out;
		}
	}
	
	/* Setting first entry to be polled, our UDPsocket */
	poll_me[0].events = POLLIN;
	poll_me[1].events = POLLIN | POLLERR;

	worker.keep_going = true;
	for(i = 0; i < num_helper; i++)
	{
		if(pthread_create(&helper[i], &server.settings.t_def_attr, G2UDP_loop, (void *)(i + 1))) {
			logg_errnod(LOGF_WARN, "starting UDP helper threads, will run with %zu", i);
			num_helper = i;
			break;
		}
	}
	/* we become one of them, only the special one which cleans up */
	/* we are up and running */
	server.status.all_abord[THREAD_UDP] = true;
	G2UDP_loop(NULL);

	for(i = 0; i < num_helper; i++)
	{
		if(pthread_join(helper[i], NULL)) {
			logg_errno(LOGF_WARN, "taking down UDP helper threads");
			break;
		}
	}

out:
	pthread_cond_destroy(&worker.cond);
out_cond:
	pthread_mutex_destroy(&worker.lock);
out_lock:
	clean_up_u(poll_me[0].fd, &poll_me[1]);
	free(helper);
	pthread_exit(NULL);
	/* to avoid warning about reaching end of non-void function */
	return NULL;
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
	num_udp_packets = (result+(UDP_MTU-UDP_RELIABLE_LENGTH-1))/(UDP_MTU-UDP_RELIABLE_LENGTH);
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

static void handle_udp_packet(struct norm_buff *d_hold, union combo_addr *from, union combo_addr *to, int answer_fd)
{
	gnd_packet_t tmp_packet;
	g2_packet_t g_packet;
	struct ptype_action_args parg;
	struct list_head answer;
	size_t res_byte;
	uint8_t tmp_byte;

	if(!buffer_remaining(*d_hold))
		return;

	/* is it long enough to be a GNutella Datagram? */
	if(UDP_RELIABLE_LENGTH > buffer_remaining(*d_hold)) {
		logg_devel("really short packet recieved\n");
		return;
	}

	/* is it a GND? */
	if('G' != *buffer_start(*d_hold) || 'N' != *(buffer_start(*d_hold)+1) || 'D' != *(buffer_start(*d_hold)+2))
		return;
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
		return;
	}
	tmp_packet.flags.deflate = (tmp_byte & FLAG_DEFLATE) ? true : false;
	tmp_packet.flags.ack_me = (tmp_byte & FLAG_ACK) ? true : false;

	/*
	 * get the packet-sequence-number,
	 * fortunately byte-sex doesn't matter, the
	 * numbers must only be different or same
	 */
	get_unaligned(tmp_packet.sequence, ((uint16_t *)buffer_start(*d_hold)));
	d_hold->pos += 2;

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
				worker.keep_going = false;
			}
			else
				logg_devel("Nothing to write!\n");
		}
		else
		{
			//putchar('+');
			//p_entry->events &= ~POLLIN;
		}
		return;
	case -1:
		if(EAGAIN != errno)
		{
			logg_errnod(LOGF_ERR, "%s ERRNO=%i\tFDNum: %i",
				"write",
				errno,
				out_file);
			worker.keep_going = false;
		}
		return;
	}
	}
/************ DEBUG *****************/

	if(!tmp_packet.count) {
// TODO: Do an appropriate action on a recived ACK
		logg_devel("ACK recived!\n");
		return;
	}

// TODO: Handle multipart UDP packets
	if(tmp_packet.count > 1) {
		logg_devel("multipart UDP packet, needs reassamble\n");
		goto out;
	}

	if(tmp_packet.count != tmp_packet.part) {
		logg_devel("broken UDP packet count = 1 part != 1\n");
		goto out;
	}

	if(tmp_packet.flags.deflate) {
		logg_devel("compressed packet recevied\n");
		goto out;
	}

	/*
	 * Look at the packet received
	 */
	g2_packet_init_on_stack(&g_packet);
	if(!g2_packet_extract_from_stream(d_hold, &g_packet, buffer_remaining(*d_hold))) {
		logg_devel("packet extract failed\n");
		goto out;
	}

	if(!(DECODE_FINISHED == g_packet.packet_decode ||
	     PACKET_EXTRACTION_COMPLETE == g_packet.packet_decode)) {
		logg_devel("packet extract stuck in wrong state\n");
		goto out_free;
	}

	INIT_LIST_HEAD(&answer);
	parg.connec      = NULL;
	parg.source      = &g_packet;
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
	g2_packet_free(&g_packet);
out:
	if(tmp_packet.flags.ack_me) {
		gnd_buff_prep(d_hold, tmp_packet.sequence, tmp_packet.part, 0);
		udp_sock_send(d_hold, from, answer_fd);
	}

	return;
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
		list_del(e);
		g2_packet_free(entry);
	}
	return;

out_err:
	list_for_each_safe(e, n, answer) {
		g2_packet_t *entry = list_entry(e, g2_packet_t, list);
		list_del(e);
		g2_packet_free(entry);
	}
}

static inline ssize_t udp_sock_send(struct norm_buff *d_hold, const union combo_addr *to, int fd)
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
				worker.keep_going = false;
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
			worker.keep_going = false;
		}
		break;
	}
	return result;
}

static inline void handle_udp_sock(struct pollfd *udp_poll, struct norm_buff *d_hold, union combo_addr *from, union combo_addr *to)
{
	ssize_t result;

	if(udp_poll->revents & ~(POLLIN|POLLOUT))
	{
		/* If our udp sock is not ready reading or writing -> fus */
		worker.keep_going = false;
		logg_pos(LOGF_ERR, "udp_so NVAL|ERR|HUP\n");
		return;
	}

	/* stop this write interrest */
	if(udp_poll->revents &   POLLOUT)
	   udp_poll->events  &= ~POLLOUT;

	if(!(udp_poll->revents & POLLIN))
		return;

	do
	{
		socklen_t from_len = sizeof(*from), to_len = sizeof(*to);
		errno = 0;
/* ssize_t  recvfromto(int s, void *buf, size_t len, int flags,
 *                     struct sockaddr *from, socklen_t *fromlen
 *                     struct sockaddr *to, socklen_t *tolen); */
		result = recvfromto(udp_poll->fd, buffer_start(*d_hold), buffer_remaining(*d_hold),
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
				          "error reading?!", errno, udp_poll->fd, from);
				worker.keep_going = false;
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
			            "recvfrom:", errno, udp_poll->fd);
			worker.keep_going = false;
		}
		break;
	}
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

static inline void clean_up_u(int who_to_say, struct pollfd udp_so[2])
{
	if(0 > send(who_to_say, "All lost", sizeof("All lost"), 0))
		diedie("initiating stop"); // hate doing this, but now it's to late
	logg_pos(LOGF_NOTICE, "should go down\n");

	if(0 <= udp_so[0].fd)
		close(udp_so[0].fd);
	if(0 <= udp_so[1].fd)
		close(udp_so[1].fd);
	if(0 <= out_file)
		close(out_file);

	server.status.all_abord[THREAD_UDP] = false;
}

/*@unsused@*/
static char const rcsid_u[] GCC_ATTR_USED_VAR = "$Id: G2UDP.c,v 1.16 2005/11/05 10:03:50 redbully Exp redbully $";
/* EOF */
