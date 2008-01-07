/*
 * G2MainServer.h
 * header-file for G2MainServer.c and global informations
 *
 * Copyright (c) 2004, Jan Seiffert
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
 * $Id: G2MainServer.h,v 1.16 2005/09/05 10:18:46 redbully Exp redbully $
 */

#ifndef _G2MAINSERVER_H
#define _G2MAINSERVER_H

// Includes if included
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <errno.h>

// Own
#include "other.h"
static always_inline int get_act_loglevel(void);
#include "G2Connection.h"
#include "G2Packet.h"
#include "lib/atomic.h"
#include "lib/log_facility.h"

#define THREAD_SUM		3
#define THREAD_ACCEPTOR		0
#define THREAD_HANDLER		1
#define THREAD_UDP		2

#define OUT   0
#define IN    1

#define EVENT_SPACE  16

#define FC_CAP_END	2048 /* not used ATM*/
#define FC_CAP_START	16
#define FC_CAP_INC	32 /* not used ATM */
#define FC_TRESHOLD	4
#define FB_CAP_START	(THREAD_SUM * EVENT_SPACE * 2)
#define FB_TRESHOLD	EVENT_SPACE

#define PD_START_CAPACITY 128
#define PD_CAPACITY_INCREMENT 32
#define WC_START_CAPACITY 128
#define WC_CAPACITY_INCREMENT 8

struct poll_info
{
	size_t limit;
	size_t capacity;
	struct pollfd data[DYN_ARRAY_LEN];
};

#ifndef _G2MAINSERVER_C
#define _G2MAIN_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#define _G2MAIN_EXTRNVAR(x) extern x GCC_ATTR_VIS("hidden")
#else
#define _G2MAIN_EXTRN(x) x GCC_ATTR_VIS("hidden")
#define _G2MAIN_EXTRNVAR(x) x GCC_ATTR_VIS("hidden")
#endif // _G2MAINSERVER_C

_G2MAIN_EXTRNVAR(struct
{
		struct
		{
			pthread_attr_t t_def_attr;
			struct
			{
				enum loglevel act_loglevel;
				bool add_date_time;
				const char *time_date_format;
			} logging;
			in_port_t portnr_2_bind;
			in_addr_t ip_2_bind;
			enum g2_connection_encodings default_in_encoding;
			enum g2_connection_encodings default_out_encoding;
			size_t default_max_g2_packet_length;
			volatile int max_connection_sum;
			uint8_t our_guid[16];
			bool want_2_send_profile;
		} settings;
		struct
		{
			atomic_t act_connection_sum;
			bool all_abord[THREAD_SUM];
			bool our_server_upeer;
			bool our_server_upeer_needed;
		} status;
} server);

_G2MAIN_EXTRNVAR(size_t packet_uprod_length);
_G2MAIN_EXTRNVAR(char *packet_uprod);

static always_inline int get_act_loglevel(void)
{
	return server.settings.logging.act_loglevel;
}

/*static inline void wrap_perror(const char *which_file, const char *which_func, const unsigned int where, const char *when)
{
	int whats_wrong = errno;
	fprintf(stderr, "%s, %s(), %u: ", which_file, which_func, where);
	errno = whats_wrong;
	perror(when);
}*/
#endif // _G2MAINSERVER_H
//EOF
