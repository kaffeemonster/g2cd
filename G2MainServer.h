/*
 * G2MainServer.h
 * header-file for G2MainServer.c and global informations
 *
 * Copyright (c) 2008-2010 Jan Seiffert
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
# define _G2MAINSERVER_H

/* Includes if included */
# include "lib/my_pthread.h"
# include <stdbool.h>
# include <stdio.h>
# include <time.h>
# include <sys/types.h>
# include <errno.h>

/* Own */
# include "lib/other.h"
static always_inline enum loglevel get_act_loglevel(void);
# include "G2Connection.h"
# include "G2Packet.h"
# include "G2QHT.h"
# include "lib/combo_addr.h"
# include "lib/atomic.h"
# include "lib/log_facility.h"

# define OWN_VENDOR_CODE    "G2CD"

# define THREAD_GUP         0
# define THREAD_TIMER       1
# define THREAD_SUM_COM     1
# define THREAD_SUM         2

# define DIR_OUT   0
# define DIR_IN    1

# define EVENT_SPACE   16

# define FC_CAP_START  16
# define FC_TRESHOLD   4
# define FB_CAP_START  (THREAD_SUM * 8 * EVENT_SPACE * 2)
# define FB_TRESHOLD   EVENT_SPACE

# define PD_START_CAPACITY      128

# ifndef _G2MAINSERVER_C
#  define _G2MAIN_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
#  define _G2MAIN_EXTRNVAR(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define _G2MAIN_EXTRN(x) x GCC_ATTR_VIS("hidden")
#  define _G2MAIN_EXTRNVAR(x) x GCC_ATTR_VIS("hidden")
# endif /* _G2MAINSERVER_C */

_G2MAIN_EXTRNVAR(struct
{
		struct
		{
			volatile int max_connection_sum;
			volatile int max_hub_sum;
			pthread_attr_t t_def_attr;
			const char *data_root_dir;
			const char *entropy_source;
			const char *config_file;
			int nice_adjust;
			unsigned num_threads;
			enum g2_connection_encodings default_in_encoding;
			enum g2_connection_encodings default_out_encoding;
			enum g2_connection_encodings hub_in_encoding;
			enum g2_connection_encodings hub_out_encoding;
			uint8_t our_guid[16];
			unsigned max_g2_packet_length;
			struct
			{
				const char *name;
				size_t len;
				uint32_t lat_long;
				bool send_clients;
				bool send_gps;
			} nick;
			struct
			{
				enum loglevel act_loglevel;
				bool add_date_time;
				const char *time_date_format;
			} logging;
			struct
			{
				union combo_addr ip4;
				union combo_addr ip6;
				bool use_ip4;
				bool use_ip6;
			} bind;
			struct
			{
				bool want_2_send;
				const char *xml;
				size_t xml_length;
			} profile;
			struct
			{
				const char *gwc_boot_url;
				const char *gwc_cache_fname;
				const char *dump_fname;
			} khl;
			struct
			{
				const char *dump_fname;
			} guid;
			struct
			{
				enum g2_qht_comp compression;
				bool compress_internal;
				size_t max_promille;
			} qht;
		} settings;
		struct
		{
			atomic_t act_connection_sum;
			bool all_abord[THREAD_SUM];
			bool our_server_upeer;
			time_t start_time;
			atomic_t act_hub_sum;
		} status;
} server);

#ifdef HAVE___THREAD
_G2MAIN_EXTRNVAR(__thread time_t local_time_now);
#else
/*
 * no tls? let the threads race against each other,
 * nothing spectacular should happen.
 */
_G2MAIN_EXTRNVAR(time_t local_time_now);
#endif
_G2MAIN_EXTRNVAR(volatile time_t master_time_now);

static always_inline void update_local_time(void)
{
	local_time_now = master_time_now;
// TODO: membar?
	barrier();
}

static always_inline void set_master_time(time_t now)
{
	master_time_now = now;
// TODO: membar?
	barrier();
}

static always_inline enum loglevel get_act_loglevel(void)
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

_G2MAIN_EXTRN(void g2_set_thread_name(const char *));
#endif /* _G2MAINSERVER_H */
/* EOF */
