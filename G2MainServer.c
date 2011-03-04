/*
 * G2MainServer.c
 * This is a server-only implementation for the G2-P2P-Protocol
 * here you will find main()
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
 * $Id: G2MainServer.c,v 1.25 2005/11/05 18:02:45 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* System includes */
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/types.h>
#ifdef WIN32
# define _WIN32_WINNT 0x0501
# include <windows.h>
# include <wincrypt.h>
#else
# include <sys/stat.h>
# include <sys/wait.h>
# include <sys/resource.h>
# include <sys/ioctl.h>
# ifdef __linux__
#  include <linux/sockios.h>
# endif
# include <pwd.h>
#endif
#include <sys/time.h>
#include <fcntl.h>
#include "lib/my_pthread.h"
#include <unistd.h>
#ifdef DRD_ME
# include <valgrind/drd.h>
#endif
/* Own Includes */
#define _G2MAINSERVER_C
#include "lib/other.h"
#include "G2MainServer.h"
#include "gup.h"
#include "G2UDP.h"
#include "G2Connection.h"
#include "G2Handler.h"
#include "G2ConRegistry.h"
#define _NEED_G2_P_TYPE
#include "G2Packet.h"
#include "timeout.h"
#include "idbus.h"
#include "G2KHL.h"
#include "G2GUIDCache.h"
#include "G2QueryKey.h"
#include "G2Acceptor.h"
#include "lib/my_epoll.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/hzp.h"
#include "lib/atomic.h"
#include "lib/backtrace.h"
#include "lib/ansi_prng.h"
#include "lib/config_parser.h"
#include "lib/my_bitopsm.h"
#include "version.h"
#include "builtin_defaults.h"
#ifdef WIN32
# define DAVE_NULL "NUL"
#else
# define DAVE_NULL "/dev/null"
#endif
#ifndef O_BINARY
# define O_BINARY 0
#endif

/* Thread data */
static pthread_t main_threads[THREAD_SUM];
static pthread_mutex_t sock_com_lock;
static some_fd sock_com[THREAD_SUM_COM][2];
static LIST_HEAD(sock_com_list);
static volatile sig_atomic_t server_running;
static volatile sig_atomic_t reopen_logfile;

/* helper vars */
static bool be_daemon;
static bool connect_dbus;
static const char *log_fname;
static time_t last_HAW;
static time_t last_full_check;

/* bulk data */
static const char *user_name;
static struct
{
	const char *latitude;
	const char *longitude;
	const char *country;
	const char *city;
	const char *state;
	const char *gender;
	unsigned age;
} profile;

/* Internal prototypes */
static noinline __init bool startup(int, char **);
static noinline bool clutch_logfile(void);
static noinline void clean_up_m(void);
static void next_HAW_helper(void);
static void next_health_helper(void);
static intptr_t check_con_health(g2_connection_t *con, void *carg);
static my_pollfd *sock_com_create_pfd(my_pollfd *pfd, unsigned *num, unsigned *len);

int main(int argc, char **args)
{
	size_t i;
	my_pollfd *gpfd;
	unsigned gpfd_num = 0, gpfd_len = THREAD_SUM_COM + 5;
	bool long_poll = true;

	gpfd = malloc(gpfd_len * sizeof(*gpfd));
	if(!gpfd) {
		logg_errno(LOGF_ERR, "couldn't allocate mem for poll fds");
		return EXIT_FAILURE;
	}

	/*
	 * OK, lets fire this thing up
	 * http://www.youtube.com/watch?v=CG91fYHcm-k
	 */
	if(!startup(argc, args))
		return EXIT_FAILURE;
	/* "It's alive!" */

	/* main server wait loop */
	while(server_running)
	{
		int num_poll;

		gpfd = sock_com_create_pfd(gpfd, &gpfd_num, &gpfd_len);
		num_poll = poll(gpfd, gpfd_num, long_poll ? 11300 : 1100);
		update_local_time();
		switch(num_poll)
		{
		/* Normally: see what has happened */
		default:
			for(i = 0; i < gpfd_num && num_poll; i++)
			{
				struct sock_com *s;
				if(!gpfd[i].revents)
					continue;
				num_poll--;
				s = sock_com_fd_find(gpfd[i].fd);
				if(!s && !s->enabled)
					continue;
				if(!s->handler) {
					logg_pos(LOGF_INFO, "bytes at the pipes\n");
					server_running = false;
					break;
				} else if(G2KHL_SOCK_COM_HANDLER == s->handler) {
					/* nop, we fall through to g2_khl_tick */;
				} else {
					s->handler(s, gpfd[i].revents);
				}
			}
			/* yes, fall through */
		/* Nothing happened (or just the Timeout) */
		case 0:
			/* all abord? */
			for(i = 0; i < THREAD_SUM; i++)
			{
				logg_develd_old("Up is thread num %zu: %s\n", i,
				                (server.status.all_abord[i]) ? "true" : "false");
				if(!server.status.all_abord[i]) {
					server_running = false;
					logg_pos(LOGF_ERR, "We've lost someone! Going down!\n");
				}
			}
			/* service our memleak ;) */
			if(server_running) /* not when someone lost, maybe deadlock */
			{
				num_poll = hzp_scan(NORM_HZP_THRESHOLD);
				logg_develd_old("HZP reclaimed chunks: %i\n", num_poll);
				long_poll = g2_khl_tick();
				/*
				 * first hzp scan, then update (will generate new
				 * mem to free, which we want to linger around one
				 * more round)
				 */
				g2_qht_global_update();
				/* Check if the query key salt needs service */
				g2_qk_tick();
				/* clean up udp reassambly cache */
				g2_udp_reas_timeout();
				/* time to send a HAW again? */
				next_HAW_helper();
				next_health_helper();
				if(reopen_logfile) {
					if(!clutch_logfile())
						server_running = false;
					reopen_logfile = false;
				}
			}
			break;
		/* Something bad happened */
		case -1:
			if(EINTR == errno) break;
			/* Print what happened */
			logg_errno(LOGF_ERR, "poll");
			/* and get out here (at the moment) */
			server_running = false;
			break;
		}
	}

	/* send all threads the abort-code */
	timeout_timer_task_abort();
	for(i = 0; i < THREAD_SUM_COM; i++)
	{
		if(0 > send(sock_com[i][DIR_OUT], "All lost", sizeof("All lost"), 0))
			logg_errno(LOGF_WARN, "initiating stop");
	}

	/*
	 * wait a moment until all threads have gracefully stoped,
	 * or exit anyway
	 */
	for(i = 0; i < 10; i++)
	{
		bool all_down = false;
		size_t j;
		for(j = 0; j < THREAD_SUM; j++)
		{
			if(!server.status.all_abord[j])
				all_down = true;
			else {
				all_down = false;
				break;
			}
		}

		if(all_down)
			break;
		else if(9 == i) {
			logg_pos(LOGF_ERR, "not all gone down! Aborting!\n");
#ifndef WIN32
			fsync(STDOUT_FILENO);
#endif
			g2_khl_end(); /* frantic writeout! */
			return EXIT_FAILURE;
		}
		sleep(1);
	}
	
	/* collect the threads */
	pthread_join(main_threads[THREAD_GUP], NULL);
	/* Join THREAD_TIMER??? Hmmm, there was a reason... */

	hzp_scan(0);
	/*
	 * cleanup khl system
	 * This is fairly important.
	 * All data to be written is only cached info, we could
	 * live without it.
	 * But not doing so may lead to:
	 * - more load on the GWCs
	 * - bad "perfomance" after a simple restart (info still valid)
	 * - no connection to the Network on startup
	 * - grief for the user because the gwc cache db may get
	 *   corrupted and we are not able to recover from this
	 * Things complicating our business are the typical
	 * "IO can fail" and that we may changed the user now, so
	 * don't have write permission. Additionaly we may have
	 * crashed, preventing us to reach this code (no, i don't
	 * want atexit()...)
	 */
	g2_khl_end();
	/*
	 * we also try to save guids, since they are essential for
	 * "routing". They also will age and get stall, but beneficial
	 * to save over a short restart.
	 */
	g2_guid_end();
	/*
	 * and the question is if we want to save the query key cache,
	 * but we don't need them, since we do not query.
	 */
	g2_conreg_cleanup();
	/*
	 * wipe out all remaining udp reassambly cache entrys
	 */
	local_time_now += 5000;
	g2_udp_reas_timeout();
	/* double scan to free everything */
	hzp_scan(0);
	hzp_scan(0);

	free(gpfd);
	clean_up_m();
#ifndef WIN32
	fsync(STDOUT_FILENO);
#endif
	return EXIT_SUCCESS;
}

static void next_HAW_helper(void)
{
	long tdiff = local_time_now - last_HAW;
	tdiff = tdiff >= 0 ? tdiff : -tdiff;
	if(tdiff >= (5 * 60)) {
		g2_conreg_random_hub(NULL, send_HAW_callback, NULL);
		last_HAW = local_time_now;
	}
}

#define FIRST_GRACE_TIME (5 * 60)
#define SECOND_GRACE_TIME (30 * 60)
#define FIRST_MIN_LEAF 5
#define SECOND_MIN_LEAF 50

struct bounce_to_timer
{
	struct hzp_free h;
	struct timeout t;
	g2_connection_t *con;
};

static int bounce_from_timer(void *data)
{
	struct bounce_to_timer *btt = data;

	logg_devel("bounced teardown\n");
	hzp_ref(HZP_QHT, btt->con);
	/* we hold the connection lock, tear it down */
	gup_teardown_con(btt->con);
	/* we hold a hzp reference on it */
	pthread_mutex_unlock(&btt->con->lock);
	hzp_unref(HZP_QHT);
	/*
	 * to avoid use after free in timer code, defer
	 * final free of this struct, we hold the hzp ref
	 * on it
	 */
	hzp_deferfree(&btt->h, btt, (void (*)(void *))free);
	return 0;
}

static noinline intptr_t check_hub_health(g2_connection_t *con, void *carg GCC_ATTR_UNUSED_PARAM)
{
	unsigned min_leafs = FIRST_MIN_LEAF;

	if(unlikely(con->connect_time > local_time_now - FIRST_GRACE_TIME))
		return 0;
	if(con->connect_time < local_time_now - SECOND_GRACE_TIME)
		min_leafs = SECOND_MIN_LEAF;

	if(con->u.handler.leaf_count < min_leafs) {
		con->flags.dismissed = true;
		gup_con_mark_write(con);
	}
	return 0;
}

static void next_health_helper(void)
{
	long tdiff = local_time_now - last_full_check;
	tdiff = tdiff >= 0 ? tdiff : -tdiff;
	if(tdiff >= (60))
	{
		last_full_check = local_time_now;
		if(server.settings.have_tcp_send_timeout)
			g2_conreg_all_hub(NULL, check_hub_health, NULL);
		else
			g2_conreg_all_con(check_con_health, NULL);
	}
}

static intptr_t check_con_health(g2_connection_t *con, void *carg)
{
	int val;

	/* if con is stuck sending or dead, kill it with fire */
	/*
	 * The worker threads should normally take down the
	 * con, but to inject the event, we normally have to go
	 * around and signal a "can write" (It is the only nice
	 * way to push this through epoll, waking a thread for
	 * THIS connection while atomically removing it from
	 * epoll, epoll "eats" HUP events...).
	 * When we can not write, because the other end is
	 * "unresponsive", we are stuck. We have to rely on
	 * the OS detecting this. Unfortunatly TCP-retries
	 * and so on totally sink us here, we accumulate
	 * dead stuff.
	 * Try to rescue the day by forcefully tearing the con
	 * down.
	 * This is scary and propably racy, and whatnot...
	 */
#ifdef __linux__
// TODO: TIOCOUTQNSD
	/* maybe there will be a new ioctl for the _not_ send data */
	if(ioctl(con->com_socket, SIOCOUTQ, &val) < 0)
#endif
		val = 0;
	shortlock_t_lock(&con->pts_lock);
	if(unlikely(con->flags.dismissed ||
	  ((val || !list_empty(&con->packets_to_send)) &&
	    local_time_now >= con->last_send + (3 * HANDLER_ACTIVE_TIMEOUT))))
	{
		struct bounce_to_timer *btt;

		/*
		 * Since we hold the con reg lock the connection can
		 * not vanish, but we have to first bounce the con to
		 * someone not holding the lock.
		 * The worker do not seem to respond to this sucker,
		 * so bounce it to the timer thread.
		 */
		shortlock_t_unlock(&con->pts_lock);
		btt = malloc(sizeof(*btt));
		/*
		 * make sure we lock the conection to keep out the
		 * worker threads.
		 */
		if(btt && !pthread_mutex_trylock(&con->lock))
		{
			/* we have the con, tear it down */
			logg_devel("init bounced teardown\n");
			INIT_TIMEOUT(&btt->t);
			btt->t.data = btt;
			btt->t.fun  = bounce_from_timer;
			btt->con    = con;
			timeout_add(&btt->t, 10);
			/*
			 * we can not and do not want to unlock the con
			 * so it is not freed by someone else, maybe something
			 * will whine that the locker and unlocker of the lock
			 * are not the same thread, so what...
			 */
		}
		else {
			/* cowardly buck down, we will come back... */
			free(btt);
			con->flags.dismissed = true;
			gup_con_mark_write(con);
		}
		return 0;
	}
	shortlock_t_unlock(&con->pts_lock);

	if(con->flags.upeer)
		return check_hub_health(con, carg);
	return 0;
}

#ifndef WIN32
static void sig_reopen_func(int signr, siginfo_t *si GCC_ATTR_UNUSED_PARAM, void *vuc GCC_ATTR_UNUSED_PARAM)
{
	if(SIGUSR2 == signr)
		reopen_logfile = true;
}
#endif

static void sig_stop_func(int signr, siginfo_t *si GCC_ATTR_UNUSED_PARAM, void *vuc GCC_ATTR_UNUSED_PARAM)
{
	if(SIGINT == signr
#ifndef WIN32
	   || SIGHUP == signr
#endif
	)
		server_running = false;
}

static noinline bool clutch_logfile(void)
{
	int log_to_fd;

	if(!log_fname)
		return true;

	if(-1 == (log_to_fd = open(log_fname, O_WRONLY|O_CREAT|O_APPEND, 0550))) {
		logg_errno(LOGF_CRIT, "opening logfile");
		return false;
	}

	if(STDOUT_FILENO != dup2(log_to_fd, STDOUT_FILENO)) {
		logg_errno(LOGF_CRIT, "mapping stdout");
		return false;
	}

	if(STDERR_FILENO != dup2(log_to_fd, STDERR_FILENO)) {
// TODO: Can we logg here?
		puts("mapping stderr");
		return false;
	}

	close(log_to_fd);
	return true;
}

static __init void parse_cmdl_args(int argc, char **args)
{
	/* seeking our options */
	while(true)
	{
		int opt_val = getopt(argc, args, "hdl:");
		if(-1 == opt_val) break;
		
		switch(opt_val)
		{
		case 'd':
				logg(LOGF_INFO, "will go to background\n");
				be_daemon = true;
				break;
		case 'h':
				logg(LOGF_NOTICE, "Help?\n");
				exit(EXIT_FAILURE);
		case 'l':
#ifndef WIN32
				log_fname = malloc(strlen(optarg) + 1);
				if(!log_fname)
					diedie("couldn't allocate mem for logfile name");
				logg(LOGF_INFO, "will try to log to: %s\n", optarg);
#endif
				break;
		case '?':
				logg_develd_old("unknown OptChar: %c\n", (char)optopt);
				break;
		case ':':
				logg(LOGF_WARN, "missing argument for OptChar: %c\n", (char)optopt);
				break;
		default:
				logg_develd("ooops: %c\n", (char)opt_val);
				break;
		}
	}	
}

static __init void fork_to_background(void)
{
#ifdef WIN32
	/* Windows can not fork. do something else */
#else
	pid_t child;
	int ch_status = 0;
	int tmp_fd = -1;

	switch((child = fork()))
	{
	/* child */
	case 0:
		if(-1 == setsid())
			diedie("becomming session-leader");

		switch((child = fork()))
		{
		/* child */
		case 0:
			/* change the w-dir and better also chroot */
#if 0
			/* chrooting needs some knobs first */
			if(chdir("/"))
			{
				logg_errno("changing working directory");
				return EXIT_FAILURE;
			}
#endif
			/*
			 * connect stdin with dave null,
			 * maybe stderr and stdout
			 */
			if(-1 == (tmp_fd = open(DAVE_NULL, O_RDONLY)))
				diedie("opening " DAVE_NULL);
			
			if(STDIN_FILENO != dup2(tmp_fd, STDIN_FILENO))
				diedie("mapping stdin");

			/*
			 * maybe the log-to-file option already
			 * defined a log-file
			 */
			if(!log_fname)
				log_fname = DAVE_NULL;
			close(tmp_fd);
			break;
		case -1:
			diedie("forking for daemon-mode");
		/* parent */
		default:
			while(true)
				exit(EXIT_SUCCESS);
		}
		break;
	case -1:
		diedie("forking for daemon-mode");
	/* parent */
	default:
		logg_develd_old("own pid %i\tchild %i\n", getpid(), child);
		waitpid(child, &ch_status, 0);
		exit(WEXITSTATUS(ch_status));
	}
#endif
}

static __init_cdata const struct config_item conf_opts[] =
{
	CONF_ITEM("max_connection_sum",   &server.settings.max_connection_sum,       config_parser_handle_int),
	CONF_ITEM("max_hub_sum",          &server.settings.max_hub_sum,              config_parser_handle_int),
	CONF_ITEM("nice_adjust",          &server.settings.nice_adjust,              config_parser_handle_int),
	CONF_ITEM("num_threads",          &server.settings.num_threads,              config_parser_handle_int),
	CONF_ITEM("max_g2_packet_length", &server.settings.max_g2_packet_length,     config_parser_handle_int),
	CONF_ITEM("tcp_send_timeout",     &server.settings.tcp_send_timeout,         config_parser_handle_int),
	CONF_ITEM("qht_max_promille",     &server.settings.qht.max_promille,         config_parser_handle_int),
	CONF_ITEM("default_port",         &server.settings.bind.default_port,        config_parser_handle_int),
	CONF_ITEM("data_root_dir",        &server.settings.data_root_dir,            config_parser_handle_string),
	CONF_ITEM("entropy_source",       &server.settings.entropy_source,           config_parser_handle_string),
	CONF_ITEM("nick_name",            &server.settings.nick.name,                config_parser_handle_string),
	CONF_ITEM("log_time_date_format", &server.settings.logging.time_date_format, config_parser_handle_string),
	CONF_ITEM("gwc_boot_url",         &server.settings.khl.gwc_boot_url,         config_parser_handle_string),
	CONF_ITEM("gwc_cache_filename",   &server.settings.khl.gwc_cache_fname,      config_parser_handle_string),
	CONF_ITEM("khl_dump_filename",    &server.settings.khl.dump_fname,           config_parser_handle_string),
	CONF_ITEM("guid_dump_filename",   &server.settings.guid.dump_fname,          config_parser_handle_string),
	CONF_ITEM("user_name",            &user_name,                                config_parser_handle_string),
	CONF_ITEM("profile_latitude",     &profile.latitude,                         config_parser_handle_string),
	CONF_ITEM("profile_longitude",    &profile.longitude,                        config_parser_handle_string),
	CONF_ITEM("profile_country",      &profile.country,                          config_parser_handle_string),
	CONF_ITEM("profile_city",         &profile.city,                             config_parser_handle_string),
	CONF_ITEM("profile_state",        &profile.state,                            config_parser_handle_string),
	CONF_ITEM("log_add_date_time",    &server.settings.logging.add_date_time,    config_parser_handle_bool),
	CONF_ITEM("use_ipv4",             &server.settings.bind.use_ip4,             config_parser_handle_bool),
	CONF_ITEM("use_ipv6",             &server.settings.bind.use_ip6,             config_parser_handle_bool),
	CONF_ITEM("profile_want_2_send",  &server.settings.profile.want_2_send,      config_parser_handle_bool),
	CONF_ITEM("crawl_send_clients",   &server.settings.nick.send_clients,        config_parser_handle_bool),
	CONF_ITEM("crawl_send_gps",       &server.settings.nick.send_gps,            config_parser_handle_bool),
	CONF_ITEM("qht_compress_internal",&server.settings.qht.compress_internal,    config_parser_handle_bool),
	CONF_ITEM("be_daemon",            &be_daemon,                                config_parser_handle_bool),
	CONF_ITEM("connect_dbus",         &connect_dbus,                             config_parser_handle_bool),
	CONF_ITEM("our_guid",             server.settings.our_guid,                  config_parser_handle_guid),
	CONF_ITEM("encoding_default_in",  &server.settings.default_in_encoding,      config_parser_handle_encoding),
	CONF_ITEM("encoding_default_out", &server.settings.default_out_encoding,     config_parser_handle_encoding),
	CONF_ITEM("encoding_hub_in",      &server.settings.hub_in_encoding,          config_parser_handle_encoding),
	CONF_ITEM("encoding_hub_out",     &server.settings.hub_out_encoding,         config_parser_handle_encoding),
	CONF_ITEM("bind_v4",              &server.settings.bind.ip4,                 config_parser_handle_ip),
	CONF_ITEM("bind_v6",              &server.settings.bind.ip6,                 config_parser_handle_ip),
};

static __init void handle_config(void)
{
	uint16_t s_lat, s_long;
	double d_lat, d_long;
	char *endptr;
	unsigned i;

	/* whats fixed */
	atomic_set(&server.status.act_connection_sum, 0);
	atomic_set(&server.status.act_hub_sum, 0);
	server.status.our_server_upeer = DEFAULT_SERVER_UPEER;

	/*
	 * var settings
	 * first round: apply defaults
	 */
	server.settings.data_root_dir                  = str_it(DEFAULT_DATA_ROOT_DIR);
	server.settings.entropy_source                 = DEFAULT_ENTROPY_SOURCE;
	server.settings.config_file                    = str_it(DEFAULT_CONFIG_FILE);
	server.settings.nick.name                      = DEFAULT_NICK_NAME;
	server.settings.nice_adjust                    = DEFAULT_NICE_ADJUST;
	server.settings.num_threads                    = DEFAULT_NUM_THREADS;
	server.settings.logging.act_loglevel           = DEFAULT_LOGLEVEL;
	server.settings.logging.add_date_time          = DEFAULT_LOG_ADD_TIME;
	server.settings.logging.time_date_format       = DEFAULT_LOG_TIME_FORMAT;
	server.settings.bind.default_port              = DEFAULT_PORT;
	server.settings.bind.use_ip4                   = DEFAULT_USE_IPV4;
	server.settings.bind.use_ip6                   = DEFAULT_USE_IPV6;
	server.settings.default_in_encoding            = DEFAULT_ENC_IN;
	server.settings.default_out_encoding           = DEFAULT_ENC_OUT;
	server.settings.hub_in_encoding                = DEFAULT_HUB_ENC_IN;
	server.settings.hub_out_encoding               = DEFAULT_HUB_ENC_OUT;
	server.settings.max_connection_sum             = DEFAULT_CON_MAX;
	server.settings.max_hub_sum                    = DEFAULT_HUB_MAX;
	server.settings.max_g2_packet_length           = DEFAULT_PCK_LEN_MAX;
	server.settings.tcp_send_timeout               = DEFAULT_TCP_SEND_TIMEOUT;
	server.settings.profile.want_2_send            = DEFAULT_SEND_PROFILE;
	server.settings.khl.gwc_boot_url               = DEFAULT_GWC_BOOT;
	server.settings.khl.gwc_cache_fname            = DEFAULT_GWC_DB;
	server.settings.khl.dump_fname                 = DEFAULT_KHL_DUMP;
	server.settings.guid.dump_fname                = DEFAULT_GUID_DUMP;
	server.settings.qht.compression                = DEFAULT_QHT_COMPRESSION;
	server.settings.qht.compress_internal          = DEFAULT_QHT_COMPRESS_INTERNAL;
	server.settings.qht.max_promille               = DEFAULT_QHT_MAX_PROMILLE;
	server.settings.nick.send_clients              = DEFAULT_NICK_SEND_CLIENTS;
	server.settings.nick.send_gps                  = DEFAULT_NICK_SEND_GPS;

	connect_dbus      = DEFAULT_DBUS;
	be_daemon         = DEFAULT_BE_DAEMON;
	profile.latitude  = DEFAULT_PROFILE_LAT;
	profile.longitude = DEFAULT_PROFILE_LONG;
	profile.country   = DEFAULT_PROFILE_COUNTRY;
	profile.city      = DEFAULT_PROFILE_CITY;
	profile.state     = DEFAULT_PROFILE_STATE;
	profile.gender    = DEFAULT_PROFILE_GENDER;
	profile.age       = time(NULL) / (365 * 24 * 60 * 60); /* We are legion and we are born 1970 */;
	user_name         = DEFAULT_USER;

	/*
	 * second round: read config file
	 */
	if(!config_parser_read(server.settings.config_file, conf_opts, anum(conf_opts)))
	{
		if(ENOENT == errno)
			logg(LOGF_NOTICE, "Config file \"%s\" not found, will continue with defaults\n", server.settings.config_file);
		else {
			logg_errnod(LOGF_ERR, "Error parsing config file \"%s\"", server.settings.config_file);
			die("giving up");
		}
	}

	if(!server.settings.bind.default_port || server.settings.bind.default_port >= 1 << 16) {
		logg(LOGF_NOTICE, "Default port number %u is out of range, using compiled in port number %u\n",
		     server.settings.bind.default_port, DEFAULT_PORT);
		server.settings.bind.default_port = DEFAULT_PORT;
	}

	if(!server.settings.bind.ip4.a)
	{
		server.settings.bind.ip4.a = malloc(sizeof(*server.settings.bind.ip4.a));
		if(!server.settings.bind.ip4.a)
			diedie("could not alloc ip mem");
		server.settings.bind.ip4.num = 1;
		memset(server.settings.bind.ip4.a, 0, sizeof(*server.settings.bind.ip4.a));
		server.settings.bind.ip4.a[0].s.fam              = AF_INET;
		server.settings.bind.ip4.a[0].in.sin_port        = htons(server.settings.bind.default_port);
		server.settings.bind.ip4.a[0].in.sin_addr.s_addr = htonl(DEFAULT_ADDR);
		casalen_ii(&server.settings.bind.ip4.a[0]);
	}
	if(!server.settings.bind.ip6.a)
	{
		server.settings.bind.ip6.a = malloc(sizeof(*server.settings.bind.ip6.a));
		if(!server.settings.bind.ip6.a)
			diedie("could not alloc ip mem");
		server.settings.bind.ip6.num = 1;
		memset(server.settings.bind.ip6.a, 0, sizeof(*server.settings.bind.ip6.a));
		server.settings.bind.ip6.a[0].s.fam              = AF_INET6;
		server.settings.bind.ip6.a[0].in6.sin6_port      = htons(server.settings.bind.default_port);
		server.settings.bind.ip6.a[0].in6.sin6_addr      = DEFAULT_ADDR6;
		casalen_ii(&server.settings.bind.ip6.a[0]);
	}
	for(i = 0; i < server.settings.bind.ip4.num; i++) {
		if(!combo_addr_port(&server.settings.bind.ip4.a[i]))
			combo_addr_set_port(&server.settings.bind.ip4.a[i], htons(server.settings.bind.default_port));
		if(server.settings.bind.use_ip4)
			logg(LOGF_INFO, "Will listen on %pI#\n", &server.settings.bind.ip4.a[i]);
	}
	for(i = 0; i < server.settings.bind.ip6.num; i++) {
		if(!combo_addr_port(&server.settings.bind.ip6.a[i]))
			combo_addr_set_port(&server.settings.bind.ip6.a[i], htons(server.settings.bind.default_port));
		if(server.settings.bind.use_ip6)
			logg(LOGF_INFO, "Will listen on %pI#\n", &server.settings.bind.ip6.a[i]);
	}

	if(server.settings.qht.max_promille > 1000)
		server.settings.qht.max_promille = 1000;

	if(server.settings.tcp_send_timeout >= (20 * 60 * 1000)) {
		logg(LOGF_NOTICE, "You want to set the tcp send timout to %ums, which is to high.\n"
		     "I cowardly refuse to do so and set it to %ums\n",
		     server.settings.tcp_send_timeout, DEFAULT_TCP_SEND_TIMEOUT);
		server.settings.tcp_send_timeout = DEFAULT_TCP_SEND_TIMEOUT;
	}

	emit_emms();
	errno = 0;
	d_lat = strtod(profile.latitude, &endptr);
	if(endptr == profile.latitude || ERANGE == errno) {
		logg(LOGF_NOTICE, "Latitude \"%s\" could not be parsed, setting to 0.0\n", profile.latitude);
		d_lat = 0.0;
		profile.latitude = "0.0";
	}
	errno = 0;
	d_long = strtod(profile.longitude, &endptr);
	if(endptr == profile.longitude || ERANGE == errno) {
		logg(LOGF_NOTICE, "Longitude \"%s\" could not be parsed, setting to 0.0\n", profile.longitude);
		d_long = 0.0;
		profile.longitude = "0.0";
	}
	/*
	 * two times 16 bit: latitude, longitude
	 * normalized:
	 * 180.0*$lat/65535.0-90.0 and 360.0*$lon/65535.0-180.0
	 *
	 * lat = (unsigned)((real_lat + 90.0) * 65535.0 / 180.0)
	 * long = (unsigned)((real_long + 180.0) * 65535.0 / 360.0)
	 */
	d_lat  = (d_lat  +  90.0) * 65535.0 / 180.0;
	d_long = (d_long + 180.0) * 65535.0 / 360.0;
	s_lat  = d_lat  < 0.0 ? 0 : (d_lat  > 65535.0 ? 65535 : (uint16_t)d_lat);
	s_long = d_long < 0.0 ? 0 : (d_long > 65535.0 ? 65535 : (uint16_t)d_long);
	server.settings.nick.lat_long = (uint32_t)s_long | (uint32_t)s_lat << 16;

	server.settings.profile.xml          = NULL;
	/* use the hostname if no nick is supplied */
	if(!server.settings.nick.name)
	{
		char *tmp_str = malloc(255);
		if(!tmp_str)
			return;
		memset(tmp_str, 0, 255);
		if(0 > gethostname(tmp_str, 254)) {
			logg_errno(LOGF_INFO, "couldn't get hostname");
			free(tmp_str);
			return;
		}
		server.settings.nick.name = tmp_str;
	}
	server.settings.nick.len = strlen(server.settings.nick.name) + 1;
	return;
}

static __init void change_the_user(void)
{
#ifdef WIN32
	/*
	 * do something clever with "OpenProcessToken" & "GetTokenInformation"
	 * And yes, windows can change the user, i was wrong, but it is a pain
	 * in the ass...
	 * so, read the cygwin info
	 */
#elif !defined(__CYGWIN__)
	struct passwd *nameinfo;
	errno = 0;
	if(!(nameinfo = getpwnam(user_name)))
		diedie("getting user-entry");

	if(setgid((gid_t) nameinfo->pw_gid))
	{
		logg_errno(LOGF_WARN, "setting GID");
/*
 * Abort disabeled during devel. Maybe configurable with
 * commandline-switch?
 */
//		exit(EXIT_FAILURE);
	}

	if(setuid((uid_t) nameinfo->pw_uid))
	{
		logg_errno(LOGF_WARN, "setting UID");
/*
 * Abort disabeled during devel. Maybe configurable with
 * commandline-switch?
 */
//		exit(EXIT_FAILURE);
	}

	/* wiping the structure for security-reasons */
	memset(nameinfo, 0, sizeof(*nameinfo));
#else
	/* Attention! Our Windau-friends know about users and user-
	 * rights (at least in the not DOS(c)-based Systems), but
	 * they could never Imagine, that a programm wants to change
	 * it's privilages during operation (at least it is possible
	 * from Cygwin). So when run under Cygwin, it is *NOT*
	 * possible to change the uid. The gid could be changed, but
	 * again: this is simply Cygwin-internal emulated. So:
	 * - Get the unpriviledged account you could find. Create
	 *   one or use "Guest". Guest *should* be unprivilaged
	 *   enough (some Win-experts around?). The Guest-account
	 *   has to be simply enabeled.
	 * - The Name of the Guest-account is a localized string,
	 *   thanks Bill (this makes it tricky to handle it from
	 *   a programm)...
	 * - Use the Win/Cygwin-delivered tools to start the program
	 *   *directly* under the wanted account (example: bla.exe->
	 *   richt-click->"Run as" or when setting up a "Service"
	 *   (cygrunservice), there are options for
	 *   "the user to run as").
	 * - Make sure, the used account could read the
	 *   config-files.
	 *
	 *   If all of the above doesn't bother you (as always with
	 *   win-users), do me at least *one* favor:
	 *
	 *   	*NEVER* run this program as "Administrator"!
	 *
	 *   Chose a different account!
	 */
	/* maybe we could at least stop using the admin-account */
	logg_develd("uid = %lu\n", (unsigned long) getuid());
	if(500 == getuid())
		die("We don't want to use the Admin-account!\n");
#endif /* __CYGWIN__ */
}

static __init void setup_resources(void)
{
	size_t i;

	if(pthread_mutex_init(&sock_com_lock, NULL)) {
		logg_errno(LOGF_CRIT, "couldn't init sock_com lock");
		exit(EXIT_FAILURE);
	}

#ifndef WIN32
	/* check and raise our open-file limit */
/*
 * WARNING:
 * On Linux (as of 10.01.2005, 2.4.x & 2.6.10) the vanilla
 * sources (and propably others) *DON'T* respect this setting.
 * They simply cut of at a compile-time constant of 1024. Some
 * distributions seem to aply a patch for this for years now
 * (SuSE, RedHat standard), some not (RedHat Enterprise,
 * Gentoo). So even if this is successfully set, opening fd's
 * could still fail.
 */
#define FD_RESSERVE 40UL
	struct rlimit our_limit;
	if(likely(!getrlimit(RLIMIT_NOFILE, &our_limit)))
	{
		logg_posd(LOGF_DEBUG,
			"FDs: max_con=%lu\thard_limit=%lu %s\tsoft_limit=%lu\n",
			(unsigned long)server.settings.max_connection_sum,
			(unsigned long)our_limit.rlim_max,
			our_limit.rlim_max == RLIM_INFINITY ? "RLIM_INFINITY" : "",
			(unsigned long)our_limit.rlim_cur);

		if(server.settings.max_connection_sum + FD_RESSERVE > our_limit.rlim_cur)
		{
			rlim_t old_cur = our_limit.rlim_cur;

			if(server.settings.max_connection_sum + FD_RESSERVE > our_limit.rlim_max)
			{
				logg(LOGF_NOTICE,
					"Warning: Not enough maximal open Files on your System to handle %lu G2-connections\n\t-> lowering to %lu\n",
					(unsigned long)server.settings.max_connection_sum,
					(unsigned long)our_limit.rlim_max - FD_RESSERVE);
				server.settings.max_connection_sum = our_limit.rlim_max - FD_RESSERVE;
			}

			our_limit.rlim_cur = server.settings.max_connection_sum + FD_RESSERVE;
			if(setrlimit(RLIMIT_NOFILE, &our_limit))
			{
				logg_errnod(LOGF_WARN,
					"setting FD-limit to %lu, lowering max. G2 connections to %lu",
					(unsigned long)server.settings.max_connection_sum,
					(unsigned long)old_cur - FD_RESSERVE);
				server.settings.max_connection_sum = old_cur - FD_RESSERVE;
			}
		}
	}
	else
		logg_errno(LOGF_WARN, "getting FD-limit");
#endif

	server.settings.have_tcp_send_timeout = check_tcp_send_timeout();

	if(pthread_attr_init(&server.settings.t_def_attr)) {
		logg_errno(LOGF_CRIT, "initialising pthread_attr");
		exit(EXIT_FAILURE);
	}

	if(!pthread_attr_getstacksize(&server.settings.t_def_attr, &i))
	{
		/*
		 * set thread stacksize to 64k
		 * Yes, this is constraining, but the default on some BSD.
		 * And by the way looks nicer (have your little proc take 27MB looks
		 * scary, even if its 3 * 8 MB Thread stack, mostly unused (not backed
		 * by real memory) only 12kb used)
		 * For 64 Bit we take more memory, because all stuff is bigger.
		 */
// TODO: also raise to low values
		if(i > (64 * 1024) || sizeof(size_t) > 4)
			pthread_attr_setstacksize(&server.settings.t_def_attr, 64 * 1024 * (sizeof(size_t)/4));
	}

	/* IPC */
	/* open Sockets for IPC */
	for(i = 0; i < THREAD_SUM_COM; i++)
	{
		if(socketpair(PF_UNIX, SOCK_DGRAM, 0, sock_com[i]))
		{
			logg_errnod(LOGF_CRIT, "creating internal Socket num %lu", (unsigned long)i);
			for( ; i; i--)
			{
				if(closesocket(sock_com[i-1][0]))
					logg_errno(LOGF_WARN, "reclaiming FD's");
				if(closesocket(sock_com[i-1][1]))
					logg_errno(LOGF_WARN, "reclaiming FD's");
			}
			/*
			 * calling cleanup here could be bad,
			 * hopefully OS handles it
			 */
			exit(EXIT_FAILURE);
		}
	}

	/* adding socket-fd's to a poll-structure */
	for(i = 0; i < THREAD_SUM_COM; i++) {
		if(!sock_com_add_fd(NULL, NULL, sock_com[i][DIR_OUT], POLLIN, true))
			diedie("preparing internal sockets");
	}

#ifdef __linux__
	{
	char buf[64];
	int fd;
	if(!server.settings.have_tcp_send_timeout)
	{
		fd = open("/proc/sys/net/ipv4/tcp_retries2", O_RDONLY|O_NOCTTY);
		if(0 <= fd)
		{
			memset(buf, 0, sizeof(buf));
			if(read(fd, buf, sizeof(buf) - 1) > 0)
			{
				int num = atoi(buf);
				unsigned bt = 1, bt1 = 1, bt2 = 1;
				for(i = num - 1; i; i--) {
					bt *= 2;
					bt1 += bt > 60 ? 60 : bt;
					bt2 += bt;
				}
				bt1 = DIV_ROUNDUP(bt1, 60);
				bt2 /= 60;
				if(num > 7)
					logg(LOGF_WARN, "Warning: Your \"/proc/sys/net/ipv4/tcp_retries2\" is high (%i), this can lead to a lot of stuck\n"
					                "         dead connection due to long timeouts (approx. %u minutes, up to %u minutes). Please refer\n"
					                "         to the README, section 2.1.2 what this is about!\n", num, bt1, bt2
					);
			}
			close(fd);
		}
	}

	/*
	 * when mem is tight, shoot us first, we are unimportant
	 * and mem hungry...
	 */
	my_snprintf(buf, sizeof(buf), "/proc/%u/oom_score_adj", getpid());
	fd = open(buf, O_WRONLY|O_NOCTTY);
	if(0 > fd)
	{
		my_snprintf(buf, sizeof(buf), "/proc/%u/oom_adj", getpid());
		fd = open(buf, O_WRONLY|O_NOCTTY);
		if(0 > fd)
			return;
		else if(sizeof("15\n") != write(fd, "15\n", sizeof("15\n"))) {
			/* yeah, whatever... */
		}
	} else if(sizeof("1000\n") != write(fd, "1000\n", sizeof("1000\n"))) {
		/* yeah, whatever... */
	}
	close(fd);
	}
#endif
}

static __init void init_prng(void)
{
	/*
	 * WARNING: even if the compiler cries and valgrind screems:
	 * Do NOT memset/init rd!
	 */
	unsigned rd[DIV_ROUNDUP(RAND_BLOCK_BYTE * 2, sizeof(unsigned))];
	bool have_entropy = false;
#ifndef WIN32
	int fin;

	/*
	 * we could use the libc fopen/fread etc. to be portable, but...
	 * A truss on FreeBSD reveales that the libc to buffer I/O is
	 * reading a whoping 4k, instead of our ~32 byte.
	 * This is not only a perfomace thing, if the entropy source is
	 * HQ (and prop. slow, some bytes a sec...) someone will be angry
	 * we eat 4k of best entropy.
	 */
	fin = open(server.settings.entropy_source, O_RDONLY|O_NOCTTY|O_BINARY);
	if(0 > fin) {
		logg_errnod(LOGF_CRIT, "opening entropy source \"%s\"",
		            server.settings.entropy_source);
	} else if(sizeof(rd) != read(fin, rd, sizeof(rd))) {
		logg_errnod(LOGF_CRIT, "reading entropy source \"%s\"",
		            server.settings.entropy_source);
	} else
		have_entropy = true;
	close(fin);
#else
	HCRYPTPROV cprovider_h = (HCRYPTPROV)INVALID_HANDLE_VALUE;
	if(CryptAcquireContext(&cprovider_h, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
		CryptGenRandom(cprovider_h, sizeof(rd), (BYTE *)rd);
		CryptReleaseContext(cprovider_h, 0);
		have_entropy = true;
	}
#endif
	if(!have_entropy)
	{
		struct timeval now;
		unsigned i, t;
		unsigned magic = 0x5BD1E995;

		logg(LOGF_CRIT, "WARNING: We could not gather high quality entropy!\n"
		                "         Will try to rectify, but this may impact the\n"
		                "         SECURITY of this G2 Hub. Please examine the\n"
		                "         situation and configure a proper entropy source!\n");
		gettimeofday(&now, 0);
		t  = getpid() | (getppid() << 16);
		t ^= now.tv_usec << 11;
		t ^= ((t >> 13) ^ (t << 7)) * magic;
		t ^= now.tv_sec << 3;
		t ^= ((t >> 13) ^ (t << 7)) * magic;
// TODO: more entropy sources for the mix?
		/* something from the filesystem? the kernel?
		 * AUX_VECTOR? Enviroment? some random read
		 * from our .text section?
		 */
		for(i = 0; i < anum(rd); i++) {
			unsigned o_rd = rd[i];
			rd[i] ^= t;
			t ^= ((t >> 13) ^ (t << 7)) * magic;
			t ^= o_rd;
			t ^= ((t >> 13) ^ (t << 7)) * magic;
		}
		/*
		 * Even if we could not get entropy, feed rd into the prng.
		 * If everything failes, its "random" stack data, undefined
		 * behaivior for the rescue...
		 */
	}
#ifndef WIN32
	else
		logg(LOGF_INFO, "read %zu bytes of entropy from \"%s\"\n", sizeof(rd),
		     server.settings.entropy_source);
#endif

	random_bytes_init((char *)rd);
}

static void read_uprofile(void)
{
	static const char *prof_str =
		"<?xml version=\"1.0\"?>"
		"<gProfile xmlns=\"http://www.shareaza.com/schemas/GProfile.xsd\">"
			"<gnutella guid=\"%pG\"/>"
			"<identity>"
				"<handle primary=\"%s\"/>"
			"</identity>"
			"<vitals gender=\"%s\" age=\"%u\"/>"
			"<location>"
				"<political country=\"%s\" city=\"%s\" state=\"%s\"/>"
				"<coordinates latitude=\"%s\" longitude=\"%s\"/>"
			"</location>"
			"<avatar path=\"\"/>"
		"</gProfile>";
	char *f_mem;
	size_t f_bytes, r_bytes;

	f_bytes = server.settings.nick.len + strlen(profile.latitude) + strlen(profile.longitude) +
	          strlen(profile.country) + strlen(profile.city) + strlen(profile.state) + strlen(profile.gender) +
	          20 /* age */ + 8 + 4 + 4 + 4 + 12 + 4 /* guid */ + strlen(prof_str) + 100;

	f_mem = malloc(f_bytes);
	if(!f_mem) {
		logg_errno(LOGF_WARN, "allocating mem for profile data");
		return;
	}

	r_bytes = my_snprintf(f_mem, f_bytes, prof_str, server.settings.our_guid, server.settings.nick.name,
	                      profile.gender, profile.age, profile.country, profile.city, profile.state,
	                      profile.latitude, profile.longitude);

	if(r_bytes >= f_bytes) { /* should not happen */
		logg(LOGF_WARN, "failed to generate profile data\n");
		free(f_mem);
		return;
	}

	server.settings.profile.xml        = f_mem;
	server.settings.profile.xml_length = r_bytes;
	return;
}

static __init void adjust_our_niceness(int adjustment)
{
	int cur_nice;

	errno = 0;
	cur_nice = getpriority(PRIO_PROCESS, 0);
	if(-1 == cur_nice && errno != 0) {
		logg_errno(LOGF_NOTICE, "getting niceness failed, continueing. reason");
		return;
	}

	if(0 > setpriority(PRIO_PROCESS, 0, cur_nice + adjustment))
		logg_errno(LOGF_NOTICE, "adjusting niceness failed, continueing, reason");
}

static noinline __init bool startup(int argc, char **args)
{
	struct sigaction old_sas, new_sas;

#ifdef DRD_ME
	DRD_IGNORE_VAR(master_time_now);
	DRD_IGNORE_VAR(server.status.all_abord);
	ANNOTATE_THREAD_NAME("main thread");
#endif

	/*
	 * we have to shourtcut this a little bit, else we would see 
	 * nothing on the sreen until the config is read (and there
	 * are enough Points of failure until that)
	 */
	server.settings.logging.act_loglevel = DEFAULT_LOGLEVEL_START;

	/* first and foremost, before something can go wrong */
	backtrace_init();

#ifdef WIN32
	{
		BOOL (WINAPI *sdep_proc)(DWORD);
		WORD version_req = MAKEWORD(2, 2);
		WSADATA wsa_data;
		int err;

		sdep_proc = (BOOL (WINAPI *)(DWORD))
			GetProcAddress(GetModuleHandle(TEXT("kernel32")), "SetProcessDEPPolicy");
		if(sdep_proc)
			sdep_proc(0x03); /* PROCESS_DEP_ENABLE|PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION */

		err = WSAStartup(version_req, &wsa_data);
		if(err)
			diedie("WSAStartup failed");
	}
#endif

	parse_cmdl_args(argc, args);

	/* setup how we are set-up :) */
	handle_config();

	/* become a daemon if wished (useless while devel but i
	 * stumbled over a snippet) stdin will be /dev/null, stdout
	 * and stderr unchanged, if no log_to_fd defined it will
	 * point to a /dev/null fd.
	 */
	if(be_daemon)
		fork_to_background();

	/* output (log) to file? */
	if(!clutch_logfile())
		return false;

	/*
	 * try to lower our prio, we are a unimportatnt process in respect
	 * to root's ssh session or other stuff
	 */
	adjust_our_niceness(server.settings.nice_adjust);

	g2_set_thread_name(OUR_PROC " main");

	/*
	 * Init our prng, also init the clib prng.
	 * DO NOT USE rand()/random() before this...
	 */
	init_prng();

	/* allocate needed working resources */
	setup_resources();

	g2_conreg_init();
	g2_qk_init();
	/* init khl system */
	if(!g2_khl_init())
		return false;
	/* init guid cache */
	if(!g2_guid_init())
		return false;

	if(0 == memcmp(server.settings.our_guid, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", sizeof(server.settings.our_guid))) {
		guid_generate(server.settings.our_guid);
		logg(LOGF_NOTICE, "Our GUID was not configured, you maybe want to configure a constant GUID\nFor this run i generated one: %pG\n",
		     server.settings.our_guid);
	}
	/* read the user-profile */
	if(server.settings.profile.want_2_send)
		read_uprofile();

	if(connect_dbus && !idbus_init())
		return false;

/* ANYTHING that needs any priviledges should be done before this */
	/* Drop priviledges */
	change_the_user();

	/* make our hzp ready */
	hzp_alloc();

	/* set signalhandler */
	memset(&new_sas, 0, sizeof(new_sas));
	memset(&old_sas, 0, sizeof(old_sas));
	new_sas.sa_handler = SIG_IGN;
	/* See what the Signal-status is, at the moment */
	if(!sigaction(SIGINT, &new_sas, &old_sas))
	{
		/*
		 * if we are already ignoring this signal
		 * (backgrounded?), don't register it
		 */
		if(SIG_IGN != old_sas.sa_handler)
		{
			memset(&new_sas, 0, sizeof(new_sas));
			new_sas.sa_sigaction = sig_stop_func;
			sigemptyset(&new_sas.sa_mask);
			new_sas.sa_flags = SA_SIGINFO;
			if(sigaction(SIGINT, &new_sas, NULL)) {
				logg_pos(LOGF_CRIT, "Error registering signal handler\n");
				return false;
			}
		}
		else
			logg_pos(LOGF_NOTICE, "Not registering Shutdown-handler\n");
	}
	else {
		logg_pos(LOGF_CRIT, "Error changing signal handler\n");
		return false;
	}

#ifndef WIN32
	memset(&new_sas, 0, sizeof(new_sas));
	new_sas.sa_sigaction = sig_reopen_func;
	sigemptyset(&new_sas.sa_mask);
	new_sas.sa_flags = SA_SIGINFO;
	if(sigaction(SIGUSR2, &new_sas, NULL))
		logg_pos(LOGF_WARN, "Error registering log reopen handler\n");
#endif

	server_running = true;
	/* fire up threads */
	if(pthread_create(&main_threads[THREAD_GUP], &server.settings.t_def_attr, (void *(*)(void *))&gup, (void *)&sock_com[THREAD_GUP][DIR_IN])) {
		logg_errno(LOGF_CRIT, "pthread_create gup");
		clean_up_m();
		return false;
	}

	if(pthread_create(&main_threads[THREAD_TIMER], &server.settings.t_def_attr, (void *(*)(void *))&timeout_timer_task, NULL)) {
		logg_errno(LOGF_CRIT, "pthread_create timeout_timer");
		/*
		 * Critical Moment: we could not run further for normal
		 * shutdown, but when clean_up'ed now->undefined behaivor
		 */
		server_running = false;
	}
	/* threads startet */

	server.status.start_time = time(NULL);
	return true;
}

static noinline void clean_up_m(void)
{
	/* Try to clean up as much as possible.
	 * Normally, if a process dies/terminates, the OS should
	 * forget all Ressources assosiated with a proccess, but, to
	 * be nice... (and clean up the valgrind-output) Specially
	 * FD's should be closed, since there are known Systems where
	 * this is not handled prop. by the OS. There is doubt that
	 * this Programm will ever run on such a OS (C99, poll/epoll
	 * etc.), but...
	 */
	struct sock_com *pos, *n;
	size_t i;

	pthread_attr_destroy(&server.settings.t_def_attr);

	for(i = 0; i < THREAD_SUM_COM; i++)
	{
		closesocket(sock_com[i][0]);
			/* output? */
		closesocket(sock_com[i][1]);
			/* output? */
	}

	free((void*)(intptr_t)server.settings.profile.xml);
	list_for_each_entry_safe(pos, n, &sock_com_list, l) {
		list_del(&pos->l);
		free(pos);
	}

	fclose(stdin);
	fclose(stdout); /* get a sync if we output to a file */
	fflush(stderr);
//	fclose(stderr);
}

#if defined(__linux__) && HAVE_DECL_PR_SET_NAME == 1
#include <sys/prctl.h>
void g2_set_thread_name(const char *name)
{
	/*
	 * we don't care for the result, either it works, nice,
	 * or not, also OK...
	 */
	prctl(PR_SET_NAME, name, 0, 0, 0);
}
#else
void g2_set_thread_name(const char *name GCC_ATTRIB_UNUSED)
{
	/* NOP */
}
#endif

struct sock_com *sock_com_add_fd(void (*handler)(struct sock_com *, short), void *data, int fd, short events, bool enabled)
{
	struct sock_com *r = malloc(sizeof(*r));

	if(!r)
		return r;
	INIT_LIST_HEAD(&r->l);
	r->handler = handler;
	r->data = data;
	r->fd = fd;
	r->events = events;
	r->enabled = enabled;
	pthread_mutex_lock(&sock_com_lock);
	list_add(&r->l, &sock_com_list);
	pthread_mutex_unlock(&sock_com_lock);
	return r;
}

void sock_com_delete(struct sock_com *s)
{
	if(!s)
		return;
	pthread_mutex_lock(&sock_com_lock);
	list_del(&s->l);
	pthread_mutex_unlock(&sock_com_lock);
}

struct sock_com *sock_com_fd_find(int fd)
{
	struct sock_com *ret_val = NULL;
	struct sock_com *pos;

	pthread_mutex_lock(&sock_com_lock);
	list_for_each_entry(pos, &sock_com_list, l) {
		if(pos->fd == fd) {
			ret_val = pos;
			break;
		}
	}
	pthread_mutex_unlock(&sock_com_lock);
	return ret_val;
}

static my_pollfd *sock_com_create_pfd(my_pollfd *pfd, unsigned *num, unsigned *len)
{
	struct sock_com *pos;
	unsigned i = 0;

	pthread_mutex_lock(&sock_com_lock);
	list_for_each_entry(pos, &sock_com_list, l)
	{
		if(!pos->enabled)
			continue;
		if(i >= *len)
		{
			my_pollfd *t = realloc(pfd, (*len + 1) * sizeof(my_pollfd));
			if(!t)
				break;
			(*len)++;
			pfd = t;
		}
		pfd[i].fd = pos->fd;
		pfd[i].events = pos->events;
		pfd[i].revents = 0;
		i++;
	}
	pthread_mutex_unlock(&sock_com_lock);
	*num = i;
	return pfd;
}

/*
 * hmpf, ok, we try to say the compiler to include them (if it
 * supports such an anotation), and not optimize them away,
 * while saying splint thats ok that they are unused
 */
/*@unused@*/
static char const ownid[] GCC_ATTR_USED_VAR = "$Own: " DIST " built on " __DATE__ " " __TIME__ " at \"" OUR_SYSTEM_INFO "\" with \"" COMPILER_INFO "\" $";
/*@unused@*/
static char const rcsid_m[] GCC_ATTR_USED_VAR = "$Id: G2MainServer.c,v 1.25 2005/11/05 18:02:45 redbully Exp redbully $";
/* EOF */
