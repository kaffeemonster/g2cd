/* bin2o.c
 * little helper to make a .o from (data)files
 *
 * Copytight (c) 2006 - 2012 Jan Seiffert
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
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
#include "lib/my_bitopsm.h"

#undef false
#undef true
#define false 0
#define true (!false)

#define mkstr2(s) #s
#define mkstr(s) mkstr2(s)
#define strsize(s) (sizeof(s)-1)
#define writestr(fd, s) (strsize(s) == write(fd, s, strsize(s)))
#define output_writestr(fd, s) output_write(fd, s, strsize(s))
#define BITS_PER_CHAR CHAR_BIT
#define HOST_IS_BIGENDIAN host_is_bigendian()

enum as_dialect
{
	GAS,
	SUN,
	COFF
};

struct xf_buf
{
	size_t len;
	char *name;
	const char *buf;
};

static int invokation(char *);
static int output_write(int, void *, size_t);
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
static unsigned balignment = 8;
static int verbose;
static int export_base_data;
static int unhidden_tramp;
static int rel_tramp;
static int no_tramp;
#define PBUF_SIZE (1<<14)
static char pbuf[PBUF_SIZE];
/*               hex chars   "0x"        ", "        ".byte"                        "\n\t"                         reserve */
static char fbuf[PBUF_SIZE + PBUF_SIZE + PBUF_SIZE + DIV_ROUNDUP(PBUF_SIZE, 16)*6 + DIV_ROUNDUP(PBUF_SIZE, 16)*2 + 4];

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
							else if('c' == argv[i][0] || 'C' == argv[i][0])
								as_dia = COFF;
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
				else if('u' == argv[i][1])
					unhidden_tramp = true;
				else if('r' == argv[i][1])
					rel_tramp = true;
				else if('n' == argv[i][1])
					no_tramp = true;
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
// TODO: only set gnu stack on __linux__ && __ELF__
		if(!output_writestr(as_fd, "\t.section .note.GNU-stack,\"\",@progbits\n"))
			goto out;
		if(!output_writestr(as_fd, "\t.section .rodata,\"a\",@progbits\n"))
			goto out;
	} else if(SUN == as_dia) {
		if(!output_writestr(as_fd, "\t.section \".rodata\"\n"))
			goto out;
	} else if(COFF == as_dia) {
		if(!output_writestr(as_fd, "\t.section .rdata,\"dr\"\n"))
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

	if(!output_writestr(as_fd, "\t.ident \"G2C bin2o-0.1\"\n\n"))
		goto out;

out:
	close(as_fd);
	return collect_child(ch_pid);
}

static int output_write(int fd, void *buf, size_t len)
{
	if(verbose) {
		if(len != write(STDOUT_FILENO, buf, len)) {
			/* if this fails, so what... */
		}
	}
	if(len != write(fd, buf, len))
		return false;
	return true;
}

static int host_is_bigendian(void)
{
	static const union {
		unsigned int d;
		unsigned char endian[sizeof(unsigned int)];
	} x = {1};
	return x.endian[0] == 0;
}

char *to_base16(char *dst, const unsigned char *src, unsigned int len)
{
	static const unsigned char base16c[] = "0123456789abcdef";
	unsigned i;

	static const unsigned long vals[] =
	{
		MK_C(0x0f0f0f0fUL),
		MK_C(0x30303030UL),
	};

	for(; len >= SOST; len -= SOST, src += SOST)
	{
		size_t in_l, in_h, m_l, m_h;
		size_t t1, t2;

		in_l = *(const size_t *)src;

		in_h  = (in_l & (~vals[0])) >> 4;
		in_l &= vals[0];
		in_h += vals[1];
		in_l += vals[1];
		m_h   = has_greater(in_h, 0x39);
		m_l   = has_greater(in_l, 0x39);
		m_h >>= 7;
		m_l >>= 7;
		in_h += 0x27 * m_h;
		in_l += 0x27 * m_l;
		if(!HOST_IS_BIGENDIAN)
		{
			t1 = in_h;
			t2 = in_l;
			for(i = 0; i < SOST; i++, dst += 2) {
				dst[0] = (t1 & 0x000000ff);
				dst[1] = (t2 & 0x000000ff);
				t1 >>= BITS_PER_CHAR; t2 >>= BITS_PER_CHAR;
			}
		}
		else
		{
			t1 = in_h;
			t2 = in_l;
			for(i = SOST; i; i--) {
				dst[(i * 2) - 2] = (t1 & 0x000000ff);
				dst[(i * 2) - 1] = (t2 & 0x000000ff);
				t1 >>= BITS_PER_CHAR; t2 >>= BITS_PER_CHAR;
			}
			dst += SOST * 2;
		}
	}

	for(i = 0; i < len; i++) {
		*dst++ = base16c[src[i] / 16];
		*dst++ = base16c[src[i] % 16];
	}
	return dst;
}

static void prep_buffer(unsigned int lines)
{
	unsigned short *dstl = (unsigned short *)(fbuf + 2);
	unsigned i, j;
	unsigned short cs, ox, nt, pb, yt, es;

	if(HOST_IS_BIGENDIAN)
	{
		pb = 0x2e62;
		yt = 0x7974;
		es = 0x6520;
		cs = 0x2c20;
		ox = 0x3078;
		nt = 0x0a09;
	}
	else
	{
		pb = 0x622e;
		yt = 0x7479;
		es = 0x2065;
		cs = 0x202c;
		ox = 0x7830;
		nt = 0x090a;
	}

	fbuf[0] = '\n';
	fbuf[1] = '\t';
	i = lines;
	while(i--)
	{
		*dstl++ = pb;
		*dstl++ = yt;
		*dstl++ = es;
		for(j = 16; j; j--) {
			*dstl++ = ox;
			*dstl++ = 0;
			*dstl++ = cs;
		}
		dstl[-1] = nt;
	}
}

static char *format_lines(char *dst, char *src, unsigned int len)
{
	unsigned short *srcl = (unsigned short *)src;
	unsigned short *dstl = (unsigned short *)dst;
	unsigned int i, j;

	i = len / 32;
	len -= i * 32;
	dstl++;
	while(i--)
	{
		dstl += 3;
		dstl[ 0] = srcl[ 0];
		dstl[ 3] = srcl[ 1];
		dstl[ 6] = srcl[ 2];
		dstl[ 9] = srcl[ 3];
		dstl[12] = srcl[ 4];
		dstl[15] = srcl[ 5];
		dstl[18] = srcl[ 6];
		dstl[21] = srcl[ 7];
		dstl[24] = srcl[ 8];
		dstl[27] = srcl[ 9];
		dstl[30] = srcl[10];
		dstl[33] = srcl[11];
		dstl[36] = srcl[12];
		dstl[39] = srcl[13];
		dstl[42] = srcl[14];
		dstl[45] = srcl[15];
		srcl += 16;
		dstl += 48;
	}
	if(len)
	{
		dstl += 3;
		for(j = len/2; j--;) {
			*dstl++ = *srcl++;
			dstl += 2;
		}
	}
	dstl -= 2;
	return (char *)dstl;
}

static int hexdump_region(struct xf_buf *buf, int as_fd)
{
	size_t pos = 0;
	char *wptr;
	unsigned int len = buf->len - pos > PBUF_SIZE/2 ? PBUF_SIZE/2 : buf->len - pos;

	prep_buffer(DIV_ROUNDUP(len, 16));
	while(buf->len - pos)
	{
		to_base16(pbuf, (unsigned char *)buf->buf + pos, len);
		wptr = format_lines(fbuf + 2, pbuf, len * 2);
		if(!output_write(as_fd, fbuf, wptr - fbuf))
			return false;
		pos += len;
		len = buf->len - pos > PBUF_SIZE/2 ? PBUF_SIZE/2 : buf->len - pos;
	}

	return true;
}

static int dump_region(struct xf_buf *buf, int as_fd)
{
	static int e_sym = 0xA000;
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
		opt_pack = NULL;

	/* add a leading underscore if coff */
	if(COFF == as_dia) {
		memmove(buf->name + 1, buf->name, strlen(buf->name) + 1);
		buf->name[0] = '_';
	}

	if(export_base_data) {
		if(GAS == as_dia)
			w_ptr += sprintf(w_ptr, "\t.hidden %s_base_data\n", buf->name);
		w_ptr += sprintf(w_ptr, ".globl %s_base_data\n", buf->name);
	}
	w_ptr += sprintf(w_ptr, "%s_base_data:\n", buf->name);
	if(!output_write(as_fd, pbuf, w_ptr - pbuf))
		return false;

	if(!hexdump_region(buf, as_fd))
		return false;

	if(COFF == as_dia) {
		w_ptr  = pbuf + sprintf(pbuf, "\n\t.def %s_base_data\n\t\t.size . - %s_base_data\n", buf->name, buf->name);
		w_ptr += sprintf(w_ptr, "\t\t.scl 3\n");
		w_ptr += sprintf(w_ptr, "\t\t.type 60\n\t.endef\n");
	} else {
		w_ptr  = pbuf + sprintf(pbuf, "\n\t.size %s_base_data, . - %s_base_data\n", buf->name, buf->name);
		w_ptr += sprintf(w_ptr, "\t.type %s_base_data, %cobject\n", buf->name, GAS == as_dia ? '@' : '#');
	}
	if(!(export_base_data && no_tramp))
	{
		if(GAS == as_dia && !unhidden_tramp)
			w_ptr += sprintf(w_ptr, "\t.hidden %s\n", buf->name);
		w_ptr += sprintf(w_ptr, ".globl %s\n\t.align 8\n", buf->name);
		w_ptr += sprintf(w_ptr, "%s:\n", buf->name);
		if(!rel_tramp)
			w_ptr += sprintf(w_ptr, "\t.long %lu\n\t.long %s_base_data\n", (unsigned long) buf->len, buf->name);
		else
			w_ptr += sprintf(w_ptr, "\t.long %lu\n\t.long %s_base_data-%s\n", (unsigned long) buf->len, buf->name, buf->name);
		if(COFF == as_dia) {
			w_ptr += sprintf(w_ptr, "\t.def %s\n\t\t.size . - %s\n", buf->name, buf->name);
			w_ptr += sprintf(w_ptr, "\t\t.scl 3\n");
			w_ptr += sprintf(w_ptr, "\t\t.type 63\n\t.endef\n\n");
		} else {
			w_ptr += sprintf(w_ptr, "\t.size %s, . - %s\n", buf->name, buf->name);
			w_ptr += sprintf(w_ptr, "\t.type %s, %cobject\n\n", buf->name, GAS == as_dia ? '@' : '#');
		}
	}
	if(!output_write(as_fd, pbuf, w_ptr - pbuf))
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
			if((off_t)-1 == (tmp_len = lseek(fd, 0, SEEK_END))) {
				fprintf(stderr, __FILE__ ", " mkstr(__LINE__) ", finding filesize of '%s': %s\n", file_name, strerror(errno));
				exit(EXIT_FAILURE);
			}
			if((off_t)-1 == lseek(fd, 0, SEEK_SET)) {
				fprintf(stderr, __FILE__ "," mkstr(__LINE__) ", rewinding file '%s': %s\n", file_name, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}

		if(tmp_len > 0x7FFFFFFF) {
			fputs(__FILE__ ", " mkstr(__LINE__) ", do you really want to generate such large files?\n", stderr);
			exit(EXIT_FAILURE);
		}
		len = (size_t) tmp_len;
	}

#if _POSIX_MAPPED_FILES > 0
	if(MAP_FAILED == (tmp_buf = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0))) {
		fprintf(stderr, __FILE__ ", " mkstr(__LINE__) ", mmaping infile '%s': %s\n", file_name, strerror(errno));
		exit(EXIT_FAILURE);
	}
#else
	if(!(tmp_buf = malloc(len))) {
		fprintf(stderr, __FILE__ ", " mkstr(__LINE__) ", allocating memory for '%s':s %s\n", file_name, strerror(errno));
		exit(EXIT_FAILURE);
	}

	{
		size_t tmp_len = len;

		do
		{
			ssize_t ret_val = read(fd, tmp_buf + len - tmp_len, tmp_len);

			if(-1 == ret_val) {
				if(EINTR != errno) {
					fprintf(stderr, __FILE__ ", " mkstr(__LINE__) ", reading '%s':s %s\n", file_name, strerror(errno));
					exit(EXIT_FAILURE);
				}
			} else if(0 == ret_val) {
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
	fputs("\t-d [gas|sun|coff]\t- assembler dialekt\t(d: gas)\n", stderr);
	fputs("\t-o outfile\t- output filename\t(d: what as takes)\n", stderr);
	fputs("\t-e\t- also export the base data as symbol\n", stderr);
	fputs("\t-v\t- dump text written to assambler\n", stderr);
	fputs("\t-u\t- unhidden trampoline\n", stderr);
	fputs("\t-r\t- relative trampoline\n", stderr);
	fputs("\t-n\t- no trampoline, only with -e\n", stderr);
	fputs("\tfile\t- the filename to \n", stderr);
	return EXIT_FAILURE;
}

static char *xstrdup(const char *str)
{
	char *new_str;

	if(!str)
		return NULL;

	if(!(new_str = malloc(strlen(str) + 1 + 1)))
	{
		perror(__FILE__ ", " mkstr(__LINE__) ", strdup malloc()");
		exit(EXIT_FAILURE);
	}

	return strcpy(new_str, str);
}
