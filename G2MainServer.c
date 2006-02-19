/*
 * G2MainServer.c
 * This is a server-only implementation for the G2-P2P-Protocol
 * here you will find main()
 *
 * Copyright (c) 2004,2005,2006 Jan Seiffert
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
 * $Id: G2MainServer.c,v 1.25 2005/11/05 18:02:45 redbully Exp redbully $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
//System includes
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <unistd.h>
#if defined __GLIBC__ && defined WANT_BACKTRACES
# include <execinfo.h>
# include <ucontext.h>
# include <malloc.h>
# include <setjmp.h>
# ifdef __linux__
#  include <syscall.h>
# endif
#endif
//#include <zlib.h>
// other
#include "other.h"
//Own Includes
#define _G2MAINSERVER_C
#include "G2MainServer.h"
#include "G2Acceptor.h"
#include "G2Handler.h"
#include "G2UDP.h"
#include "G2Connection.h"
#include "lib/sec_buffer.h"
#include "lib/log_facility.h"
#include "lib/hzp.h"
#include "lib/atomic.h"
#include "version.h"
#include "builtin_defaults.h"

//Thread data
static pthread_t main_threads[THREAD_SUM];
static int sock_com[THREAD_SUM][2];
static int accept_2_handler[2];
static struct g2_con_info *free_cons = NULL;
static pthread_mutex_t free_cons_mutex;
static struct pollfd sock_poll[THREAD_SUM];
static volatile sig_atomic_t server_running = true;

//
static bool be_daemon = DEFAULT_BE_DAEMON;
static int log_to_fd = -1;

//
static const char guid_file_name[] = DEFAULT_FILE_GUID;
static const char profile_file_name[] = DEFAULT_FILE_PROFILE;
static const char user_name[] = DEFAULT_USER;

// Internal prototypes
static inline void clean_up(void);
static inline void parse_cmdl_args(int, char **);
static inline void fork_to_background(void);
static inline void handle_config(void);
static inline void change_the_user(void);
static inline void setup_resources(void);
static inline void read_uprofile(void);
static void sig_stop_func(int signr);
static void sig_segv_print(int signr, siginfo_t *, void *);

int main(int argc, char **args)
{
	size_t i, j;

#ifdef WANT_BACKTRACES
	{
		struct sigaction sas;
		memset(&sas, 0, sizeof(sas));
		sas.sa_sigaction = sig_segv_print;
		sas.sa_flags = SA_SIGINFO;
		if(sigaction(SIGSEGV, &sas, NULL))
			logg_pos(LOGF_NOTICE, "Unable to register SIGSEGV-handler\n");
		if(sigaction(SIGILL, &sas, NULL))
			logg_pos(LOGF_NOTICE, "Unable to register SIGILL-handler\n");
		if(sigaction(SIGBUS, &sas, NULL))
			logg_pos(LOGF_NOTICE, "Unable to register SIGBUS-handler\n");
		if(sigaction(SIGFPE, &sas, NULL))
			logg_pos(LOGF_NOTICE, "Unable to register SIGFPE-handler\n");
	}
#endif
	
	// we have to shourtcut this a little bit, else we would see 
	// nothing on the sreen until the config is read (and there
	// are enough Points of failure until that)
	server.settings.logging.act_loglevel = DEFAULT_LOGLEVEL_START;

	parse_cmdl_args(argc, args);

	// setup how we are set-up :)
	handle_config();

	// become a daemon if wished (useless while devel but i
	// stumbled over a snippet) stdin will be /dev/null, stdout
	// and stderr unchanged, if no log_to_fd defined it will
	// point to a /dev/null fd.
	if(be_daemon)
		fork_to_background();

	// output (log) to file?
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
	// Drop priviledges
	change_the_user();

	// allocate needed working resources
	setup_resources();
	
	//fire up threads
	if(pthread_create(&main_threads[THREAD_ACCEPTOR], NULL, (void *(*)(void *))&G2Accept, (void *)&sock_com[THREAD_ACCEPTOR][IN]))
	{
		logg_errno(LOGF_CRIT, "pthread_create G2Accept");
		clean_up();
		return EXIT_FAILURE;
	}

	if(pthread_create(&main_threads[THREAD_HANDLER], NULL, (void *(*)(void *))&G2Handler, (void *)&sock_com[THREAD_HANDLER][IN]))
	{
		logg_errno(LOGF_CRIT, "pthread_create G2Handler");
		// Critical Moment: we could not run further for normal
		// shutdown, but when clean_up'ed now->undefined behaivor
		return EXIT_FAILURE;
	}

	if(pthread_create(&main_threads[THREAD_UDP], NULL, (void *(*)(void *))&G2UDP, (void *)&sock_com[THREAD_UDP][IN]))
	{
		logg_errno(LOGF_CRIT, "pthread_create G2UDP");
		server_running = false;
	}

	// send the pipe between Acceptor and Handler
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

	// set signalhandler
	{
		// See what the Signal-status is, at the moment
		sighandler_t old_sig_handler = signal(SIGINT, SIG_IGN);
		if(SIG_ERR != old_sig_handler)
		{
			// if we are already ignoring this signal
			// (backgrounded?), don't register it
			if(SIG_IGN != old_sig_handler)
			{
				if(SIG_ERR == signal(SIGINT, sig_stop_func))
					logg_pos(LOGF_CRIT, "Error registering signal handler\n"), server_running = false;
			}
			else
				logg_pos(LOGF_NOTICE, "Not registering Shutdown-handler\n");
		}
		else
			logg_pos(LOGF_CRIT, "Error changing signal handler\n"), server_running = false;
	//	if(SIG_ERR == signal(SIGHUP, sig_stop_func))
	//			server_running = false;
	}

	/* main server wait loop */
	while(server_running)
	{
		int num_poll = poll(sock_poll, THREAD_SUM, 11000);
		switch(num_poll)
		{
		// Normally: see what has happened
		default:
			logg_pos(LOGF_INFO, "was durch die pipe gekommen\n");
			server_running = false;
			break;
		// Nothing happened (or just the Timeout)
		case 0:
			// all abord?
			for(i = 0; i < THREAD_SUM; i++)
			{
				logg_develd_old("Up is thread num %lu: %s\n", (unsigned long) i, (all_abord[i]) ? "true" : "false");
				if(!server.status.all_abord[i])
				{
					server_running = false;
					logg_pos(LOGF_ERR, "We've lost someone! Going down!\n");
 				}
 			}
		//	putchar('+');
		//	fflush(stdout);
			break;
		// Something bad happened
		case -1:
			if(EINTR == errno) break;
			// Print what happened
			logg_errno(LOGF_ERR, "poll");
			//and get out here (at the moment)
			server_running = false;
			//return EXIT_FAILURE;
			break;
		}
	}

	// send all threads the abort-code
	for(i = 0; i < THREAD_SUM; i++)
	{
		if(0 > send(sock_com[i][OUT], "All lost", sizeof("All lost"), 0))
			logg_errno(LOGF_WARN, "initiating stop");
	}
	
	// wait a moment until all threads have gracefully stoped,
	// or exit anyway
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
			return EXIT_FAILURE;
		}
		sleep(1);
	}
	
	//collect the threads
	pthread_join(main_threads[THREAD_ACCEPTOR], NULL);
	pthread_join(main_threads[THREAD_HANDLER], NULL);
	pthread_join(main_threads[THREAD_UDP], NULL);

	clean_up();
	return EXIT_SUCCESS;
}

static void sig_stop_func(int signr)
{
	// if reregistering the Signal-hanlder fails,
	if(SIG_ERR == signal(SIGINT, sig_stop_func))
		exit(EXIT_FAILURE);
	// droping it all to the Ground, since we have to be
	// carefull while in a signal-handler

	if(SIGINT == signr || SIGHUP == signr)
		server_running = false;
}

#ifdef WANT_BACKTRACES
static void sig_segv_print(int signr, siginfo_t *si, void *vuc)
{
	static pthread_mutex_t bt_mutex = PTHREAD_MUTEX_INITIALIZER;
	static sigjmp_buf catch_sigsegv;
	static volatile sig_atomic_t critical = 0;
	static char path[4096];
	const char *osl = NULL, *isl = NULL;
	char *wptr = NULL; 
	struct ucontext *uc = vuc;
# ifdef __GLIBC__
#  define BACKTRACE_DEPTH (sizeof(path) / sizeof(void *))
	void **buffer = (void **) path;
# endif
	int stderrfd = fileno(stderr), ret_val = 0, i;

# ifdef __sun__
#  define DEBUG_CMD \
 "/usr/proc/bin/pstack "
# else
#  define DEBUG_CMD \
 "echo 'info threads\nthread apply all bt\ndetach\nquit\n' | gdb -batch\
 -x /dev/stdin "
# endif
	
# define DEATHSTR_1 "We crashed hard!!! Please fasten your seat belts...\n\
(you may see unrelated program output spilling out of buffers)\n"
	write(stderrfd, DEATHSTR_1, str_size(DEATHSTR_1));
	/* lock out the other threads, if another one apaers, simply deadlock */
	if(SIGSEGV == signr && critical)
	{
		/* maybe we caught a sigsegv in our own sighandler, try to resume */
		siglongjmp(catch_sigsegv, 666);
	}
	else
	{
		if(pthread_mutex_lock(&bt_mutex))
		{
# define DEATHSTR_2 "Another thread crashed and something went wrong.\
So no BT, maybe a core.\n"
			write(stderrfd, DEATHSTR_2, str_size(DEATHSTR_2));
			goto out;
		}
	}
	fflush(NULL);

	/* first dump on Info what we can find ourself, maybe no debugger */
	/* signal name and reason */
	switch(signr)
	{
	case SIGSEGV:
		osl = "SIGSEGV -> EIIYYY!!\nSurrounded, outnumbered and outgunned";
		switch(si->si_code)
		{
		case SEGV_MAPERR:
			isl = "address not mapped to object"; break;
		case SEGV_ACCERR:
			isl = "invalid permissions for mapped object"; break;
		default:
			isl = "Unknown SIKSEGV problem"; break;
		}
		break;
	case SIGILL:
		osl = "SIGILL -> OHOH!!\nMan the lifeboats";
		switch(si->si_code)
		{
		case ILL_ILLOPC:
			isl = "illegal opcode"; break;
		case ILL_ILLOPN:
			isl = "illegal operand"; break;
		case ILL_ILLADR:
			isl = "illegal addressing mode"; break;
	   case ILL_ILLTRP:
			isl = "illegal trap"; break;
		case ILL_PRVOPC:
			isl = "privileged opcode"; break;
		case ILL_PRVREG:
			isl = "privileged register"; break;
		case ILL_COPROC:
			isl = "coprocessor error"; break;
		case ILL_BADSTK:
			isl = "internal stack error"; break;
		default:
			isl = "Unknown SIGILL problem"; break;
		}
		break;
	case SIGBUS:
		osl = "SIGBUS -> grml...\nNo one expects the Spanish inquesition";
		switch(si->si_code)
		{
		case BUS_ADRALN:
			isl = "invalid address alignment"; break;
		case BUS_ADRERR:
			isl = "non-existent physical address"; break;
		case BUS_OBJERR:
			isl = "object specific hardware error"; break;
		default:
			isl = "Unknown SIGBUS problem"; break;
		}
		break;
	case SIGFPE:
		osl = "SIGFPE -> Whoot!°\nWho the fuck is using floating point??";
		switch(si->si_code)
		{
		case FPE_INTDIV:
			isl = "integer divide by zero"; break;
		case FPE_INTOVF:
			isl = "integer overflow"; break;
		case FPE_FLTDIV:
			isl = "floating point divide by zero"; break;
		case FPE_FLTOVF:
			isl = "floating point overflow"; break;
		case FPE_FLTUND:
			isl = "floating point underflow"; break;
		case FPE_FLTRES:
			isl = "floating point inexact result"; break;
		case FPE_FLTINV:
			isl = "floating point invalid operation"; break;
		case FPE_FLTSUB:
			isl = "subscript out of range"; break;
		default:
			isl = "Unknown SIGFPE problem"; break;
		}
		break;
	default:
		pthread_mutex_unlock(&bt_mutex);
		return;
	}
	ret_val = sprintf(path, "\n\n[%u:%u]\n%s\nWhy: %s (%i)\n",
			(unsigned) getpid(),
# ifdef __linux__
			(unsigned) syscall(__NR_gettid),
# else
			0,
# endif
			osl, isl, si->si_code);
	write(stderrfd, path, ret_val);
	/* errno set? */
	if(si->si_errno)
	{
		ret_val = sprintf(path, "Errno: %i\n", si->si_errno);
		write(stderrfd, path, ret_val);
	}

	/* Dump registers, a better dump would be plattformspecific */
	wptr = path + sprintf(path, "\nRegisters:\n");
	for(i = 0; i < NGREG; i++)
# if defined __powerpc__ || defined __powerpc64__
/* whoever made the powerpc-linux-gnu header...
 * I mean, an old sun an a new Linux i386 have it in sync, afaik only ppc, gnarf
 */
#  if __WORDSIZE == 32
		wptr += sprintf(wptr, "[% 3i]: 0x%016lX\n", i, (unsigned long)uc->uc_mcontext.uc_regs->gregs[i]);
#  else
		wptr += sprintf(wptr, "[% 3i]: 0x%016lX\n", i, (unsigned long)uc->uc_mcontext.gp_regs[i]);
# endif
# else
		wptr += sprintf(wptr, "[% 3i]: 0x%016lX\n", i, (unsigned long)uc->uc_mcontext.gregs[i]);
# endif
	ret_val = wptr - path;
	write(stderrfd, path, ret_val);
	
	/* Stackinfo */
	ret_val = sprintf(path, "Stack ref:\t0x%016lX\tsize: %lu\tflags: %i\n",
			(unsigned long) uc->uc_stack.ss_sp,
			(unsigned long) uc->uc_stack.ss_size,
			uc->uc_stack.ss_flags);
	write(stderrfd, path, ret_val);
	
	/* whats at the memref */
	ret_val = sprintf(path, "Memory ref:\t0x%016lX", (unsigned long)si->si_addr);
	write(stderrfd, path, ret_val);
	wptr = path;
	if(SIGSEGV != signr && si->si_addr)
	{
		/* blaerch, catch a sigsegv in our own signalhandler, memref can be bogus */
		if(!sigsetjmp(catch_sigsegv, 0))
		{
			wptr = path;
			critical = 1; 
			for(i = 0; i < 64; i++)
				wptr += sprintf(wptr, "%c%02X", (i % 16) ? ' ' : '\n', (unsigned int)((char *)si->si_addr)[i] & 0xFF);
			*wptr++ = '\n';
			*wptr++ = '"';
			for(i = 0; i < 64; i++)
			{
				char c = ((char *)si->si_addr)[i];
				*wptr++ = isprint(c) ? c : isspace(c) ? ' ' : '.';
			}
			*wptr++= '"'; 
		}
		critical = 0; 
	}
	*wptr++= '\n'; *wptr++ = '\0';
	ret_val = wptr - path;
	write(stderrfd, path, ret_val);

	/* Now get the real bt */
# define DEATHSTR_3 "\ntrying to get backtrace...\n\
-------------- start of backtrace --------------\n"
	write(stderrfd, DEATHSTR_3, str_size(DEATHSTR_3));
	strcpy(path, DEBUG_CMD);
	{
		wptr = path + str_size(DEBUG_CMD);
		ret_val = 0;
# ifndef __sun__
		ret_val = readlink("/proc/self/exe", wptr, path + (sizeof(path)-1) - wptr);
# endif
		if(-1 != ret_val)
		{
			wptr += ret_val;
			ret_val = snprintf(wptr, path + (sizeof(path)-1) - wptr, " %d\n", (int)getpid());
			if(ret_val < path + (sizeof(path)-1) - wptr)
				wptr += ret_val;
			else
				ret_val = -1; 
		}
		*wptr = '\0';
	}

	if(-1 == ret_val || system(path))
	{
# ifdef __GLIBC__
		ret_val = backtrace(buffer, BACKTRACE_DEPTH);
		backtrace_symbols_fd(buffer, ret_val, stderrfd);
# else
# define DEATHSTR_4 "No real backtrace available ;(\n"
		write(stderrfd, DEATHSTR_4, str_size(DEATHSTR_4));
# endif
	}
# define DEATHSTR_5 \
"--------------- end of backtrace ---------------\n\
\nNow the same SIG again, so you can get a CORE-file:\n"
	write(stderrfd, DEATHSTR_5, str_size(DEATHSTR_5));

out:
	/* do not reregister, we want to crash again after
	 * this debug-print
	 */
	{
		struct sigaction sas;
		memset(&sas, 0, sizeof(sas));
		sas.sa_handler = SIG_DFL;
		if(sigaction(SIGSEGV, &sas, NULL))
			exit(EXIT_FAILURE);
		if(sigaction(SIGILL, &sas, NULL))
			exit(EXIT_FAILURE);
		if(sigaction(SIGBUS, &sas, NULL))
			exit(EXIT_FAILURE);
		if(sigaction(SIGFPE, &sas, NULL))
			exit(EXIT_FAILURE);
	}
}
#endif

inline g2_connection_t *get_free_con(const char *from_file, const char *from_func, const unsigned int from_line)
{
	g2_connection_t *ret_val = NULL;
	// Getting the first Connections to work with
	if(!pthread_mutex_lock(&free_cons_mutex))
	{
		if(FC_TRESHOLD >= free_cons->limit)
		{
			size_t i;
			for(i = free_cons->limit; i < free_cons->capacity; i++, free_cons->limit++)
			{
				if(!(free_cons->data[i] = g2_con_alloc(1)))
				{
					logg_more(LOGF_WARN, from_file, from_func, from_line, 1, "free_cons memory");
					break;
				}
			}
		}

		if(free_cons->limit)
		{
			free_cons->limit--;
			ret_val = free_cons->data[free_cons->limit];
			free_cons->data[free_cons->limit] = NULL;
		}

		if(pthread_mutex_unlock(&free_cons_mutex))
		{
// TODO: shouldn't we stop everything?
			logg_more(LOGF_ERR, from_file, from_func, from_line, 1, "unlocking free_cons mutex");
			if(ret_val)
			{
				g2_con_free(ret_val);
				ret_val = NULL;
			}
		}
	}
	else
	{
		logg_more(LOGF_WARN, from_file, from_func, from_line, 1, "locking free_cons mutex");
		if(!(ret_val = g2_con_alloc(1)))
			logg_more(LOGF_ERR, from_file, from_func, from_line, 1, "no new free_con");
	}

	return(ret_val);
}

inline bool return_free_con(g2_connection_t *to_return, const char *from_file, const char *from_func, const unsigned int from_line)
{
	if(!pthread_mutex_lock(&free_cons_mutex))
	{
		if(free_cons->limit == free_cons->capacity)
		{
			if(FC_END_CAPACITY <= free_cons->limit)
			{
				g2_con_free(to_return);

				if(pthread_mutex_unlock(&free_cons_mutex))
				{
					logg_more(LOGF_ERR, from_file, from_func, from_line, 1, "unlocking free_cons mutex");
					return false;
				}
				return true;
			}
			else
			{
				struct g2_con_info *tmp_pointer = realloc(free_cons, sizeof(struct g2_con_info) + ((free_cons->capacity + FC_CAPACITY_INCREMENT) * sizeof(g2_connection_t *)));
				if(!tmp_pointer)
				{				
					logg_more(LOGF_WARN, from_file, from_func, from_line, 1, "reallocating free_cons");
					g2_con_free(to_return);
					if(pthread_mutex_unlock(&free_cons_mutex))
					{
						logg_more(LOGF_ERR, from_file, from_func, from_line, 1, "unlocking free_cons mutex");
						return false;
					}
					return true;
				}
				free_cons = tmp_pointer;
				free_cons->capacity += FC_CAPACITY_INCREMENT;
			}
		}
					
		free_cons->data[free_cons->limit] = to_return;
		free_cons->limit++;

		if(pthread_mutex_unlock(&free_cons_mutex))
		{
			logg_more(LOGF_ERR, from_file, from_func, from_line, 1, "unlocking free_cons mutex");
			return false;
		}
	}
	else
	{
		logg_more(LOGF_WARN, from_file, from_func, from_line, 1, "locking free_cons mutex");
		g2_con_free(to_return);
	}

	return true;
}

static inline void parse_cmdl_args(int argc, char **args)
{
	// seeking our options
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
				{
					logg_errno(LOGF_ERR, "opening logfile");
					exit(EXIT_FAILURE);
				}
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
	// child
	case 0:
		if(-1 == setsid())
		{
			logg_errno(LOGF_ERR, "becomming session-leader");
			exit(EXIT_FAILURE);
		}
		switch((child = fork()))
		{
		// child
		case 0:
			// change the w-dir and better also chroot
			/* if(chdir("/"))
			{
				logg_errno("changing working directory");
				return EXIT_FAILURE;
			}*/

			// connect stdin with dave null,
			// maybe stderr and stdout
			if(-1 == (tmp_fd = open("/dev/null", O_RDONLY)))
			{
				logg_errno(LOGF_ERR, "opening /dev/null");
				exit(EXIT_FAILURE);
			}
			
			if(STDIN_FILENO != dup2(tmp_fd, STDIN_FILENO))
			{
				logg_errno(LOGF_ERR, "mapping stdin");
				exit(EXIT_FAILURE);
			}

			// maybe the log-to-file option already
			// defined a log-file
			if(-1 == log_to_fd)
				log_to_fd = tmp_fd;
			else
				close(tmp_fd);
			break;
		case -1:
			logg_errno(LOGF_ERR, "forking for daemon-mode");
			while(true)
				exit(EXIT_FAILURE);
		// parent
		default:
			while(true)
				exit(EXIT_SUCCESS);
		}
		break;
	case -1:
		logg_errno(LOGF_ERR, "forking for daemon-mode");
		while(true)
			exit(EXIT_FAILURE);
	// parent
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

	// whats fixed
	server.status.our_server_upeer_needed = true;
	atomic_set(&server.status.act_connection_sum, 0);

// TODO: read from config files
	// var settings
	server.settings.logging.act_loglevel = DEFAULT_LOGLEVEL;
	server.settings.logging.add_date_time = DEFAULT_LOG_ADD_TIME;
	server.settings.logging.time_date_format = DEFAULT_LOG_TIME_FORMAT;
	server.settings.portnr_2_bind = htons(DEFAULT_PORT);
	server.settings.ip_2_bind = htonl(DEFAULT_ADDR);
	server.status.our_server_upeer = DEFAULT_SERVER_UPEER;
	server.settings.default_in_encoding = DEFAULT_ENC_IN;
	server.settings.default_out_encoding = DEFAULT_ENC_OUT;
	server.settings.max_connection_sum = DEFAULT_CON_MAX;
	server.settings.default_max_g2_packet_length = DEFAULT_PCK_LEN_MAX;
	server.settings.want_2_send_profile = DEFAULT_SEND_PROFILE;

	// set the GUID
	if(!(config = fopen(guid_file_name, "r")))
	{
		logg_errno(LOGF_ERR, "opening guid-file");
		exit(EXIT_FAILURE);
	}

	if(16 != fscanf(config,
		"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		tmp_id, tmp_id+1, tmp_id+2, tmp_id+3, tmp_id+4, tmp_id+5,
		tmp_id+6, tmp_id+7, tmp_id+8, tmp_id+9, tmp_id+10,
		tmp_id+11, tmp_id+12, tmp_id+13, tmp_id+14, tmp_id+15))
	{
		logg_errno(LOGF_ERR, "reading guid");
		exit(EXIT_FAILURE);
	}
		
	for(i = 0; i < sizeof(server.settings.our_guid); i++)
		server.settings.our_guid[i] = (uint8_t)tmp_id[i];

	fclose(config);

	// read the user-profile
	packet_uprod = NULL;

	if(server.settings.want_2_send_profile)
		read_uprofile();
}

static inline void change_the_user(void)
{
#ifndef __CYGWIN__
	struct passwd *nameinfo;
	errno = 0;
	if(!(nameinfo = getpwnam(user_name)))
	{
		logg_errno(LOGF_CRIT, "getting user-entry");
		exit(EXIT_FAILURE);
	}

	if(setgid((gid_t) nameinfo->pw_gid))
	{
		logg_errno(LOGF_WARN, "setting GID");
// Abort disabeled during devel. Maybe configurable with
// commandline-switch?
//		exit(EXIT_FAILURE);
	}

	if(setuid((uid_t) nameinfo->pw_uid))
	{
		logg_errno(LOGF_WARN, "setting UID");
// Abort disabeled during devel. Maybe configurable with
// commandline-switch?
//		exit(EXIT_FAILURE);
	}

	// wiping the structure for security-reasons
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
	// maybe we could at least stop using the admin-account
	logg_develd("uid = %lu\n", (unsigned long) getuid());
	if(500 == getuid())
	{
		logg(LOGF_ERR, "We don't want to use the Admin-account!\n");
		exit(EXIT_FAILURE);
	}
#endif // __CYGWIN__
}

static inline void setup_resources(void)
{
	size_t i;
	struct rlimit our_limit;

	// check and raise our open-file limit
/*
 * WARNING:
 * On Linux (as of 10.01.2005, 2.4.x & 2.6.10) the vanilla
 * sources (and propably others) *DON'T* respect this setting.
 * They simply cut of at a compile-time constant of 1024.Some
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

	// mutexes
	// generate a lock for shared free_cons
	if(pthread_mutex_init(&free_cons_mutex, NULL))
	{
		logg_errno(LOGF_CRIT, "creating free_cons mutex");
		exit(EXIT_FAILURE);
	}

	// getting memory
	// memory for free_cons
	if(!pthread_mutex_lock(&free_cons_mutex))
	{
		if(!(free_cons = calloc(1, sizeof(struct g2_con_info) + (FC_START_CAPACITY * sizeof(g2_connection_t *)))))
		{
			logg_errno(LOGF_CRIT, "free_cons[] memory");
			if(pthread_mutex_unlock(&free_cons_mutex))
				logg_errno(LOGF_CRIT, "unlocking free_cons mutex");
			exit(EXIT_FAILURE);
		}
		free_cons->capacity = FC_START_CAPACITY;

		for(i = 0; i < free_cons->capacity; i++, free_cons->limit++)
		{
			if(!(free_cons->data[i] = g2_con_alloc(1)))
			{
				logg_errno(LOGF_CRIT, "free_cons memory");
				if(FC_TRESHOLD >= free_cons->limit)
					break;

				for(; i > 0; --i)
				{
					g2_con_free(free_cons->data[i]);
					free_cons->limit--;
				}
				free(free_cons);
				if(pthread_mutex_unlock(&free_cons_mutex))
					logg_errno(LOGF_CRIT, "unlocking free_cons mutex");
				exit(EXIT_FAILURE);
			}
		}

		if(pthread_mutex_unlock(&free_cons_mutex))
		{
			logg_errno(LOGF_CRIT, "unlocking free_cons mutex");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		logg_errno(LOGF_CRIT, "first locking free_cons");
		exit(EXIT_FAILURE);
	}

	// IPC
	// open Sockets for IPC
	for(i = 0; i < THREAD_SUM; i++)
	{
		if(socketpair(PF_UNIX, SOCK_DGRAM, 0, sock_com[i]))
		{
			logg_errnod(LOGF_CRIT, "creating internal Socket num %u", i);
			for( ; i; i--)
			{
				if(close(sock_com[i-1][0]))
					logg_errno(LOGF_WARN, "reclaiming FD's");
				if(close(sock_com[i-1][1]))
					logg_errno(LOGF_WARN, "reclaiming FD's");
			}
			// calling cleanup here could be bad,
			// hopefully OS handles it
			exit(EXIT_FAILURE);
		}
	}

	// open pipes for IPC
	if(0 > pipe(accept_2_handler))
	{
		logg_errno(LOGF_CRIT, "opening acceptor-handler pipe");
		clean_up();
		exit(EXIT_FAILURE);
	}

	// adding socket-fd's to a poll-structure
	for(i = 0; i < THREAD_SUM; i++)
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

	// calculate inner /XML length
	
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

	xml_length = 1;                  // /XML-Control-byte
	xml_length += xml_length_length; // /XML-length
	xml_length += 3;                 // /XML-Type-length
	xml_length += f_bytes;           // /XML-Payload

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

	uprod_length = 1;                   // /UPROD-Control-byte
	uprod_length += uprod_length_length;// /UPROD-length
	uprod_length += 5;                  // /UPROD-Type-length
	uprod_length += xml_length;         // /UPROD-child-length

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

static inline void clean_up(void)
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

	for(i = 0; i < THREAD_SUM; i++)
	{
		close(sock_com[i][0]);
			// output?
		close(sock_com[i][1]);
			// output?
	}

	close(accept_2_handler[0]);
	close(accept_2_handler[1]);

	// no locking here, since maybe we are here because a thread
	// already has trouble with it and by the way... it
	// shouldn't matter anyhow at this point
	for(i = free_cons->limit; i; i--, free_cons->limit--)
		g2_con_free(free_cons->data[i-1]);

	free(free_cons);

	if(packet_uprod)
		free(packet_uprod);

	// what should i do if this fails?
	pthread_mutex_destroy(&free_cons_mutex);

	fclose(stdin);
	fclose(stdout); // get a sync if we output to a file
	fclose(stderr);
}

/* 
 * hmpf, ok, we try to say the compiler to include them (if it
 * supports such an anotation), and not optimize them away,
 * while saying splint thats ok that they are unused
 */
/*@unused@*/
static char const ownid[] GCC_ATTR_USED_VAR = "$Own: " DIST " built on " __DATE__ " " __TIME__ " at \"" SYSTEM_INFO "\" with \"" COMPILER_INFO "\" $";
/*@unused@*/
static char const rcsid[] GCC_ATTR_USED_VAR = "$Id: G2MainServer.c,v 1.25 2005/11/05 18:02:45 redbully Exp redbully $";
//EOF
