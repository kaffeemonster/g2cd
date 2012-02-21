/*
 * backtrace.c
 * try to spit out a backtrace on crashes
 *
 * Copyright (c) 2008-2010 Jan Seiffert
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
 * $Id: $
 */


#include "../config.h"
#include "backtrace.h"

#ifndef WANT_BACKTRACES
void __init backtrace_init(void)
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
# include "my_pthread.h"
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
# include "itoa.h"

static inline void write_panic(int fd, const void *buf, size_t len)
{
	ssize_t res = write(fd, buf, len);
	if((size_t)res != len)
	{
		/* we can do nothing if the write fails */
	}
}

static char *my_crashdump(char *buf, unsigned char *addr, int lines)
{
/*	unsigned char *org_addr = addr;*/
	int i;

	addr -= lines * 8;
	for(i = lines; i; i--, addr += 16)
	{
		int j;

		buf = addrtoa(buf, addr);
		*buf++ = ':'; *buf++ = ' ';
		for(j = 0; j < 2; j++)
		{
			int k = 0;
			for(; k < 8; k++) {
				buf = utoXa_0fix(buf, (unsigned int)addr[k] & 0xFF, 2);
				*buf++ = ' ';
			}
			*buf++ = ' ';
		}
		*buf++ = '\t';
		for(j = 0; j < 2; j++)
		{
			int k = 0;
			for(; k < 8; k++)
				*buf++ = isprint(addr[k]) ? addr[k] : (isspace_a(addr[k]) ? ' ' : '.');
			*buf++ = ' ';
		}
		*buf++ = '\n';
	}
	return buf;
}

static GCC_ATTR_COLD void sig_segv_print(int signr, siginfo_t *si, void *vuc)
{
#if defined(__alpha__)
	static unsigned long greg_space[NGREG];
#endif
	static pthread_mutex_t bt_mutex = PTHREAD_MUTEX_INITIALIZER;
	static sigjmp_buf catch_sigsegv;
	static volatile sig_atomic_t critical = 0;
	static char path[4096];
	const char *osl = NULL, *isl = NULL;
	char *wptr = NULL; 
	ucontext_t *uc = vuc;
	unsigned long *greg_iter;
# ifdef __GLIBC__
#  define BACKTRACE_DEPTH (sizeof(path) / sizeof(void *))
	void **buffer = (void **) path;
# endif
	static int stderrfd;
	int ret_val = 0;
	unsigned i;

	stderrfd = fileno(stderr);
	if(-1 == stderrfd) {
		/*
		 * we crashed very late or something else is foo
		 * fix it, we maybe write to lala land, but...
		 */
		stderrfd = STDERR_FILENO;
	}

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

	write_panic(stderrfd, DEATHSTR_1, str_size(DEATHSTR_1));
	/* lock out the other threads, if another one apears, simply deadlock */
	if(SIGSEGV == signr && critical) {
		/* maybe we caught a sigsegv in our own sighandler, try to resume */
		siglongjmp(catch_sigsegv, 666);
	}
	else
	{
		if(pthread_mutex_lock(&bt_mutex))
		{
# define DEATHSTR_2 "\"I'm scared Dave. Will i dream?\"\n\
Another thread crashed and something went wrong.\nSo no BT, maybe a core.\n"
			write_panic(stderrfd, DEATHSTR_2, str_size(DEATHSTR_2));
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
			isl = "(unshure) special instruction execption"; break;
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
	case SIGTRAP:
		osl = "SIGTRAP -> Boom!\nBan land mines.";
		switch(si->si_code)
		{
		case TRAP_BRKPT:
			isl = "Breakpoint"; break;
		case TRAP_TRACE:
			isl = "trace trap"; break;
		default:
			isl = "Unknown SIGTRAP cause"; break;
		}
		break;
	default:
		pthread_mutex_unlock(&bt_mutex);
		return;
	}
	wptr = utoa(strplitcpy(path, "\n\n["), (unsigned)getpid());
	*wptr++ = ':';
	wptr = utoa(wptr,
# ifdef __linux__
	            (unsigned) syscall(__NR_gettid)
# else
	             0
# endif
	);
	*wptr++ = ']'; *wptr++ = '\n';
	wptr = strpcpy(strplitcpy(strpcpy(wptr, osl), "\nWhy: "), isl);
	*wptr++ = ' '; *wptr++ = '(';
	wptr = mitoa(wptr, si->si_code);
	*wptr++ = ')'; *wptr++ = '\n';
	write_panic(stderrfd, path, wptr - path);
	/* errno set? */
	if(si->si_errno) {
		wptr = mitoa(strplitcpy(path, "Errno: "), si->si_errno);
		*wptr++ = '\n';
		write_panic(stderrfd, path, wptr - path);
	}

	/* Dump registers, a better dump would be plattformspecific */
	wptr = strplitcpy(path, "\nRegisters:\n");
# ifdef __FreeBSD__
#  define NGREG (uc->uc_mcontext.mc_len)
	greg_iter = (unsigned long *)&uc->uc_mcontext.mc_onstack;
# elif defined __powerpc__ || defined __powerpc64__
	/* whoever made the powerpc-linux-gnu header...
	 * I mean, an old sun an a new Linux i386 have it in sync, gnarf
 	*/
#  if __WORDSIZE == 32
	greg_iter = (unsigned long *)&uc->uc_mcontext.uc_regs->gregs[0];
#  else
	greg_iter = (unsigned long *)&uc->uc_mcontext.gp_regs[0];
#  endif
# elif defined(__arm__)
	/* and again, hey, lets define something somehow different.
	 * Please, and where are those 18 gregs now?
	 */
	greg_iter = (unsigned long *)&uc->uc_mcontext.arm_r0;
# elif defined(__alpha__)
	/* make greg linear, fix name, again... */
	for(i = 0; i < NGREG - 1; i++)
		greg_space[i] = uc->uc_mcontext.sc_regs[i];
	greg_space[32] = uc->uc_mcontext.sc_pc;
	greg_iter = greg_space;
# elif defined(__ia64__)
#  define NGREG 52
	greg_iter = (unsigned long *)&uc->uc_mcontext.sc_ip;
# elif defined(__hppa__)
#  undef NGREG
#  define NGREG (sizeof(uc->uc_mcontext)/sizeof(unsigned long))
	greg_iter = (unsigned long *)&uc->uc_mcontext.sc_flags;
# else
	greg_iter = (unsigned long *)&uc->uc_mcontext.gregs[0];
# endif
	for(i = 0; i < NGREG; i++)
	{
		*wptr++ = '[';
		wptr = strplitcpy(mitoa_sfix(wptr, i, 3), "]: 0x");
		wptr = multoXa_0fix(wptr, greg_iter[i], 16);
		*wptr++ = '\n';
	}
	write_panic(stderrfd, path, wptr - path);

	/* Stackinfo */
	wptr = strplitcpy(path, "Stack ref:\t0x");
	wptr = multoXa_0fix(wptr, (unsigned long) uc->uc_stack.ss_sp, 16);
	wptr = strplitcpy(wptr, "\tsize: ");
	wptr = multoa(wptr, (unsigned long)uc->uc_stack.ss_size);
	wptr = strplitcpy(wptr, "\tflags: ");
	wptr = mitoa(wptr, uc->uc_stack.ss_flags);
	*wptr++ = '\n';
	write_panic(stderrfd, path, wptr - path);

	wptr = path;
	if(SIGSEGV != signr && uc->uc_stack.ss_sp)
	{
		/* blaerch, catch a sigsegv in our own signalhandler, stackref can be bogus */
		if(!sigsetjmp(catch_sigsegv, 0))
		{
			wptr = path;
			critical = 1;
			wptr = my_crashdump(wptr, (unsigned char *)uc->uc_stack.ss_sp, 8);
		}
		critical = 0; 
	}
	*wptr++= '\n'; *wptr++ = '\0';
	write_panic(stderrfd, path, wptr - path);
	
	/* whats at the memref */
	wptr = multoXa_0fix(strplitcpy(path, "Memory ref:\t0x"), (unsigned long)si->si_addr, 16);
	*wptr++ = '\n';
	write_panic(stderrfd, path, wptr - path);

	wptr = path;
	if(SIGSEGV != signr && si->si_addr)
	{
		/* blaerch, catch a sigsegv in our own signalhandler, memref can be bogus */
		if(!sigsetjmp(catch_sigsegv, 0)) {
			wptr = path;
			critical = 1;
			wptr = my_crashdump(wptr, (unsigned char *)si->si_addr, 8);
		}
		critical = 0;
	}
	*wptr++= '\n'; *wptr++ = '\0';
	write_panic(stderrfd, path, wptr - path);

	fflush(NULL);

	/* Now get the real bt */
# define DEATHSTR_31 "\ntrying to get backtrace...\n"
	write_panic(stderrfd, DEATHSTR_31, str_size(DEATHSTR_31));
	wptr = strplitcpy(path, DEBUG_CMD);
	ret_val = 0;
# ifndef __sun__
	ret_val = readlink("/proc/self/exe", wptr, path + (sizeof(path)-1) - wptr);
# endif
	if(-1 != ret_val)
	{
		wptr += ret_val;
		*wptr++ = ' ';
		wptr = mitoa(wptr, (int)getpid());
# ifndef __sun__
		*wptr++ = ' ';
# else
		*wptr++ = '\n';
# endif
# ifndef __sun__
		if(str_size("<<EOF\n"DEBUG_CMDS"EOF\n") <=
		   (unsigned)(path + (sizeof(path)-1) - wptr))
			wptr = strplitcpy(wptr, "<<EOF\n" DEBUG_CMDS "EOF\n");
		else
			ret_val = -1;
# endif
	}
	*wptr = '\0';

	write_panic(stderrfd, path, wptr - path);
#define DEATHSTR_32 \
	"-------------- start of backtrace --------------\n"
	write_panic(stderrfd, DEATHSTR_32, str_size(DEATHSTR_32));

	if(-1 == ret_val || system(path))
	{
# ifdef __GLIBC__
		ret_val = backtrace(buffer, BACKTRACE_DEPTH);
		backtrace_symbols_fd(buffer, ret_val, stderrfd);
# else
# define DEATHSTR_4 "No real backtrace available ;-(\n"
		write_panic(stderrfd, DEATHSTR_4, str_size(DEATHSTR_4));
# endif
	}
# define DEATHSTR_5 \
"\n--------------- end of backtrace ---------------\n" \
"+++++++++++++++ start of mappings ++++++++++++++\n"
	write_panic(stderrfd, DEATHSTR_5, str_size(DEATHSTR_5));
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
					write_panic(stderrfd, path, rret);
			} while(rret > 0);
			close(mfd);
		}
	}
# define DEATHSTR_6 \
"++++++++++++++++ end of mappings ++++++++++++++\n\n" \
"Now the same SIG again, so you can get a CORE-file:\n"
	write_panic(stderrfd, DEATHSTR_6, str_size(DEATHSTR_6));

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
		if(sigaction(SIGTRAP, &sas, NULL))
			exit(EXIT_FAILURE);
		if(sigaction(SIGABRT, &sas, NULL))
			exit(EXIT_FAILURE);
	}
}

void __init backtrace_init(void)
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
	if(sigaction(SIGTRAP, &sas, NULL))
		logg_pos(LOGF_NOTICE, "Unable to register SIGTRAP-handler\n");
	if(sigaction(SIGABRT, &sas, NULL))
		logg_pos(LOGF_NOTICE, "Unable to register SIGABRT-handler\n");
}
#endif
/*@unused@*/
static char const rcsid_bt[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
