/* ccdrv.c
 * cc driver to pretty print the build
 *
 * Copyright (c) 2005-2010 Jan Seiffert
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
 * Idea derived from ccdv wich states:
 * Copyright (C) 2002-2003, by Mike Gleason, NcFTP Software.
 * All Rights Reserved.
 *
 * Licensed under the GNU Public License.
 *
 * $Id: ccdrv.c,v 1.5 2005/11/05 18:00:56 redbully Exp redbully $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <curses.h>
#include <term.h>

#define mkstr2(s) #s
#define mkstr(s) mkstr2(s)

#undef true
#undef false
#define true 1
#define false 0

struct my_buffer
{
	size_t pos, limit, capacity;
	char *data;
};

#define buffer_start(x) ((x).data + (x).pos)
#define buffer_remaining(x) ((x).limit - (x).pos)
#define buffer_flip(x) do { \
		(x).limit = (x).pos; \
		(x).pos = 0; \
	} while(0)
#define buffer_clear(x) do { \
		(x).limit = (x).capacity; \
		(x).pos = 0; \
	} while(0)

int verbose  = false;
int show_fail_cmd = false;
const char *comment = NULL;

static int handle_output(int *, int, int *);
static int read_output(int, struct my_buffer *);
static int collect_child(pid_t);
static void invokation(char *);

int main(int argc, char **argv)
{
	int argi = 1, c_exit_status;
	int com_pipe[2][2];
	int std_emul_fds[2];
	int new_stdin;
	pid_t c_pid = -1;

	if(2 > argc)
		goto BAIL_OUT;

	if('-' == argv[argi][0])
	{
		size_t len = strlen(argv[argi]);
		size_t tmp = 1;
		
		if(2 > len)
			goto BAIL_OUT;
		
		for(; tmp < len; tmp++)
		{
			switch(argv[argi][tmp])
			{
			case 'v':
				verbose++;
				break;
			case 's':
				show_fail_cmd++;
				break;
			default:
				fprintf(stderr, __FILE__ ", unknown option: %c\n", argv[argi][tmp]);
			}
		}
		
		/* since we consumed our arguments,
		 * there must be more to come
		 */
		if(3 > argc)
			goto BAIL_OUT;

		argi++;
	}

	if(verbose)
	{
		int i;
		/* be verbose, so simply dump the cmd,
		 * don't print the comment, if there
		 */
		if((argc - argi) >= 2)
			argi++;

		i = argi;
		for(; i < argc; i++)
			printf("%s ", argv[i]);
		putchar('\n');
	}
	else if((argc - argi) >= 2)
	{
		comment = argv[argi];
		argi++;
	}

	if(0 > pipe(com_pipe[0]) || 0 > pipe(com_pipe[1]))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", pipe()");
		exit(EXIT_FAILURE);
	}

	/* open our own stdin */
	close(STDIN_FILENO);
	new_stdin = open("/dev/null", O_RDWR, 00666);
	if(STDIN_FILENO != new_stdin && 0 == dup2(new_stdin, STDIN_FILENO))
		close(new_stdin);
	
	if(-1 == (c_pid = fork()))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", fork()");
		exit(EXIT_FAILURE);
	}
	else if(0 == c_pid) /* Child */
	{
		/* close the pipes read end */
		close(com_pipe[0][0]);
		close(com_pipe[1][0]);

		/* set up stderr and stdout on the pipes */
		if(STDOUT_FILENO != com_pipe[0][1])
		{
			dup2(com_pipe[0][1], STDOUT_FILENO);
			close(com_pipe[0][1]);
		}
		if(STDERR_FILENO != com_pipe[1][1])
		{
			dup2(com_pipe[1][1], STDERR_FILENO);
			close(com_pipe[1][1]);
		}
		
		execvp(argv[argi], &argv[argi]);
		perror(argv[argi]);
		exit(EXIT_FAILURE);
	}
	/* Parent */
	/* close pipes write end */
	close(com_pipe[0][1]);
	close(com_pipe[1][1]);

	std_emul_fds[0] = com_pipe[0][0];
	std_emul_fds[1] = com_pipe[1][0];
	if(!handle_output(std_emul_fds, c_pid, &c_exit_status))
	{
		kill(c_pid, SIGKILL);
		exit(EXIT_FAILURE);
	}

	/* if cmd failed, we should show the failed cmd
	 * if we have not alredy printed it
	 */
	if(c_exit_status && !verbose && show_fail_cmd)
	{
		int i;
		fputs("command was:\n", stderr);
		for(i = argi; i < argc; i++)
			fprintf(stderr, "%s ", argv[i]);
		putchar('\n');
	}
	exit(c_exit_status);
	
BAIL_OUT:
	invokation(argv[0]);
	exit(EXIT_FAILURE);
}

static int handle_output(int *fds, int c_pid, int *c_exit_status)
{
	struct my_buffer data[2] = {{0,0,0,NULL},{0,0,0,NULL}};
	struct pollfd poll_fds[2];
	int ret_val, poll_done[2] = {false, false};
	/* term things */
	int my_cols = 80, my_tabs = 8;
	int onatty = isatty(STDOUT_FILENO), errnum;
	const char *my_reset = "", *my_red = "";
	const char *my_green = "", *my_yellow = "";

	if(!(data[0].data = malloc(4096)) || !(data[1].data = malloc(4096)))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", malloc()");
		return false;
	}
	data[0].capacity = data[0].limit = data[1].capacity = data[1].limit = 4096;

	poll_fds[0].fd     = fds[0];
	poll_fds[0].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
	poll_fds[1].fd     = fds[1];
	poll_fds[1].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;

	if(!verbose)
	{
		if(onatty)
		{
			if(OK == setupterm(NULL, STDOUT_FILENO, &errnum))
			{
				char *tmp_str = NULL;
				my_cols = columns < 0 ? 80 : columns;
				my_tabs = init_tabs < 0 ? 8 : init_tabs;
				my_reset = exit_attribute_mode;

				tmp_str = tparm(set_a_foreground, 1);
				if(tmp_str)
				{
					char *tmp_str2 = malloc(strlen(tmp_str)+1);
					if(tmp_str2)
						my_red = strcpy(tmp_str2, tmp_str);
				}
				tmp_str = tparm(set_a_foreground, 2);
				if(tmp_str)
				{
					char *tmp_str2 = malloc(strlen(tmp_str)+1);
					if(tmp_str2)
						my_green = strcpy(tmp_str2, tmp_str);
				}
				tmp_str = tparm(set_a_foreground, 3);
				if(tmp_str)
				{
					char *tmp_str2 = malloc(strlen(tmp_str)+1);
					if(tmp_str2)
						my_yellow = strcpy(tmp_str2, tmp_str);
				}
			}
			else
			{
				printf("setupterm failed\n");
			}
		}

//		printf("oa: %s\tc: %d\tt: %d\n", onatty ? "true":"false", my_cols, my_tabs);
		/* finally print the comment over the complete column */
		printf(onatty ? "\r\t%-*s[..]" : "\t%-*s", my_cols - my_tabs - 5, comment);

		/* bring the good news only to human users... */
		if(onatty)
			fflush(stdout);
	}
	
	do
	{
		int i;
		errno = 0;
		ret_val = poll(poll_fds, 2, -1);
		if(-1 == ret_val)
		{
			if(EINTR == errno)
				continue;
			perror(__FILE__ ", " mkstr(__LINE__) ", poll()");
			return false;
		}
		if(0 == ret_val && EAGAIN != errno)
			break;

		for(i = 0; ret_val && i < 2; i++)
		{
			if(!poll_fds[i].revents)
				continue;
			if(poll_fds[i].revents & POLLIN)
			{
				int read_status;
				if(-1 == (read_status = read_output(poll_fds[i].fd, &data[i])))
					return false;
				else if(1 == read_status)
				{
					poll_fds[i].events &= ~POLLIN;
					poll_done[i] = true;
				}
			}
			if(poll_fds[i].revents & ~POLLIN)
				poll_done[i] = true;
			ret_val--;
		}
	} while(!poll_done[0] && !poll_done[1]);

	*c_exit_status = collect_child(c_pid);

	buffer_flip(data[0]);
	buffer_flip(data[1]);

	if(!verbose)
	{
		if(onatty)
			printf("\r\t%-*s[%s%s%s]\n",
					my_cols - my_tabs - 5,
					comment,
					(*c_exit_status) ? my_red : (buffer_remaining(data[0]) || buffer_remaining(data[1])) ? my_yellow : my_green,
					(*c_exit_status) ? "!!" : "OK",
					my_reset);
		else
			printf("[%s]\n", (*c_exit_status) ? "!!" : (buffer_remaining(data[0]) || buffer_remaining(data[1])) ? "ok" : "OK");
	}

	/* yes, never use stdio and unistd interleaving,
	 * hopefully fflush allows us to
	 */
	fflush(NULL);
	if((size_t)write(STDOUT_FILENO, buffer_start(data[0]), buffer_remaining(data[0])) !=
	   buffer_remaining(data[0])) {
	}
	if((size_t)write(STDERR_FILENO, buffer_start(data[1]), buffer_remaining(data[1])) !=
	   buffer_remaining(data[1])) {
	}

	return true;
}

static int read_output(int fd, struct my_buffer *storage)
{
	ssize_t ret_val;
	do
	{
		errno = 0;
		ret_val = read(fd, buffer_start(*storage), buffer_remaining(*storage));
	} while(-1 == ret_val && EINTR == errno);

	if(-1 == ret_val)
	{
		if(EAGAIN == errno)
			return 0;
		perror(__FILE__ ", " mkstr(__LINE__) ", read()");
		return -1;
	}
	else if(0 == ret_val)
	{
		if(EAGAIN == errno)
			return 0;
		
		/* EOF reached */
		return 1;
	}
	else
	{
		storage->pos += ret_val;
		if(!buffer_remaining(*storage))
		{
			char *tmp_ptr = realloc(storage->data, storage->capacity * 2);
			if(!tmp_ptr)
			{
				perror(__FILE__ ", " mkstr(__LINE__) ", realloc()");
				return -1;
			}
			storage->data = tmp_ptr;
			storage->capacity *= 2;
			storage->limit = storage->capacity;
		}
		return 0;
	}
}

static int collect_child(pid_t c_pid)
{
	pid_t ret_val;
	int status;

	do {
		status = 0;
		ret_val =  waitpid(c_pid, &status, 0);
	} while((0 > ret_val && EINTR == errno) || (0 <= ret_val && !WIFEXITED(status)));

	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	else
		return EXIT_FAILURE;
}

static void invokation(char *prg_name)
{
	fprintf(stderr, "%s [-s|v] COMMENT cc-line\n", prg_name);
	fputs("\t-s: print the cc-line invoked when something\n\t    wents wrong (and -v was not given)\n", stderr);
	fputs("\t-v: no fancy printing, just echo cc-line\n", stderr);
}
