/* arflock.c
 * ar proxy to lock the .a so concurrent access is possible
 * 
 * Copyright (c) 2006, Jan Seiffert
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
 * $Id: $
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


#define DEFAULT_TIMEOUT 30
#define mkstr2(s) #s
#define mkstr(s) mkstr2(s)

static void sig_alarm(int);
static int collect_child(pid_t);
static void invokation(char *);

static volatile int timeout;

int main(int argc, char **argv)
{
	int fd, ret_val;
	int exit_val = EXIT_SUCCESS;
	pid_t c_pid = -1;

	if(3 > argc)
	{
		invokation(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* lets see if the archive is there */
	if(-1 == (fd = open(argv[1], O_WRONLY)))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", open()" );
		exit(EXIT_FAILURE);
	}

	timeout = DEFAULT_TIMEOUT;
	if(SIG_ERR == signal(SIGALRM, sig_alarm))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", signal()");
		exit(EXIT_FAILURE);
	}

	alarm(1);
	do
	{
		ret_val = lockf(fd, F_LOCK, 0);
	} while(0 > ret_val && EINTR == errno && 0 < timeout);
	alarm(0);
	signal(SIGALRM, SIG_DFL);
	if(0 > ret_val)
	{
		if(EINTR != errno)
		{
			perror(__FILE__ ", " mkstr(__LINE__) ", lockf()");
			exit(EXIT_FAILURE);
		}
		else
		{
			fputs(__FILE__ ", " mkstr(__LINE__) ", lockf()", stderr);
			fputs(": " mkstr(DEFAULT_TIMEOUT) "s timeout reached\n", stderr);
			exit(EXIT_FAILURE);
		}
	}

	if(-1 == (c_pid = fork()))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", fork()");
		exit_val = EXIT_FAILURE;
	}
	else if(0 == c_pid) /* child */
	{
		execvp(argv[2], &argv[3]);
		perror(argv[2]);
		exit(EXIT_FAILURE);
	}
	else /* parent */
		exit_val = collect_child(c_pid);

	if(-1 == lockf(fd, F_ULOCK, 0))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", lockf() unlock");
		exit(EXIT_FAILURE);
	}

	exit(exit_val);
}

static void sig_alarm(int signr)
{
	if(SIGALRM != signr)
		return;

	timeout--;
	if(SIG_ERR == signal(SIGALRM, sig_alarm))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", signal()");
		exit(EXIT_FAILURE);
	}
}

static int collect_child(pid_t c_pid)
{
	pid_t ret_val;
	int status;

	do {
		status = 0;
		ret_val = waitpid(c_pid, &status, 0);
	} while((0 > ret_val && EINTR == errno) || (0 <= ret_val && !WIFEXITED(status)));

	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	else
		return EXIT_FAILURE;
}

static void invokation(char *prg_name)
{
	fprintf(stderr, "%s file ar-line\n", prg_name);
	fputs("\tfile\t- the .a to save\n", stderr);
	fputs("\tar-line\t- the complete ar comandline\n", stderr);
}
