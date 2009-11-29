/* arflock.c
 * ar proxy to lock the .a so concurrent access is possible
 *
 * Copyright (c) 2006-2009 Jan Seiffert
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

#undef false
#undef true
#define false 0
#define true (!false)

static void sig_alarm(int);
static int gime_lock(int);
static int collect_child(pid_t);
static int invokation(char *);

static volatile int timeout;
static char pid_buf[100];

int main(int argc, char **argv)
{
	int fd = -1, fd_gate, fd_locked = false, fd_gate_locked = false;
	int exit_val = EXIT_SUCCESS, ret_val;
	pid_t c_pid = -1;

	if(3 > argc)
		exit(invokation(argv[0]));

	/* first lock the gate, may be the archive isn't created yet */
	if(-1 == (fd_gate = open(".arflockgate", O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", open() gate" );
		exit(EXIT_FAILURE);
	}
	ret_val = sprintf(pid_buf, "%i\n", (int)getpid());
	if(ret_val != write(fd_gate, pid_buf, ret_val)) {
		/* nothing we can do about, it's only nice to put our pid inside */
	}

	/* register alarm handler for timeout */
	if(SIG_ERR == signal(SIGALRM, sig_alarm))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", signal()");
		exit(EXIT_FAILURE);
	}

	/* get lock on the gate */
	timeout = DEFAULT_TIMEOUT;
	if(!gime_lock(fd_gate))
	{
		exit_val = EXIT_FAILURE;
		goto BAIL_OUT;
	}
	fd_gate_locked = true;

	/* open the lib */
	fd = open(argv[1], O_WRONLY);
	if(fd != -1)
	{
		/* lock the lib */
		timeout = DEFAULT_TIMEOUT;
		if(!gime_lock(fd))
		{
			exit_val = EXIT_FAILURE;
			goto BAIL_OUT;
		}
		fd_locked = true;
		/* since we could lock the lib, unlock gate */
		if(fd_gate_locked && -1 == lockf(fd, F_ULOCK, 0))
			perror(__FILE__ ", " mkstr(__LINE__) ", lockf() gate unlock");
		else
			fd_gate_locked = false;
	}
	/* if it did not exist, keep gate lock, other errors bail out */
	else if(ENOENT != errno)
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", open()" );
		exit_val = EXIT_FAILURE;
		goto BAIL_OUT;
	}
	
	/* unregister sighandler, we should be done with timeouts */
	signal(SIGALRM, SIG_DFL);

	if(-1 == (c_pid = fork()))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", fork()");
		exit_val = EXIT_FAILURE;
	}
	else if(0 == c_pid) /* child */
	{
		execvp(argv[2], &argv[2]);
		perror(argv[2]);
		exit(EXIT_FAILURE);
	}
	else /* parent */
		exit_val = collect_child(c_pid);

BAIL_OUT:
	if(fd_locked && -1 == lockf(fd, F_ULOCK, 0))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", lockf() unlock");
		exit_val = EXIT_FAILURE;
	}
	if(fd_gate_locked && -1 == lockf(fd_gate, F_ULOCK, 0))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", lockf() gate unlock");
		exit_val = EXIT_FAILURE;
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

static int gime_lock(int fd)
{
	int ret_val;
	alarm(1);
	do
	{
		ret_val = lockf(fd, F_LOCK, 0);
	} while(0 > ret_val && EINTR == errno && 0 < timeout);
	alarm(0);
	if(0 > ret_val)
	{
		if(EINTR != errno)
			perror(__FILE__ ", " mkstr(__LINE__) ", lockf() gate");
		else
		{
			fputs(__FILE__ ", " mkstr(__LINE__) ", lockf() gate", stderr);
			fputs(": " mkstr(DEFAULT_TIMEOUT) "s timeout reached\n", stderr);
		}
		return false;
	}

	return true;
}

static int collect_child(pid_t c_pid)
{
	pid_t ret_val;
	int status;

	do {
		status = 0;
		ret_val = waitpid(c_pid, &status, 0);
	} while((0 > ret_val && EINTR == errno) || (0 <= ret_val && !WIFEXITED(status) && !WIFSIGNALED(status)));

	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	else
		return EXIT_FAILURE;
}

static int invokation(char *prg_name)
{
	fprintf(stderr, "%s file ar-line\n", prg_name);
	fputs("\tfile\t- the .a to save\n", stderr);
	fputs("\tar-line\t- the complete ar comandline\n", stderr);
	return EXIT_FAILURE;
}
