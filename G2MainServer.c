/*
 * G2MainServer.c
 * This is a server-only implementation for the G2-P2P-Protocol
 * here you will find main()
 *
 * Copyright (c) 2004-2008 Jan Seiffert
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
#include <sys/resource.h>
#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <unistd.h>
/* Own Includes */
#define _G2MAINSERVER_C
#include "other.h"
#include "G2MainServer.h"
#include "G2Acceptor.h"
#include "G2Handler.h"
#include "G2UDP.h"
#include "G2Connection.h"
#include "timeout.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/hzp.h"
#include "lib/atomic.h"
#include "lib/backtrace.h"
#include "version.h"
#include "builtin_defaults.h"

/* Thread data */
static pthread_t main_threads[THREAD_SUM];
static int sock_com[THREAD_SUM_COM][2];
static int accept_2_handler[2];
static struct pollfd sock_poll[THREAD_SUM_COM];
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
static inline void read_uprofile(void);
static void sig_stop_func(int signr, siginfo_t *, void *);
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
static void test_cpu(void);
#else
static void test_cpu(void) { }
#endif

int main(int argc, char **args)
{
	size_t i, j;

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

	/* 
	 * sometimes things really stink, like cpu-bugs...
	 * x86 specific code to warn the user that atomic ops may
	 * "fail". Would have liked to hide it somewhere in lib/arch,
	 * but i either put it in a file where it does not belong, or
	 * it would not get picked up.
	 */
	test_cpu();

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
		if(STDOUT_FILENO != dup2(log_to_fd, STDOUT_FILENO))
		{
			logg_errno(LOGF_CRIT, "mapping stdout");
			return EXIT_FAILURE;
		}
				
		if(STDERR_FILENO != dup2(log_to_fd, STDERR_FILENO))
		{
// TODO: Can we logg here?
			puts("mapping stderr");
			return EXIT_FAILURE;
		}

		close(log_to_fd);
	}

/* ANYTHING what need any priviledges should be done before */
	/* Drop priviledges */
	change_the_user();

	/* allocate needed working resources */
	setup_resources();

	/* fire up threads */
	if(pthread_create(&main_threads[THREAD_ACCEPTOR], &server.settings.t_def_attr, (void *(*)(void *))&G2Accept, (void *)&sock_com[THREAD_ACCEPTOR][IN]))
	{
		logg_errno(LOGF_CRIT, "pthread_create G2Accept");
		clean_up_m();
		return EXIT_FAILURE;
	}

	if(pthread_create(&main_threads[THREAD_HANDLER], &server.settings.t_def_attr, (void *(*)(void *))&G2Handler, (void *)&sock_com[THREAD_HANDLER][IN]))
	{
		logg_errno(LOGF_CRIT, "pthread_create G2Handler");
		/*
		 * Critical Moment: we could not run further for normal
		 * shutdown, but when clean_up'ed now->undefined behaivor
		 */
		return EXIT_FAILURE;
	}

	if(pthread_create(&main_threads[THREAD_UDP], &server.settings.t_def_attr, (void *(*)(void *))&G2UDP, (void *)&sock_com[THREAD_UDP][IN]))
	{
		logg_errno(LOGF_CRIT, "pthread_create G2UDP");
		server_running = false;
	}
	
	if(pthread_create(&main_threads[THREAD_TIMER], &server.settings.t_def_attr, (void *(*)(void *))&timeout_timer_task, NULL))
	{
		logg_errno(LOGF_CRIT, "pthread_create timeout_timer");
		server_running = false;
	}
	/* threads startet */

	/* send the pipe between Acceptor and Handler */
	if(sizeof(accept_2_handler[IN]) != send(sock_com[THREAD_ACCEPTOR][OUT], &accept_2_handler[IN], sizeof(accept_2_handler[IN]), 0))
	{
		logg_errno(LOGF_CRIT, "sending IPC Pipe");
		server_running = false;
	}
	if(sizeof(accept_2_handler[OUT]) != send(sock_com[THREAD_HANDLER][OUT], &accept_2_handler[OUT], sizeof(accept_2_handler[OUT]), 0))
	{
		logg_errno(LOGF_CRIT, "sending IPC Pipe");
		server_running = false;
	}

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

	/* main server wait loop */
	while(server_running)
	{
		int num_poll = poll(sock_poll, THREAD_SUM_COM, 110000);
		switch(num_poll)
		{
		/* Normally: see what has happened */
		default:
			logg_pos(LOGF_INFO, "bytes at the pipes\n");
			server_running = false;
			break;
		/* Nothing happened (or just the Timeout) */
		case 0:
			/* all abord? */
			for(i = 0; i < THREAD_SUM; i++)
			{
				logg_develd_old("Up is thread num %lu: %s\n", (unsigned long) i,
						(server.status.all_abord[i]) ? "true" : "false");
				if(!server.status.all_abord[i])
				{
					server_running = false;
					logg_pos(LOGF_ERR, "We've lost someone! Going down!\n");
 				}
 			}
			/* service our memleak ;) */
			if(server_running) /* not when someone lost, maybe deadlock */
			{
				num_poll = hzp_scan(NORM_HZP_THRESHOLD);
				logg_develd("HZP reclaimed chunks: %i\n", num_poll);
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
			else
			{
				all_down = false;
				break;
			}
		}

		if(all_down) break;
		else if(9 == i)
		{
			logg_pos(LOGF_ERR, "not all gone down! Aborting!\n");
			fsync(STDOUT_FILENO);
			return EXIT_FAILURE;
		}
		sleep(1);
	}
	
	/* collect the threads */
	pthread_join(main_threads[THREAD_ACCEPTOR], NULL);
	pthread_join(main_threads[THREAD_HANDLER], NULL);
	pthread_join(main_threads[THREAD_UDP], NULL);

	clean_up_m();
	fsync(STDOUT_FILENO);
	return EXIT_SUCCESS;
}

static void sig_stop_func(int signr, GCC_ATTR_UNUSED_PARAM(siginfo_t, *si), GCC_ATTR_UNUSED_PARAM(void, *vuc))
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

static inline void handle_config(void)
{
	FILE *config = NULL;
	unsigned int tmp_id_2[sizeof(server.settings.our_guid)];
	unsigned int *tmp_id = tmp_id_2;
	size_t i;

	/* whats fixed */
	server.status.our_server_upeer_needed = true;
	atomic_set(&server.status.act_connection_sum, 0);

// TODO: read from config files
	/* var settings */
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
	server.settings.default_max_g2_packet_length = DEFAULT_PCK_LEN_MAX;
	server.settings.want_2_send_profile = DEFAULT_SEND_PROFILE;

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
	packet_uprod = NULL;

	if(server.settings.want_2_send_profile)
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
	if(!getrlimit(RLIMIT_NOFILE, &our_limit))
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
		 */
		if(i > (64 * 1024))
			pthread_attr_setstacksize(&server.settings.t_def_attr, 64 * 1024);
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

	/* open pipes for IPC */
	if(0 > pipe(accept_2_handler))
	{
		logg_errno(LOGF_CRIT, "opening acceptor-handler pipe");
		clean_up_m();
		exit(EXIT_FAILURE);
	}

	/* adding socket-fd's to a poll-structure */
	for(i = 0; i < THREAD_SUM_COM; i++)
	{
		sock_poll[i].fd = sock_com[i][OUT];
		sock_poll[i].events = POLLIN;
	}	
}

static inline void read_uprofile(void)
{
	FILE *prof_file;
	char *wptr;
	size_t xml_length;
	size_t uprod_length;
	size_t i;
	size_t f_bytes;
	uint8_t xml_length_length;
	uint8_t uprod_length_length;
	uint8_t tmp_cbyte;

	if(!(prof_file = fopen(profile_file_name, "r")))
	{
		logg_errno(LOGF_NOTICE, "opening profile-file");
		return;
	}

	if(fseek(prof_file, 0, SEEK_END))
	{
		logg_errno(LOGF_NOTICE, "determinig profile-file size");
		goto read_uprofile_end;
	}

	if((size_t)-1 == (f_bytes = (size_t) ftell(prof_file)))
	{
		logg_errno(LOGF_NOTICE, "determinig profile-file size");
		goto read_uprofile_end;
	}

	if(fseek(prof_file, 0, SEEK_SET))
	{
		logg_errno(LOGF_NOTICE, "determinig profile-file size");
		goto read_uprofile_end;
	}

	/* calculate inner /XML length */
	
	if(f_bytes <= 0x0000FFL)
		xml_length_length = 1;
	else if(f_bytes <= 0x00FFFFL)
		xml_length_length = 2;
	else if(f_bytes <= 0xFFFFFFL)
		xml_length_length = 3;
	else
	{
		logg_pos(LOGF_NOTICE, "profile-file too big\n");
		goto read_uprofile_end;
	}

	xml_length = 1;                  /* /XML-Control-byte */
	xml_length += xml_length_length; /* /XML-length */
	xml_length += 3;                 /* /XML-Type-length */
	xml_length += f_bytes;           /* /XML-Payload */

	if(xml_length <= 0x0000FF)
		uprod_length_length = 1;
	else if(xml_length <= 0x00FFFF)
		uprod_length_length = 2;
	else if(xml_length <= 0xFFFFFF)
		uprod_length_length = 3;
	else
	{
		logg_pos(LOGF_NOTICE, "profile-file too big\n");
		goto read_uprofile_end;
	}

	uprod_length = 1;                   /* /UPROD-Control-byte */
	uprod_length += uprod_length_length;/* /UPROD-length */
	uprod_length += 5;                  /* /UPROD-Type-length */
	uprod_length += xml_length;         /* /UPROD-child-length */

// TODO: make it send-buffer-capacity transparent
	if(uprod_length > NORM_BUFF_CAPACITY)
	{
		logg_pos(LOGF_NOTICE, "profile-file too big\n");
		goto read_uprofile_end;
	}

	if(!(packet_uprod = calloc(1, uprod_length)))
	{
		logg_errno(LOGF_NOTICE, "allocating profile-packet memory");
		goto read_uprofile_end;
	}

	packet_uprod_length = uprod_length;
	
	wptr = packet_uprod;
	tmp_cbyte = 0;
	tmp_cbyte |= (uprod_length_length << 6) & 0xFF;
	tmp_cbyte |= (4) << 3;
	tmp_cbyte |= 0x04;
	*wptr++ = (char)tmp_cbyte;
	
	for(i = 0; i < uprod_length_length; i++, wptr++)
		*wptr = (char)((xml_length >> (i*8)) & 0xFF);
	
	*wptr++ = 'U'; *wptr++ = 'P'; *wptr++ = 'R';
	*wptr++ = 'O'; *wptr++ = 'D';

	tmp_cbyte = 0;
	tmp_cbyte |= xml_length_length << 6;
	tmp_cbyte |= (2) << 3;
	*wptr++ = (char)tmp_cbyte;
	
	for(i = 0; i < xml_length_length; i++, wptr++)
		*wptr = (char)((f_bytes >> (i*8)) & 0xFF);
	
	*wptr++ = 'X'; *wptr++ = 'M'; *wptr++ = 'L';

	if(f_bytes != fread(wptr, 1, f_bytes, prof_file))
	{
		logg_errno(LOGF_NOTICE, "reading profile-file");
		free(packet_uprod);
		packet_uprod = NULL;
	}

read_uprofile_end:
	fclose(prof_file);
	return;
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

	close(accept_2_handler[0]);
	close(accept_2_handler[1]);

	if(packet_uprod)
		free(packet_uprod);

	fclose(stdin);
	fclose(stdout); // get a sync if we output to a file
	fclose(stderr);
}

#if defined(__i386__) || defined(__x86_64__)
enum cpu_vendor
{
	X86_VENDOR_OTHER,
	X86_VENDOR_INTEL,
	X86_VENDOR_AMD,
};

static struct cpuinfo
{
	enum cpu_vendor vendor;
	union v_str
	{
		char s[13];
		uint32_t r[3];
	} vendor_str;
	uint32_t family;
	uint32_t model;
	uint32_t stepping;
	int count;
} our_cpu;

struct cpuid_regs
{
	uint32_t	eax, ebx, ecx, edx;
};

static always_inline size_t read_flags(void)
{
	size_t f;

	asm volatile(
		"pushf\n\t"
		"pop %0\n\t"
		: "=r" (f)
	);

	return f;
}

static always_inline void write_flags(size_t f)
{
	asm volatile(
		"push %0\n\t"
		"popf\n\t"
		: : "r" (f) : "cc"
	);
}

static inline bool has_cpuid(void)
{
	const size_t id_mask = (1 << 21);
	size_t f;

	f = read_flags();
	write_flags(f | id_mask);
	f = read_flags() & id_mask;
	if (!f)
		return false;
	f = read_flags() & ~id_mask;
	write_flags(f);
	f = read_flags() & id_mask;
	if (f)
		return false;

	return true;
}

static always_inline void cpuid(struct cpuid_regs *regs, uint32_t func)
{
	/* save ebx around cpuid call, PIC code needs it */
	asm volatile (
		"xchg	%1, %%ebx\n\t"
		"cpuid\n\t"
		"xchg	%1, %%ebx\n"
		: /* %0 */ "=a" (regs->eax),
		  /* %1 */ "=r" (regs->ebx),
		  /* %2 */ "=c" (regs->ecx),
		  /* %4 */ "=d" (regs->edx)
		: /* %5 */ "0" (func)
		: "cc"
		);
}

#define CPUID_STEPPING(x)	((x) & 0x0F)
#define CPUID_MODEL(x)	(((x) >> 4) & 0x0F)
#define CPUID_FAMILY(x)	(((x) >> 8) & 0x0F)
#define CPUID_XMODEL(x)	(((x) >> 16) & 0x0F)
#define CPUID_XFAMILY(x)	(((x) >> 20) & 0xFF)

static void identify_vendor(struct cpuinfo *);

void test_cpu(void)
{
	struct cpuid_regs a;
	
	if(unlikely(!has_cpuid())) {
		/* distinguish 386 from 486, same trick for EFLAGS bit 18 */
		strcpy(our_cpu.vendor_str.s, "{3|4}86??");
		logg_pos(LOGF_DEBUG, "Looks like this is an CPU older Pentium I???\n");
		return;
	}

	cpuid(&a, 0);
	our_cpu.vendor_str.r[0] = a.ebx;
	our_cpu.vendor_str.r[1] = a.edx;
	our_cpu.vendor_str.r[2] = a.ecx;
	our_cpu.vendor_str.s[12] = '\0';
	identify_vendor(&our_cpu);


	cpuid(&a, 1);
	our_cpu.family   = CPUID_FAMILY(a.eax);
	our_cpu.model    = CPUID_MODEL(a.eax);
	our_cpu.stepping = CPUID_STEPPING(a.eax);

	switch(our_cpu.vendor)
	{
	case X86_VENDOR_INTEL:
		if(our_cpu.family == 0x0F || our_cpu.family == 0x06)
			our_cpu.model += CPUID_XMODEL(a.eax) << 4;
		break;
	case X86_VENDOR_AMD:
		if(our_cpu.family == 0x0F)
			our_cpu.model += CPUID_XMODEL(a.eax) << 4;
		break;
	default:
		if(our_cpu.model == 0x0F)
			our_cpu.model += CPUID_XMODEL(a.eax) << 4;
		break;
	}

	if(our_cpu.family == 0x0F)
		our_cpu.family += CPUID_XFAMILY(a.eax);

	logg_posd(LOGF_DEBUG, "Vendor: \"%s\" Family: %d Model: %d Stepping: %d\n",
			our_cpu.vendor_str.s, our_cpu.family, our_cpu.model, our_cpu.stepping);

	if(our_cpu.vendor != X86_VENDOR_AMD || our_cpu.family != 0x0F)
		return;

# ifdef __linux__
#  define S_STR "\nprocessor"
#  define S_SIZE (sizeof(S_STR)-1)
	{
		char read_buf[4096];
		FILE *f;
		char *w_ptr;
		size_t ret;

		f = fopen("/proc/cpuinfo", "r");
		/* if we couldn't read it, simply check*/
		if(!f)
			goto check;

		read_buf[0] = '\n';
		ret = fread(read_buf + 1, 1, sizeof(read_buf) - 2, f);
		read_buf[ret + 1] = '\0';
		w_ptr = read_buf;

		while((w_ptr = strstr(w_ptr, S_STR)))
		{
			our_cpu.count++;
			w_ptr += S_SIZE;
		}

		fclose(f);

		/* if we only have 1 CPU, no problem */
		if(1 == our_cpu.count)
			return;
	}
check:
# endif
	/* 
	 * early AMD Opterons and everything remotely derived from them
	 * seem to drop the ball on read-modify-write instructions after a
	 * locked instruction (missing internal lfence, they say). Ok, you
	 * also need > 1 Processor.
	 * This is unfortunatly all wild speculation, no (visible) Errata,
	 * no info, but:
	 * Google speaks of Opteron Rev. E Model 32..63 in their perftools stuff
	 * MySQL seem to hit it on 64Bit
	 * Slowlaris trys to detect it, marks everything affected < Model 0x40
	 *  (but since they don't build mashines with every avail. AMD
	 *  processor (only Servers with Opterons...), this smells like a
	 *  sledgehammer)
	 */
	if(our_cpu.vendor == X86_VENDOR_AMD &&
	   our_cpu.family == 0x0F &&
		our_cpu.model  >= 32 &&
		our_cpu.model  <= 63)
		logg(LOGF_WARN, "Warning! Your specific CPU can frobnicate interlocked instruction sequences.\nThis may leed to errors or crashes. But there is a chance i frobnicatet it myself;-)\n");

	return;
}

static void identify_vendor(struct cpuinfo *cpu)
{
	if(!strcmp(cpu->vendor_str.s, "GenuineIntel"))
		cpu->vendor = X86_VENDOR_INTEL;
	else if(!strcmp(cpu->vendor_str.s, "AuthenticAMD"))
		cpu->vendor = X86_VENDOR_AMD;
	else
		cpu->vendor = X86_VENDOR_OTHER;
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
//EOF
