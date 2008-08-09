/*
 * backtrace.c
 * try to spit out a backtrace on crashes
 *
 * Copyright (c) 2008 Jan Seiffert
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
 * $Id: $
 */


#include "../config.h"
#include "backtrace.h"

#ifndef WANT_BACKTRACES
void backtrace_init(void)
{
	/* nop */
}
#else

/*
 * ----~~~~====****!!!! BEWARE !!!!****====~~~~----
 * VERY VERY UGLY, HACKY, UNPORTABLE code ahead...
 *
 * This code does everything wrong what could be wrong:
 * - It calls (io-)lib funcs in a sighandler
 * - when the hole program is already b0rken
 * - fiddles with a lot of stuff
 * - tinkers around in platform specific things
 * - prints to output, wherever that is
 * - even makes direct syscalls
 * - risks a SIGSEGV in a sighandler (and has to fix it even
 *   hackier, to not recurse ad infinitum)
 * - uses static buffers unlocked in a multithreded program
 * - and even forks of a childprocess (from a sighandler...)
 * - to attach a debuger to itself
 * - which needs even more hackie stuff
 * - your shell does support here-files?
 * - and then reads around in /proc
 *
 * Did i mention its hacked up quick & dirty? Uhhh awfull style.
 * Didn't even know i'm capable of something like this...
 *
 * THIS CODE IS NOT SUITABLE FOR PRODUCTION USE, IT MAY EAT YOUR KITTEN
 *
 * (unfortunatly you want backtraces when you least expect failure,
 * in production.... sigh)
 *
 * DON'T USE THIS
 * DON'T USE THIS AS AN EXAMPLE
 * DON'T TRUST THIS
 * DON'T LOOK AT IT, for your own sake
 *
 * I have warned you
 */

# include <stdlib.h>
# include <stdio.h>
# include <ctype.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <pthread.h>
# include <signal.h>
# include <setjmp.h>
# include <ucontext.h>
# ifdef __GLIBC__
#  include <execinfo.h>
#  include <malloc.h>
#  ifdef __linux__
#   include <syscall.h>
#  endif
# endif
/* Own */
# include "log_facility.h"

static char *my_crashdump(char *buf, unsigned char *addr, int lines)
{
/*	unsigned char *org_addr = addr;*/
	int i;

	addr -= lines * 8;
	for(i = lines; i; i--, addr += 16)
	{
		int j;

		buf += sprintf(buf, "%p:  ", addr);
		for(j = 0; j < 2; j++)
		{
			int k = 0;
			for(; k < 8; k++)
				buf += sprintf(buf, "%02X ", (unsigned int)addr[k] & 0xFF);
			*buf++ = ' ';
		}
		*buf++ = '\t';
		for(j = 0; j < 2; j++)
		{
			int k = 0;
			for(; k < 8; k++)
				*buf++ = isprint(addr[k]) ? addr[k] : (isspace(addr[k]) ? ' ' : '.');
			*buf++ = ' ';
		}
		*buf++ = '\n';
	}
	return buf;
}

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
 "gdb -nw -q -r "
#  define DEBUG_CMDS \
	"set pagination off\ninfo threads\nthread apply all bt full\ninfo frame\ninfo registers\ndisass $pc\nlist\ndetach\nquit\n"
# endif

# define DEATHSTR_1 \
"                                                            <~     ° *  * |X|\n\
                                                                 °    \\*|*|X|\n\
=======================================================================*@@|X|\n\
                                                              |ouch|° /*|*|X|\n\
                                                              °      *  * |X|\n\
We crashed hard!!! Please fasten your seat belts...\n\
We will now try scary things to aid debuging the situation.\n\
(you may see unrelated program output spilling out of buffers)\n"
	write(stderrfd, DEATHSTR_1, str_size(DEATHSTR_1));
	/* lock out the other threads, if another one apears, simply deadlock */
	if(SIGSEGV == signr && critical)
	{
		/* maybe we caught a sigsegv in our own sighandler, try to resume */
		siglongjmp(catch_sigsegv, 666);
	}
	else
	{
		if(pthread_mutex_lock(&bt_mutex))
		{
# define DEATHSTR_2 "\"I'm scared Dave. Will i dream?\"\n\
Another thread crashed and something went wrong.\nSo no BT, maybe a core.\n"
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
		case 128: /* Linux only? x86 only? */
			isl = "(unshure) priveleged instruction, probaly a SIGILL";
		default:
			isl = "Unknown SIGSEGV problem"; break;
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
	wptr = path;
	if(SIGSEGV != signr && uc->uc_stack.ss_sp)
	{
		/* blaerch, catch a sigsegv in our own signalhandler, memref can be bogus */
		if(!sigsetjmp(catch_sigsegv, 0))
		{
			wptr = path;
			critical = 1;
			wptr = my_crashdump(wptr, (unsigned char *)uc->uc_stack.ss_sp, 8);
		}
		critical = 0; 
	}
	*wptr++= '\n'; *wptr++ = '\0';
	ret_val = wptr - path;
	write(stderrfd, path, ret_val);
	
	/* whats at the memref */
	ret_val = sprintf(path, "Memory ref:\t0x%016lX\n", (unsigned long)si->si_addr);
	write(stderrfd, path, ret_val);
	wptr = path;
	if(SIGSEGV != signr && si->si_addr)
	{
		/* blaerch, catch a sigsegv in our own signalhandler, memref can be bogus */
		if(!sigsetjmp(catch_sigsegv, 0))
		{
			wptr = path;
			critical = 1;
			wptr = my_crashdump(wptr, (unsigned char *)si->si_addr, 8);
		}
		critical = 0;
	}
	*wptr++= '\n'; *wptr++ = '\0';
	ret_val = wptr - path;
	write(stderrfd, path, ret_val);

	fflush(NULL);

	/* Now get the real bt */
# define DEATHSTR_31 "\ntrying to get backtrace...\n"
	write(stderrfd, DEATHSTR_31, str_size(DEATHSTR_31));
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
			ret_val = snprintf(wptr, path + (sizeof(path)-1) - wptr,
# ifndef __sun__
					" %d ",
# else
					" %d\n",
# endif
					(int)getpid());

			if(ret_val < path + (sizeof(path)-1) - wptr)
			{
				wptr += ret_val;
# ifndef __sun__
				ret_val = snprintf(wptr, path + (sizeof(path)-1) - wptr,
						"<<EOF\n%sEOF\n", DEBUG_CMDS);
				if(ret_val < path + (sizeof(path)-1) - wptr)
					wptr += ret_val;
				else
					ret_val = -1;
# endif
			}
			else
				ret_val = -1;
		}
		*wptr = '\0';
	}

	write(stderrfd, path, strlen(path));
#define DEATHSTR_32 \
	"-------------- start of backtrace --------------\n"
	write(stderrfd, DEATHSTR_32, str_size(DEATHSTR_32));

	if(-1 == ret_val || system(path))
	{
# ifdef __GLIBC__
		ret_val = backtrace(buffer, BACKTRACE_DEPTH);
		backtrace_symbols_fd(buffer, ret_val, stderrfd);
# else
# define DEATHSTR_4 "No real backtrace available ;-(\n"
		write(stderrfd, DEATHSTR_4, str_size(DEATHSTR_4));
# endif
	}
# define DEATHSTR_5 \
"\n--------------- end of backtrace ---------------\n" \
"+++++++++++++++ start of mappings ++++++++++++++\n"
	write(stderrfd, DEATHSTR_5, str_size(DEATHSTR_5));
	if(!access("/proc/self/maps", R_OK))
	{
		int mfd = open("/proc/self/maps", O_RDONLY);
		if(-1 != mfd)
		{
			ssize_t rret;
			do
			{
				rret = read(mfd, path, sizeof(path));
	
				if(rret > 0)
					write(stderrfd, path, rret);
			} while(rret > 0);
			close(mfd);
		}
	}
# define DEATHSTR_6 \
"++++++++++++++++ end of mappings ++++++++++++++\n\n" \
"Now the same SIG again, so you can get a CORE-file:\n"
	write(stderrfd, DEATHSTR_6, str_size(DEATHSTR_6));

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

void backtrace_init(void)
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