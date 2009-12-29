/*
 * G2MainServer.c
 * This is a server-only implementation for the G2-P2P-Protocol
 * here you will find main()
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
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <unistd.h>
/* Own Includes */
#define _G2MAINSERVER_C
#include "lib/other.h"
#include "G2MainServer.h"
#include "gup.h"
#include "G2UDP.h"
#include "G2Connection.h"
#include "G2ConRegistry.h"
#include "G2Packet.h"
#include "G2PacketSerializer.h"
#include "timeout.h"
#include "G2KHL.h"
#include "G2GUIDCache.h"
#include "G2QueryKey.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/hzp.h"
#include "lib/atomic.h"
#include "lib/backtrace.h"
#include "lib/ansi_prng.h"
#include "lib/config_parser.h"
#include "version.h"
#include "builtin_defaults.h"

/* Thread data */
static pthread_t main_threads[THREAD_SUM];
static int sock_com[THREAD_SUM_COM][2];
static struct pollfd sock_poll[THREAD_SUM_COM + 1];
static volatile sig_atomic_t server_running = true;

/* helper vars */
static bool be_daemon = DEFAULT_BE_DAEMON;
static int log_to_fd = -1;

/* bulk data */
static const char guid_file_name[] = DEFAULT_FILE_GUID;
static const char profile_file_name[] = DEFAULT_FILE_PROFILE;
static const char user_name[] = DEFAULT_USER;

/* Internal prototypes */
static inline void clean_up_m(void);
static inline void parse_cmdl_args(int, char **);
static inline void fork_to_background(void);
static inline void handle_config(void);
static inline void change_the_user(void);
static inline void setup_resources(void);
static void init_prng(void);
static noinline void read_uprofile(void);
static void adjust_our_niceness(int);
static void sig_stop_func(int signr, siginfo_t *, void *);

int main(int argc, char **args)
{
	size_t i, j;
	int extra_fd = -1;
	bool long_poll = true;

	/*
	 * we have to shourtcut this a little bit, else we would see 
	 * nothing on the sreen until the config is read (and there
	 * are enough Points of failure until that)
	 */
	server.settings.logging.act_loglevel = DEFAULT_LOGLEVEL_START;

	/* first and foremost, before something can go wrong */
	backtrace_init();

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
	if(-1 != log_to_fd)
	{
		if(STDOUT_FILENO != dup2(log_to_fd, STDOUT_FILENO)) {
			logg_errno(LOGF_CRIT, "mapping stdout");
			return EXIT_FAILURE;
		}

		if(STDERR_FILENO != dup2(log_to_fd, STDERR_FILENO)) {
// TODO: Can we logg here?
			puts("mapping stderr");
			return EXIT_FAILURE;
		}

		close(log_to_fd);
	}

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

	g2_qk_init();
	/* init khl system */
	if(!g2_khl_init())
		return EXIT_FAILURE;
	/* init guid cache */
	if(!g2_guid_init())
		return EXIT_FAILURE;

/* ANYTHING what need any priviledges should be done before */
	/* Drop priviledges */
	change_the_user();

	/* allocate needed working resources */
	setup_resources();

	/* fire up threads */
	if(pthread_create(&main_threads[THREAD_GUP], &server.settings.t_def_attr, (void *(*)(void *))&gup, (void *)&sock_com[THREAD_GUP][IN])) {
		logg_errno(LOGF_CRIT, "pthread_create gup");
		clean_up_m();
		return EXIT_FAILURE;
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

	/* set signalhandler */
	{
		struct sigaction old_sas, new_sas;
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
				if(sigaction(SIGINT, &new_sas, NULL))
					logg_pos(LOGF_CRIT, "Error registering signal handler\n"), server_running = false;
			}
			else
				logg_pos(LOGF_NOTICE, "Not registering Shutdown-handler\n");
		}
		else
			logg_pos(LOGF_CRIT, "Error changing signal handler\n"), server_running = false;
	}

	/* make our hzp ready */
	hzp_alloc();

	/* main server wait loop */
	while(server_running)
	{
		int num_poll = poll(sock_poll,
				-1 == extra_fd ? THREAD_SUM_COM : THREAD_SUM_COM + 1,
				long_poll ? 11300 : 1100);
		local_time_now = time(NULL);
		switch(num_poll)
		{
		/* Normally: see what has happened */
		default:
			if(!sock_poll[THREAD_SUM_COM].revents) {
				logg_pos(LOGF_INFO, "bytes at the pipes\n");
				server_running = false;
				break;
			}
			/* fall through to g2_khl_tick */
		/* Nothing happened (or just the Timeout) */
		case 0:
			/* all abord? */
			for(i = 0; i < THREAD_SUM; i++)
			{
				logg_develd_old("Up is thread num %lu: %s\n", (unsigned long) i,
						(server.status.all_abord[i]) ? "true" : "false");
				if(!server.status.all_abord[i]) {
					server_running = false;
					logg_pos(LOGF_ERR, "We've lost someone! Going down!\n");
 				}
 			}
			/* service our memleak ;) */
			if(server_running) /* not when someone lost, maybe deadlock */
			{
				extra_fd = -1;
				num_poll = hzp_scan(NORM_HZP_THRESHOLD);
				logg_develd_old("HZP reclaimed chunks: %i\n", num_poll);
				long_poll = g2_khl_tick(&extra_fd);
				sock_poll[THREAD_SUM_COM].fd = extra_fd;
				sock_poll[THREAD_SUM_COM].events = POLLIN|POLLERR|POLLHUP;
				sock_poll[THREAD_SUM_COM].revents = 0;
				/*
				 * first hzp scan, then update (will generate new
				 * mem to free, which we want to linger around one
				 * more round)
				 */
				g2_qht_global_update();
				/* Check if the query key salt needs service */
				g2_qk_tick();
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
		if(0 > send(sock_com[i][OUT], "All lost", sizeof("All lost"), 0))
			logg_errno(LOGF_WARN, "initiating stop");
	}

	/*
	 * wait a moment until all threads have gracefully stoped,
	 * or exit anyway
	 */
	for(i = 0; i < 10; i++)
	{
		bool all_down = false;
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
			fsync(STDOUT_FILENO);
			g2_khl_end(); /* frantic writeout! */
			return EXIT_FAILURE;
		}
		sleep(1);
	}
	
	/* collect the threads */
	pthread_join(main_threads[THREAD_GUP], NULL);
	/* Join THREAD_TIMER??? Hmmm, there was a reason... */

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

	clean_up_m();
	fsync(STDOUT_FILENO);
	return EXIT_SUCCESS;
}

static void sig_stop_func(int signr, siginfo_t *si GCC_ATTR_UNUSED_PARAM, void *vuc GCC_ATTR_UNUSED_PARAM)
{
	if(SIGINT == signr || SIGHUP == signr)
		server_running = false;
}

static inline void parse_cmdl_args(int argc, char **args)
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
				logg(LOGF_INFO, "will log to: %s\n", optarg);
				if(-1 == (log_to_fd = open(optarg, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)))
					diedie("opening logfile");
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

static inline void fork_to_background(void)
{
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
			if(-1 == (tmp_fd = open("/dev/null", O_RDONLY)))
				diedie("opening /dev/null");
			
			if(STDIN_FILENO != dup2(tmp_fd, STDIN_FILENO))
				diedie("mapping stdin");

			/*
			 * maybe the log-to-file option already
			 * defined a log-file
			 */
			if(-1 == log_to_fd)
				log_to_fd = tmp_fd;
			else
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
}

static const struct config_item conf_opts[] =
{
	CONF_ITEM("max_connection_sum", &server.settings.max_connection_sum, config_parser_handle_int),
	CONF_ITEM("max_hub_sum",        &server.settings.max_hub_sum,        config_parser_handle_int),
	CONF_ITEM("nice_adjust",        &server.settings.nice_adjust,        config_parser_handle_int),
};

static inline void handle_config(void)
{
	FILE *config = NULL;
	unsigned int tmp_id_2[sizeof(server.settings.our_guid)];
	unsigned int *tmp_id = tmp_id_2;
	size_t i;

	/* whats fixed */
// TODO: switch according to hub need
	/* set to true on startup, switch when enough hubs */
	server.status.our_server_upeer_needed = false;
	atomic_set(&server.status.act_connection_sum, 0);
	atomic_set(&server.status.act_hub_sum, 0);

	/*
	 * var settings
	 * first round: apply defaults
	 */
	server.settings.data_root_dir = DEFAULT_DATA_ROOT_DIR;
	server.settings.entropy_source = DEFAULT_ENTROPY_SOURCE;
	server.settings.config_file = DEFAULT_CONFIG_FILE;
	server.settings.nice_adjust = DEFAULT_NICE_ADJUST;
	server.settings.logging.act_loglevel = DEFAULT_LOGLEVEL;
	server.settings.logging.add_date_time = DEFAULT_LOG_ADD_TIME;
	server.settings.logging.time_date_format = DEFAULT_LOG_TIME_FORMAT;
	server.settings.bind.ip4.s_fam = AF_INET;
	server.settings.bind.ip4.in.sin_port = htons(DEFAULT_PORT);
	server.settings.bind.ip4.in.sin_addr.s_addr = htonl(DEFAULT_ADDR);
	server.settings.bind.use_ip4 = true;
	server.settings.bind.ip6.s_fam = AF_INET6;
	server.settings.bind.ip6.in6.sin6_port = htons(DEFAULT_PORT);
	server.settings.bind.ip6.in6.sin6_addr = in6addr_any;
	server.settings.bind.use_ip6 = true;
	server.status.our_server_upeer = DEFAULT_SERVER_UPEER;
	server.settings.default_in_encoding = DEFAULT_ENC_IN;
	server.settings.default_out_encoding = DEFAULT_ENC_OUT;
	server.settings.max_connection_sum = DEFAULT_CON_MAX;
	server.settings.max_hub_sum = DEFAULT_HUB_MAX;
	server.settings.default_max_g2_packet_length = DEFAULT_PCK_LEN_MAX;
	server.settings.profile.want_2_send = DEFAULT_SEND_PROFILE;
	server.settings.khl.gwc_boot_url = DEFAULT_GWC_BOOT;
	server.settings.khl.gwc_cache_fname = DEFAULT_GWC_DB;
	server.settings.khl.dump_fname = DEFAULT_KHL_DUMP;
	server.settings.guid.dump_fname = DEFAULT_GUID_DUMP;
	server.settings.qht.compression = DEFAULT_QHT_COMPRESSION;
	server.settings.qht.compress_internal = DEFAULT_QHT_COMPRESS_INTERNAL;

	/*
	 * second round: read config file
	 */
	if(!config_parser_read(server.settings.config_file, conf_opts, anum(conf_opts)))
	{
		if(ENOENT == errno)
			logg(LOGF_NOTICE, "Config file \"%s\" not found, will continue with defaults\n", server.settings.config_file);
		else {
			logg_errnod(LOGF_ERR, "Error parsing config file \"%s\"", server.settings.config_file);
			goto err;
		}
	}

	/* set the GUID */
	if(!(config = fopen(guid_file_name, "r")))
		goto err;

	if(16 != fscanf(config,
		"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		tmp_id, tmp_id+1, tmp_id+2, tmp_id+3, tmp_id+4, tmp_id+5,
		tmp_id+6, tmp_id+7, tmp_id+8, tmp_id+9, tmp_id+10,
		tmp_id+11, tmp_id+12, tmp_id+13, tmp_id+14, tmp_id+15))
		goto err;
		
	for(i = 0; i < sizeof(server.settings.our_guid); i++)
		server.settings.our_guid[i] = (uint8_t)tmp_id[i];

	fclose(config);

	/* read the user-profile */
	server.settings.profile.packet_uprod = NULL;
	server.settings.profile.xml          = NULL;

	if(server.settings.profile.want_2_send)
		read_uprofile();
	return;

err:
	{
		int tmp_errno = errno;
		logg(LOGF_ERR, "Tried to open/read guid-file \"%s\", but it failed!\n"
				"Please check situation. Maybe you want to create one with gen_guid.sh?\n", guid_file_name);
		errno = tmp_errno;
		diedie("opening/reading guid-file");
	}
}

static inline void change_the_user(void)
{
#ifndef __CYGWIN__
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
	 * it's privilages during operation (at leat it is possible
	 * from Cygwin). So when run under Cygwin, it is *NOT*
	 * possible to change the uid. The gid could be changed, but
	 * again: this is simply Cygwin-internal emulated. So:
	 * - Get the unpriviledged account you could find. Create
	 *   one or use "Guest". Guest *should* be unprivilaged
	 *   enough (some Win-experts around?). The Guest-account
	 *   have to be simply enabeled.
	 * - The Name of the Guest-account is a localized string,
	 *   thanks Bill (this makes it tricky to handle it from
	 *   a Programm)...
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

static inline void setup_resources(void)
{
	size_t i;
	struct rlimit our_limit;

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
#define FD_RESSERVE 20UL
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
		logg_errno(LOGF_WARN, "geting FD-limit");

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
				if(close(sock_com[i-1][0]))
					logg_errno(LOGF_WARN, "reclaiming FD's");
				if(close(sock_com[i-1][1]))
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
	for(i = 0; i < THREAD_SUM_COM; i++)
	{
		sock_poll[i].fd = sock_com[i][OUT];
		sock_poll[i].events = POLLIN;
	}	
}

static void init_prng(void)
{
	char rd[RAND_BLOCK_BYTE * 2];
	FILE *fin;

	fin = fopen(server.settings.entropy_source, "rb");
	if(!fin) {
		logg_errnod(LOGF_CRIT, "opening entropy source \"%s\"",
		            server.settings.entropy_source);
	} else if(1 != fread(rd, sizeof(rd), 1, fin)) {
		logg_errnod(LOGF_CRIT, "reading entropy source \"%s\"",
		            server.settings.entropy_source);
	}
	fclose(fin);

// TODO: warn more prominent when we can not gather entropy
// TODO: install some bogus sheme to set up the buffer with entropy

	/*
	 * Even if we could not get entropy, feed fd into the prng
	 * if everything failes, its "random" stack data, undefined
	 * behaivior for the rescue...
	 */

	logg(LOGF_INFO, "read %zu bytes of entropy from \"%s\"\n", sizeof(rd),
	     server.settings.entropy_source);

	random_bytes_init(rd);
}

static void read_uprofile(void)
{
	struct norm_buff buff;
	FILE *prof_file;
	g2_packet_t *uprod, *xml;
	char *wptr, *uprod_mem = NULL, *f_mem;
	ssize_t ret;
	size_t f_bytes, uprod_len;
	bool f_mem_needed = false, uprod_mem_needed = false;

	if(!(prof_file = fopen(profile_file_name, "r"))) {
		logg_errno(LOGF_NOTICE, "opening profile-file");
		return;
	}

	if(fseek(prof_file, 0, SEEK_END)) {
		logg_errno(LOGF_NOTICE, "determinig profile-file size");
		goto read_uprofile_end;
	}

	if((size_t)-1 == (f_bytes = (size_t) ftell(prof_file))) {
		logg_errno(LOGF_NOTICE, "determinig profile-file size");
		goto read_uprofile_end;
	}

	if(fseek(prof_file, 0, SEEK_SET)) {
		logg_errno(LOGF_NOTICE, "determinig profile-file size");
		goto read_uprofile_end;
	}

	g2_packet_local_alloc_init_min();
	uprod = g2_packet_calloc();
	xml   = g2_packet_calloc();
	f_mem = malloc(f_bytes);
	if(!(uprod && xml && f_mem)) {
		logg_errno(LOGF_WARN, "allocating mem for uprod");
		goto read_uprofile_free;
	}

	if(f_bytes != fread(f_mem, 1, f_bytes, prof_file)) {
		logg_errno(LOGF_NOTICE, "reading profile-file");
		goto read_uprofile_free;
	}

	uprod->type = PT_UPROD;
	xml->type   = PT_XML;
	xml->data_trunk.data  = f_mem;
	xml->data_trunk.limit = xml->data_trunk.capacity = f_bytes;
	list_add_tail(&xml->list, &uprod->children);
	ret = g2_packet_serialize_prep_min(uprod);
	if(-1 == ret) {
		logg_pos(LOGF_NOTICE, "serializing uprod");
		goto read_uprofile_free;
	}

	uprod_mem = malloc(ret);
	if(!uprod_mem) {
		logg_errno(LOGF_NOTICE, "allocating mem for uprod");
		goto read_uprofile_free;
	}
	uprod_len = ret;

	buff.pos = 0;
	buff.limit = buff.capacity = NORM_BUFF_CAPACITY;

	wptr = uprod_mem;
	do
	{
		if(!g2_packet_serialize_to_buff(uprod, &buff)) {
			logg_pos(LOGF_NOTICE, "serializing uprod");
			goto read_uprofile_free;
		}
		buffer_flip(buff);
		memcpy(wptr, buffer_start(buff), buffer_remaining(buff));
		wptr += buffer_remaining(buff);
		buffer_clear(buff);
	} while(uprod->packet_encode != ENCODE_FINISHED);

	server.settings.profile.packet_uprod        = uprod_mem;
	server.settings.profile.packet_uprod_length = uprod_len;
	server.settings.profile.xml        = f_mem;
	server.settings.profile.xml_length = f_bytes;
	uprod_mem_needed = true;
	f_mem_needed     = true;

read_uprofile_free:
	if(!uprod_mem_needed)
		free(uprod_mem);
	if(!f_mem_needed)
		free(f_mem);
	g2_packet_free(uprod);
read_uprofile_end:
	fclose(prof_file);
	return;
}

static void adjust_our_niceness(int adjustment)
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

static inline void clean_up_m(void)
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
	size_t i;

	pthread_attr_destroy(&server.settings.t_def_attr);

	for(i = 0; i < THREAD_SUM_COM; i++)
	{
		close(sock_com[i][0]);
			/* output? */
		close(sock_com[i][1]);
			/* output? */
	}

	free((void*)(intptr_t)server.settings.profile.packet_uprod);
	free((void*)(intptr_t)server.settings.profile.xml);

	fclose(stdin);
	fclose(stdout); // get a sync if we output to a file
	fclose(stderr);
}

#ifdef __linux__
#include <sys/prctl.h>
void g2_set_thread_name(const char *name)
{
	/*
	 * we don't care for the result, either it works, nice
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

/*
 * hmpf, ok, we try to say the compiler to include them (if it
 * supports such an anotation), and not optimize them away,
 * while saying splint thats ok that they are unused
 */
/*@unused@*/
static char const ownid[] GCC_ATTR_USED_VAR = "$Own: " DIST " built on " __DATE__ " " __TIME__ " at \"" SYSTEM_INFO "\" with \"" COMPILER_INFO "\" $";
/*@unused@*/
static char const rcsid_m[] GCC_ATTR_USED_VAR = "$Id: G2MainServer.c,v 1.25 2005/11/05 18:02:45 redbully Exp redbully $";
/* EOF */
