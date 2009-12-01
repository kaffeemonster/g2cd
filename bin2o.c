/* bin2o.c
 * little helper to make a .o from (data)files
 *
 * Copytight (c) 2006 - 2009 Jan Seiffert
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
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#if _POSIX_MAPPED_FILES > 0
# include <sys/mman.h>
#endif
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#undef false
#undef true
#define false 0
#define true (!false)

#define mkstr2(s) #s
#define mkstr(s) mkstr2(s)
#define strsize(s) (sizeof(s)-1)
#define writestr(fd, s) (strsize(s) == write(fd, s, strsize(s)))

enum as_dialect
{
	GAS,
	SUN,
};

struct xf_buf
{
	size_t len;
	char *name;
	const char *buf;
};

static int invokation(char *);
static char *char2hex(char *, char);
static char *dump_line(const char *, char *, int);
static int dump_region(struct xf_buf *, int);
static void get_file(struct xf_buf *, const char *);
static void put_file(struct xf_buf *);
static void trav_dir(const char *, int);
static int collect_child(pid_t);
static char *xstrdup(const char *);

static const char *as_name = "as";
static enum as_dialect as_dia = GAS;
static const char *o_file_name;
static char **i_file_name;
static int num_i_file;
static const char *opt_pack = NULL;
static unsigned balignment = 16;
static int verbose;
static int export_base_data;
#define PBUF_SIZE (1<<15)
static char pbuf[PBUF_SIZE];

int main(int argc, char **argv)
{
	int i;
	int pipe2as[2];
	int as_fd;
	pid_t ch_pid;

	if(2 > argc)
		exit(invokation(argv[0]));

	/* allocate space for in_file array, maybe a bit much, but... */
	if(!(i_file_name = calloc(argc, sizeof(const char *)))) {
		perror(__FILE__ ", " mkstr(__LINE__) ", i_file malloc()");
		exit(EXIT_FAILURE);
	}
	
	{
		int no_more_opt = 0;
		for(i = 1; i < argc; i++)
		{
			if(!no_more_opt && '-' == argv[i][0])
			{
				if(!argv[i][1])
					fputs("stray '-' in args!?\n", stderr);
				else if('-' == argv[i][1])
					no_more_opt = true;
				else if('a' == argv[i][1] || 'd' == argv[i][1] || 'p' == argv[i][1] || 'o' == argv[i][1] || 'l' == argv[i][1])
				{
					char opt_c = argv[i][1];
					if((i + 1) < argc)
					{
						i++;
						if('o' == opt_c)
							o_file_name = xstrdup(argv[i]);
						else if('p' == opt_c)
							opt_pack = xstrdup(argv[i]);
						else if('a' == opt_c)
							as_name = xstrdup(argv[i]);
						else if('d' == opt_c)
						{
							/* simplify test, strcmp would be the right thing, but... */
							if('g' == argv[i][0] || 'G' == argv[i][0])
								as_dia = GAS;
							else if('s' == argv[i][0] || 'S' == argv[i][0])
								as_dia = SUN;
							else
								fprintf(stderr, "'%s' is an unknown assembler dialect!\n", argv[i]);
						}
						else if('l' == opt_c)
							balignment = atoi(argv[i]);
					}
					else
						fprintf(stderr, "option '-%c' needs an arg!\n", opt_c);
				}
				else if('v' == argv[i][1])
					verbose = true;
				else if('e' == argv[i][1])
					export_base_data = true;
				else
					fprintf(stderr, "unknown option: '-%c'\n", argv[i][1]);
				continue;
			}

			i_file_name[num_i_file++] = xstrdup(argv[i]);
		}
		if(!num_i_file && !opt_pack)
			puts("Warning: no input files? Will generate empty outfile!");
	}

	if(0 > pipe(pipe2as)) {
		perror(__FILE__ ", " mkstr(__LINE__) ", creating pipe");
		exit(EXIT_FAILURE);
	}

	ch_pid = fork();
	if(-1 == ch_pid) {
		perror(__FILE__ ", " mkstr(__LINE__) ", fork()");
		exit(EXIT_FAILURE);
	}

	/* child ? */
	if(0 == ch_pid)
	{
		close(pipe2as[1]);
		if(STDIN_FILENO != pipe2as[0])
		{
			if(-1 == dup2(pipe2as[0], STDIN_FILENO)) {
				perror(__FILE__ ", " mkstr(__LINE__) ", dup2()");
				exit(EXIT_FAILURE);
			}
			close(pipe2as[0]);
		}
		if(o_file_name)
			execlp(as_name, as_name, "-o", o_file_name, "-", (char *) NULL);
		else
			execlp(as_name, as_name, "-", (char *) NULL);
		fprintf(stderr, __FILE__ ", " mkstr(__LINE__) ", execlp('%s'): %s\n", as_name, strerror(errno));
		exit(EXIT_FAILURE);
	}
	close(pipe2as[0]);
	as_fd = pipe2as[1];
	/* check if our child is still with us */
	if(0 < waitpid(ch_pid, &i, WNOHANG))
	{
		fprintf(stderr, __FILE__ ", " mkstr(__LINE__) ", lost child '%s'(%i)\n", as_name, (int) ch_pid);
		if(WIFEXITED(i))
			exit(WEXITSTATUS(i));
		else
			exit(EXIT_FAILURE);
	}

	if(GAS == as_dia)
	{
		if(verbose)
			(void)writestr(STDOUT_FILENO, "\t.section .rodata,\"a\",@progbits\n");
		if(!writestr(as_fd, "\t.section .rodata,\"a\",@progbits\n"))
			goto out;
	}
	else
	{
		if(verbose)
			(void)writestr(STDOUT_FILENO, "\t.section \".rodata\"\n");
		if(!writestr(as_fd, "\t.section \".rodata\"\n"))
			goto out;
	}

	for(i = 0; i < num_i_file; i++)
	{
		struct xf_buf file;
		get_file(&file, i_file_name[i]);

		if(!dump_region(&file, as_fd))
			goto out;

		put_file(&file);
	}
	if(opt_pack)
		trav_dir(opt_pack, 0);
//		if(!writestr(as_fd, "\t.section .comments,\"a\",@progbits\n"))
//			goto end_pack;
	if(verbose)
		(void)writestr(STDOUT_FILENO, "\t.ident \"G2C bin2o-0.1\"\n\n");
	if(!writestr(as_fd, "\t.ident \"G2C bin2o-0.1\"\n\n"))
		goto out;

out:
	close(as_fd);
	return collect_child(ch_pid);
}

static char *char2hex(char *buf, char c)
{
	static const char hexchars[] = "0123456789ABCDEF";
	*buf++ = '0';
	*buf++ = 'x';
	*buf++ = hexchars[(c & 0x0f0) >> 4];
	*buf++ = hexchars[c & 0x00f];
	return buf;
}

static char *dump_line(const char *in, char *out, int count)
{
#define dump_a_hex(out, in) *out++ = ' '; out = char2hex(out, in); *out++ = ',';
	int i = 0;
	*out++ = '\t'; *out++ = '.'; *out++ = 'b'; *out++ = 'y'; *out++ = 't'; *out++ = 'e';
	switch(count % 8)
	{
	case 0:
		while(i < count)
		{
			dump_a_hex(out, in[i++])
	case 1:
			dump_a_hex(out, in[i++])
	case 2:
			dump_a_hex(out, in[i++])
	case 3:
			dump_a_hex(out, in[i++])
	case 4:
			dump_a_hex(out, in[i++])
	case 5:
			dump_a_hex(out, in[i++])
	case 6:
			dump_a_hex(out, in[i++])
	case 7:
			dump_a_hex(out, in[i++])
		}
	}
#undef dump_a_hex
	/* remove last ',' */
	*(out-1) = '\n';
	*out = '\0';
	return out;
}

static int dump_region(struct xf_buf *buf, int as_fd)
{
	static int e_sym = 0xA000;
	size_t pos;
	char *c_ptr;
	char *w_ptr;

	/* clean up the varname a bit... */
	if((c_ptr = strrchr(buf->name, '/')))
		memmove(buf->name, c_ptr+1, strlen(c_ptr+1)+1);
	if(0 == strlen(buf->name))
		sprintf(buf->name, "%X", e_sym++);
	w_ptr = pbuf + sprintf(pbuf, "\t.file \"%s\"\n\t.align %u\n", buf->name, balignment);

	if((c_ptr = strrchr(buf->name, '.')))
		*c_ptr = '\0';
	while((c_ptr = strchr(buf->name, ' ')))
		*c_ptr = '_';
	while((c_ptr = strchr(buf->name, '.')))
		*c_ptr = '_';
	while((c_ptr = strchr(buf->name, '-')))
		*c_ptr = '_';
	if(opt_pack && !strcmp("sbox", buf->name))
	{
		puts("setze opt_pack null");
		opt_pack = NULL;
	}

	if(export_base_data) {
		if(GAS == as_dia)
			w_ptr += sprintf(w_ptr, "\t.hidden %s_base_data\n", buf->name);
		w_ptr += sprintf(w_ptr, ".globl %s_base_data\n", buf->name);
	}
	w_ptr += sprintf(w_ptr, "\t.type %s_base_data,%cobject\n%s_base_data:\n", buf->name, GAS == as_dia ? '@' : '#', buf->name);
	for(pos = 0; (pos + 16) < buf->len; pos += 16)
	{
		if(w_ptr > (pbuf + PBUF_SIZE - 240))
		{
			if(verbose) {
				if(w_ptr - pbuf != write(STDOUT_FILENO, pbuf, w_ptr - pbuf)) {
				}
			}
			if((w_ptr - pbuf) != write(as_fd, pbuf, w_ptr - pbuf))
				return false;
			w_ptr = pbuf;
		}
		w_ptr = dump_line(buf->buf + pos, w_ptr, 16);
	}
	w_ptr = dump_line(buf->buf + pos, w_ptr, buf->len - pos);
	if(verbose) {
		if(w_ptr - pbuf != write(STDOUT_FILENO, pbuf, w_ptr - pbuf)) {
		}
	}
	if((w_ptr - pbuf) != write(as_fd, pbuf, w_ptr - pbuf))
		return false;
	w_ptr = pbuf + sprintf(pbuf, "\t.size %s_base_data, . - %s_base_data\n", buf->name, buf->name);
	if(GAS == as_dia)
		w_ptr += sprintf(w_ptr, "\t.hidden %s\n", buf->name);
	w_ptr += sprintf(w_ptr, ".globl %s\n\t.align 8\n", buf->name);
	w_ptr += sprintf(w_ptr, "\t.type %s,%cobject\n%s:\n", buf->name, GAS == as_dia ? '@' : '#', buf->name);
	w_ptr += sprintf(w_ptr, "\t.long %lu\n\t.long %s_base_data\n", (unsigned long) buf->len, buf->name);
	w_ptr += sprintf(w_ptr, "\t.size %s, . - %s\n\n", buf->name, buf->name);
	if(verbose) {
		if(w_ptr - pbuf != write(STDOUT_FILENO, pbuf, w_ptr - pbuf)) {
		}
	}
	if((w_ptr - pbuf) != write(as_fd, pbuf, w_ptr - pbuf))
		return false;
	return true;
}

static void get_file(struct xf_buf *buf, const char *file_name)
{
	char *tmp_buf;
	size_t len;
	int fd = open(file_name, O_RDONLY);

	if(0 > fd)
	{
		fprintf(stderr, __FILE__ ", " mkstr(__LINE__) ", opening infile '%s': %s\n", file_name, strerror(errno));
		exit(EXIT_FAILURE);
	}

	{
		struct stat st_buf;
		off_t tmp_len;
		if(0 == fstat(fd, &st_buf))
			tmp_len = st_buf.st_size;
		else
		{
			if((off_t)-1 == (tmp_len = lseek(fd, 0, SEEK_END)))
			{
				fprintf(stderr, __FILE__ ", " mkstr(__LINE__) ", finding filesize of '%s': %s\n", file_name, strerror(errno));
				exit(EXIT_FAILURE);
			}
			if((off_t)-1 == lseek(fd, 0, SEEK_SET))
			{
				fprintf(stderr, __FILE__ "," mkstr(__LINE__) ", rewinding file '%s': %s\n", file_name, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}

		if(tmp_len > 0x7FFFFFFF)
		{
			fputs(__FILE__ ", " mkstr(__LINE__) ", do you really want to generate such large files?\n", stderr);
			exit(EXIT_FAILURE);
		}
		len = (size_t) tmp_len;
	}

#if _POSIX_MAPPED_FILES > 0
	if(MAP_FAILED == (tmp_buf = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0)))
	{
		fprintf(stderr, __FILE__ ", " mkstr(__LINE__) ", mmaping infile '%s': %s\n", file_name, strerror(errno));
		exit(EXIT_FAILURE);
	}
#else
	if(!(tmp_buf = malloc(len)))
	{
		fprintf(stderr, __FILE__ ", " mkstr(__LINE__) ", allocating memory for '%s':s %s\n", file_name, strerror(errno));
		exit(EXIT_FAILURE);
	}

	{
		size_t tmp_len = len;

		do
		{
			ssize_t ret_val = read(fd, tmp_buf + len - tmp_len, tmp_len);

			if(-1 == ret_val)
			{
				if(EINTR != errno)
				{
					fprintf(stderr, __FILE__ ", " mkstr(__LINE__) ", reading '%s':s %s\n", file_name, strerror(errno));
					exit(EXIT_FAILURE);
				}
			}
			else if(0 == ret_val)
			{
				len -= tmp_len;
				break;
			}
			else
				tmp_len -= ret_val;
		} while(tmp_len);
	}
#endif
	close(fd);

	buf->buf = tmp_buf;
	buf->name = xstrdup(file_name);
	buf->len = len;
}

static void put_file(struct xf_buf *buf)
{
#if _POSIX_MAPPED_FILES > 0
	munmap((void *)buf->buf, buf->len);
#else
	free(buf->buf);
#endif
	free(buf->name);
	buf->buf = NULL;
	buf->name = NULL;
	buf->len = 0;
}

static void trav_dir(const char *dir_name, int level)
{
	DIR *pdir = opendir(dir_name);
	struct dirent *entry;
	size_t d_len = strlen(dir_name);
//	int i;

//	for(i = 0; i < level; i++)
//		printf("\t");
//	puts("--------------- in trav_dir --------------");

	if(!pdir)
		return;

	while((entry = readdir(pdir)))
	{
		static struct stat st_buf;
		size_t e_len = strlen(entry->d_name);
		if(!strcmp("zlib", entry->d_name))
			continue;
		else
		{
			char n_path[d_len + e_len + 5];
			memcpy(n_path, dir_name, d_len);
			n_path[d_len] = '/';
			memcpy(n_path + d_len + 1, entry->d_name, e_len);
			n_path[d_len + 1 + e_len] = '\0';
			if(stat(n_path, &st_buf))
				continue;
		
			if(S_ISDIR(st_buf.st_mode))
			{
				if('.' != entry->d_name[0] || ('\0' != entry->d_name[1] && ('.' != entry->d_name[1] || '\0' != entry->d_name[2])))
					trav_dir(n_path, level + 1);
			}
			else
			{
				char *c_ptr = strrchr(n_path, '.');
				if(!c_ptr)
					continue;
//				if(('c' == c_ptr[1] || 'h' == c_ptr[1]) && '\0' == c_ptr[2])
//					printf("%s\n", n_path);
			}
		}
	}
	closedir(pdir);
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
	fprintf(stderr, "%s [-v] [-a as] [-d gas|sun] [-o outfile] file file file ...\n", prg_name);
	fputs("\t-a as\t- assembler-bin to use\t(d: as)\n",stderr);
	fputs("\t-d [gas|sun]\t- assembler dialekt\t(d: gas)\n", stderr);
	fputs("\t-o outfile\t- output filename\t(d: what as takes)\n", stderr);
	fputs("\t-e\t- also export the base data as symbol\n", stderr);
	fputs("\t-v\t- dump text written to assambler\n", stderr);
	fputs("\tfile\t- the filename to \n", stderr);
	return EXIT_FAILURE;
}

static char *xstrdup(const char *str)
{
	char *new_str;

	if(!str)
		return NULL;

	if(!(new_str = malloc(strlen(str) + 1)))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", strdup malloc()");
		exit(EXIT_FAILURE);
	}

	return strcpy(new_str, str);
}
