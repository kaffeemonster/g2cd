/*
 * G2UDP.c
 * thread to handle the UDP-part of the G2-Protocol
 *
 * Copyright (c) 2004, Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2
 * of the License, or any later version.
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
#include "config.h"
#endif
// System includes
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
// System net-includes
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
// other
#include "other.h"
// Own includes
#define _G2UDP_C
#include "G2UDP.h"
#include "G2MainServer.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"

#define FLAG_DEFLATE		0x01
#define FLAG_ACK			0x02
#define FLAG_CRITICAL	((~(FLAG_DEFLATE | FLAG_ACK)) & 0x0F)

//internal prototypes
static inline void handle_udp_sock(struct pollfd *, struct norm_buff *, struct sockaddr_in *);
static inline void handle_udp_packet(struct norm_buff *, struct sockaddr_in *);
//static inline bool init_memory();
static inline bool init_con_u(int *);
static inline void clean_up_u(int, int);

#define G2UDP_FD_SUM 2

//data-structures
static int out_file = -1;
static struct pollfd poll_me[G2UDP_FD_SUM];
static struct worker_sync {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	volatile bool wait;
	volatile bool keep_going;
} worker;

static void *G2UDP_loop(void *param)
{
	struct norm_buff d_hold = { .pos = 0, .limit = NORM_BUFF_CAPACITY, .capacity = NORM_BUFF_CAPACITY, .data = {0}};
	//other variables
	struct sockaddr_in from;

	while(worker.keep_going)
	{
		bool repoll;
		/*
		 * let only one thread at a time poll and recieve a packet
		 *
		 * We could keep them aout with a simple lock, but to avoid
		 * a "thundering herd" Problem, when the reciever leaves the
		 * section, we put them on a conditional (in the hope,
		 * cond_signal to wake one is implemented sensible...)
		 */
		pthread_mutex_lock(&worker.lock);
		while(worker.wait)
			pthread_cond_wait(&worker.cond, &worker.lock);
		if(!worker.keep_going)
		{
			pthread_cond_broadcast(&worker.cond);
			break;
		}
		worker.wait = true;
		pthread_mutex_unlock(&worker.lock);

		do
		{
			int num_poll = poll(poll_me, G2UDP_FD_SUM, 9000);
			repoll = false;
			switch(num_poll)
			{
			default:
				if(poll_me[0].revents)
				{
					handle_udp_sock(&poll_me[0], &d_hold, &from);
					if(!--num_poll)
						break;
				}
				if(poll_me[1].revents)
				{
// TODO: Check for a proper stop-sequence ??
					/*
					 * everything but 'ready for write' means:
					 * we're finished...
					 */
					if(poll_me[1].revents & ~POLLOUT)
						worker.keep_going = false;

					/* mask out any write interrest */
					poll_me[1].events &= ~POLLOUT;
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
					//and get out here (at the moment)
					worker.keep_going = false;
					break;
				}
			/* Nothing happened (or just the Timeout) */
			case 0:
				repoll = true;
			//	putchar('-');
			//	fflush(stdout);
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
		handle_udp_packet(&d_hold, &from);
		buffer_clear(d_hold);
	}

	if(!param)
		pthread_exit(NULL);
	/* only the creator thread should get out here */
	return NULL;
}

void *G2UDP(void *param)
{
	poll_me[1].fd = *((int *)param);
	logg(LOGF_INFO, "UDP:\t\tOur SockFD -> %d\t\tMain SockFD -> %d\n", poll_me[1].fd, *(((int *)param)-1));

	/* Setup locks */
	if(pthread_mutex_init(&worker.lock, NULL))
		goto out_lock;
	if(pthread_cond_init(&worker.cond, NULL))
		goto out_cond;
	/* Setting up the UDP Socket */
	if(!init_con_u(&poll_me[0].fd))
		goto out;
	else
	{
		char tmp_nam[sizeof("./G2UDPincomming.bin") + 12];
		snprintf(tmp_nam, sizeof(tmp_nam), "./G2UDPincomming%lu.bin", (unsigned long)getpid());
		if(0 > (out_file = open(tmp_nam, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)))
		{
			logg_errno(LOGF_ERR, "opening UDP-file");
			goto out;
		}
	}
	
	/* Setting first entry to be polled, our UDPsocket */
	poll_me[0].events = POLLIN | POLLERR;
	poll_me[1].events = POLLIN;

	worker.keep_going = true;
//TODO: Set up other worker threads
	/* we become one of them, only the special one which cleans up */
	/* we are up and running */
	server.status.all_abord[THREAD_UDP] = true;
	G2UDP_loop((void *)0x01);

out:
	pthread_cond_destroy(&worker.cond);
out_cond:
	pthread_mutex_destroy(&worker.lock);
out_lock:
	clean_up_u(poll_me[0].fd, poll_me[1].fd);
	pthread_exit(NULL);
	/* to avoid warning about reaching end of non-void function */
	return NULL;
}

static void handle_udp_packet(struct norm_buff *d_hold, struct sockaddr_in *from)
{
	gnd_packet_t tmp_packet;
	char addr_buf[INET6_ADDRSTRLEN];
	ssize_t result;
	size_t res_byte;
	uint8_t tmp_byte;

	if(!buffer_remaining(*d_hold))
		return;

	/* is it long enough to be a GNutella Datagram? */
	if(8 > buffer_remaining(*d_hold))
	{
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

	/* according to the specs, if there is a bit
	 * set in the lower nibble we can't assume
	 * any meaning too, discard the packet
	 */
	if(tmp_byte & FLAG_CRITICAL)
	{
		logg_devel("packet with other critical flags\n");
		return;
	}
	tmp_packet.flags.deflate = (tmp_byte & FLAG_DEFLATE) ? true : false;
	tmp_packet.flags.ack_me = (tmp_byte & FLAG_ACK) ? true : false;

	/* get the packet-sequence-number,
	 * fortunately byte-sex doesn't matter the
	 * numbers must only be diffrent or same
	 */
	tmp_packet.sequence = ((uint16_t)*buffer_start(*d_hold)) | (((uint16_t)*(buffer_start(*d_hold)+1)) << 8);
	d_hold->pos += 2;
						
	/* part */
	tmp_packet.part = *buffer_start(*d_hold);
	d_hold->pos++;

	/* count */
	tmp_packet.count = *buffer_start(*d_hold);
	d_hold->pos++;

	if(!tmp_packet.count)
	{
// TODO: Do an appropriate action on a recived ACK
		logg_devel("ACK recived!\n");
	}

	buffer_compact(*d_hold);

	res_byte = (size_t) snprintf(buffer_start(*d_hold), buffer_remaining(*d_hold),
		"\n----------\nseq: %u\tpart: %u/%u\ndeflate: %s\tack_me: %s\nsa_family: %i\nsin_addr:sin_port: %s:%hu\n----------\n",
		(unsigned)tmp_packet.sequence, (unsigned)tmp_packet.part, (unsigned)tmp_packet.count,
		(tmp_packet.flags.deflate) ? "true" : "false", (tmp_packet.flags.ack_me) ? "true" : "false", 
		((struct sockaddr *)from)->sa_family,
		inet_ntop(((struct sockaddr *)from)->sa_family, &from->sin_addr, addr_buf, sizeof(addr_buf)),
		ntohs(from->sin_port));
						
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
		d_hold->pos += result;
		logg_devel_old("Daten weggeschrieben\n");
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
		break;
	case -1:
		if(EAGAIN != errno)
		{
			logg_errnod(LOGF_ERR, "%s ERRNO=%i\tFDNum: %i",
				"write",
				errno,
				out_file);
			worker.keep_going = false;
		}
		break;
	}
}

static inline void handle_udp_sock(struct pollfd *udp_poll, struct norm_buff *d_hold, struct sockaddr_in *from)
{
	ssize_t result;
	socklen_t from_len;

	if(udp_poll->revents & ~(POLLIN|POLLOUT))
	{
		/* If our udp sock is not ready reading or writing -> fus */
		worker.keep_going = false;
		logg_pos(LOGF_ERR, "udp_so NVAL|ERR|HUP\n");
		return;
	}

	/* stop this write interrest */
	if(udp_poll->revents & POLLOUT)
		udp_poll->events &= ~POLLOUT;

	if(!(udp_poll->revents & POLLIN))
		return;

	do
	{
		errno = 0;
		from_len = sizeof(*from);
// ssize_t  recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
		result = recvfrom(udp_poll->fd,
			buffer_start(*d_hold),
			buffer_remaining(*d_hold),
			0,
			(struct sockaddr *)from,
			&from_len);
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
			if(EAGAIN != errno)
			{
				char addr_buf[INET6_ADDRSTRLEN];
				logg_posd(LOGF_ERR, "%s ERRNO=%i\tFDNum: %i\tFromIp: %s\tFromPort: %hu\n",
					"EOF reached!",
					errno,
					udp_poll->fd,
					inet_ntop(((struct sockaddr *)from)->sa_family, &from->sin_addr, addr_buf, sizeof(addr_buf)),
					ntohs(from->sin_port));
				worker.keep_going = false;
			}
			else
				logg_devel("Nothing to read!\n");
		}
		else
		{
			//putchar('+');
			//p_entry->events &= ~POLLIN;
		}
		break;
	case -1:
		if(EAGAIN != errno)
		{
			logg_errnod(LOGF_ERR, "%s ERRNO=%i\tFDNum: %i",
				"recvfrom:",
				errno,
				udp_poll->fd);
			worker.keep_going = false;
		}
		break;
	}
}

static inline bool init_con_u(int *udp_so)
{
	//sock-things
	struct sockaddr_in our_addr;
	int yes = 1; // for setsockopt() SO_REUSEADDR, below

	*udp_so = socket(AF_INET, SOCK_DGRAM, 0);
	if(-1 == *udp_so)
	{
		logg_errno(LOGF_ERR, "socket");
		return false;
	}
	
	// lose the pesky "address already in use" error message
	if(-1 == setsockopt(*udp_so, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)))
	{
		logg_errno(LOGF_ERR, "setsockopt");
		close(*udp_so);
		return false;
	}

	memset(&our_addr, 0, sizeof(our_addr));
	our_addr.sin_family = AF_INET;
	our_addr.sin_port = server.settings.portnr_2_bind;
	our_addr.sin_addr.s_addr = server.settings.ip_2_bind;

	if(bind(*udp_so, (struct sockaddr *)&our_addr, sizeof(our_addr)))
	{
		logg_errno(LOGF_ERR, "bind");
		close(*udp_so);
		return false;
	}

	// Die gebundene Ip rausfinden?
	//socklen_t len;
	//getsockname(sock, (sockaddr *) &my_addr, &len);
	logg_develd_old("Port: %d\n", ntohs(my_addr.sin_port));

// UDP and non-blocking?? that's two points of unrelaiability,
// no, thanks
//	if(fcntl(*udp_so, F_SETFL, O_NONBLOCK))
//	{
//		logg_errno(LOGF_ERR, "udp non-blocking");
//		close(*udp_so);
//		return(false);
//	}

	return true;
}

static inline void clean_up_u(int udp_so, int who_to_say)
{
	if(0 > send(who_to_say, "All lost", sizeof("All lost"), 0))
		diedie("initiating stop"); // hate doing this, but now it's to late
	logg_pos(LOGF_NOTICE, "should go down\n");

	if(0 < udp_so)
		close(udp_so);
	if(0 < out_file)
		close(out_file);

	server.status.all_abord[THREAD_UDP] = false;
}

static char const rcsid_u[] GCC_ATTR_USED_VAR = "$Id: G2UDP.c,v 1.16 2005/11/05 10:03:50 redbully Exp redbully $";
//EOF
