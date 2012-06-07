/*
 * G2PacketTyperGenerator.c
 * Automatic generator for the G2-Packet typer tables
 *
 * Copyright (c) 2008-2012 Jan Seiffert
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "config.h"
#include "G2Packet.h"

struct p_names
{
	const char c[8];
	const long long weight;
};

#define ENUM_CMD(x, y) { .c = str_it(x), .weight = (y) }
const struct p_names p_names[] =
{
	G2_PACKET_TYPES
};
#undef ENUM_CMD

struct p_table {
	unsigned start;
	unsigned type;
	unsigned idx;
	long long weight;
};

#define die(x) do { fprintf(stderr, "%s::%s@%d: %s\n", __FILE__, __func__, __LINE__, x); exit(EXIT_FAILURE); } while(false)

# define swab32(x) ((unsigned int)(				\
	(((unsigned int)(x) & (unsigned int)0x000000ffUL) << 24) |		\
	(((unsigned int)(x) & (unsigned int)0x0000ff00UL) <<  8) |		\
	(((unsigned int)(x) & (unsigned int)0x00ff0000UL) >>  8) |		\
	(((unsigned int)(x) & (unsigned int)0xff000000UL) >> 24)))

# define swab64(x) ((unsigned long long)(				\
	(((unsigned long long)(x) & (unsigned long long)0x00000000000000ffULL) << 56) |	\
	(((unsigned long long)(x) & (unsigned long long)0x000000000000ff00ULL) << 40) |	\
	(((unsigned long long)(x) & (unsigned long long)0x0000000000ff0000ULL) << 24) |	\
	(((unsigned long long)(x) & (unsigned long long)0x00000000ff000000ULL) <<  8) |	\
	(((unsigned long long)(x) & (unsigned long long)0x000000ff00000000ULL) >>  8) |	\
	(((unsigned long long)(x) & (unsigned long long)0x0000ff0000000000ULL) >> 24) |	\
	(((unsigned long long)(x) & (unsigned long long)0x00ff000000000000ULL) >> 40) |	\
	(((unsigned long long)(x) & (unsigned long long)0xff00000000000000ULL) >> 56)))

static const char *f_name = "G2PacketTyper.h";
static int target_is_bigendian;
static int verbose;
static FILE *out;
static unsigned table_used, table_base_num;
static struct p_table *f;

static void print_trailer(void);
static void print_table(void);
static void print_score(void);
static void print_weight(void);
static int host_is_bigendian(void);

static int p_table_cmp(const void *a, const void *b)
{
	const struct p_table *aa = a, *bb = b;
	int diff;

	diff = aa->start - bb->start;
	if(diff)
		return diff;
	return bb->weight - aa->weight;
}

static int p_table_cmp_swp(const void *a, const void *b)
{
	const struct p_table *aa = a, *bb = b;
	int diff;

	diff = swab32(aa->start) - swab32(bb->start);
	if(diff)
		return diff;
	return bb->weight - aa->weight;
}

static void new_p_table(void)
{
	unsigned i;
	unsigned real_num, used, back_index, base_num;

	f = malloc(PT_MAXIMUM * 2 * sizeof(*f));
	if(!f)
		die("allocating mem");
	real_num = PT_MAXIMUM * 2;
	base_num = back_index = PT_MAXIMUM-1;

	/* add all packets to the array */
	for(i = 1, used = 0, back_index; i < PT_MAXIMUM; i++)
	{
		memcpy(&f[i-1].start, &p_names[i].c[0], 4);
		used++;
		f[i-1].weight = p_names[i].weight;
		if(p_names[i].c[3] != '\0')
		{
			f[i-1].type = PT_UNKNOWN;
			f[i-1].idx = back_index;
			memcpy(&f[back_index].start, &p_names[i].c[4], 4);
			f[back_index].weight = p_names[i].weight;
			f[back_index].type = i;
			f[back_index].idx  = 0;
			used++;
			back_index++;
		}
		else
		{
			f[i-1].type = i;
			f[i-1].idx  = 0;
		}
		if(used >= real_num)
		{
			struct p_table *t = realloc(f, real_num*2*sizeof(*f));
			if(t) {
				real_num *= 2;
				f = t;
			} else {
				die("reallocating mem");
			}
		}
	}
	if(verbose)
	{
		for(i = 0; i < used; i++)
		{
			printf("%3.0u: \"%-4.4s\"\tt: %u\ti: %u\tw: %lli\n",
					i, (const char *)&f[i].start, f[i].type, f[i].idx,
					f[i].weight);
		}
		puts("----------------------------------------");
	}

	/* sort it */
	qsort(f, PT_MAXIMUM-1, sizeof(*f),
		target_is_bigendian != host_is_bigendian() ? p_table_cmp_swp : p_table_cmp);
	if(verbose)
	{
		for(i = 0; i < used; i++)
		{
			printf("%3.0u: \"%-4.4s\"\tt: %u\ti: %u\tw: %lli\n",
					i, (const char *)&f[i].start, f[i].type, f[i].idx,
					f[i].weight);
		}
		puts("----------------------------------------");
	}

	/* remove and chain duplicates */
	for(i = 1; i < base_num; i++)
	{
		unsigned j;

		if(f[i-1].start != f[i].start)
			continue;

		j = f[i-1].idx;
		while(f[j].idx != 0)
			j = f[j].idx;

		f[j].idx = f[i].idx;

		if(i + 1 < used)
			memmove(&f[i], &f[i+1], (used - (i+1)) * sizeof(*f));
		base_num--;
		used--;
		i--;
		for(j = 0; j < used; j++) {
			if(f[j].idx)
				f[j].idx--;
		}
	}
	if(verbose)
	{
		for(i = 0; i < used; i++)
		{
			printf("%3.0u: \"%-4.4s\"\tt: %u\ti: %u\tw: %lli\n",
					i, (const char *)&f[i].start, f[i].type, f[i].idx,
					f[i].weight);
		}
		puts("----------------------------------------");
	}

	/* sort second tier */
	back_index = base_num;
	for(i = 0; i < base_num; i++)
	{
		unsigned j, idx;
		struct p_table t;

		if(f[i].idx == 0)
			continue;

		if(f[i].idx != back_index)
		{
			idx = f[i].idx;
			t = f[back_index];
			f[back_index] = f[idx];
			f[idx] = t;
			for(j = 0; j < used; j++) {
				if(f[j].idx == back_index) {
					f[j].idx = idx;
					break;
				}
			}
		}
		idx = back_index++;
		f[i].idx = idx;

		while(f[idx].idx != 0)
		{
			unsigned u = f[idx].idx;

			if(u != back_index)
			{
				t = f[u];
				f[u] = f[back_index];
				f[back_index] = t;
				for(j = 0; j < used; j++) {
					if(f[j].idx == back_index) {
						f[j].idx = u;
						break;
					}
				}
				f[idx].idx = back_index;
			}
			idx = back_index++;
		}
	}
	if(verbose)
	{
		for(i = 0; i < used; i++)
		{
			printf("%3.0u: \"%-4.4s\"\tt: %u\ti: %u\tw: %lli\n",
					i, (const char *)&f[i].start, f[i].type, f[i].idx,
					f[i].weight);
		}
		puts("----------------------------------------");
	}

	for(i = 0; i < used; i++) {
		if(f[i].idx > 0xffff)
			die("Table to big!! New trick needed");
	}

	table_used = used;
	table_base_num = base_num;
}

int main(int argc, char *argv[])
{
	int i;

	if(PT_MAXIMUM > 0xfff)
		die("More than 12Bit packet types!! new trick needed");

	for(i = 1; i < argc; i++)
	{
		if('-' == argv[i][0])
		{
			int len = strlen(argv[i]);
			int j;
			for(j = 1; j < len; j++)
			{
				switch(argv[i][j])
				{
				case 'v':
					verbose++;
					break;
				case 'c':
					print_weight();
					return EXIT_SUCCESS;
				case 'b':
					target_is_bigendian = !0;
					break;
				case 'l':
					target_is_bigendian = 0;
					break;
				default:
					break;
				}
			}
		}
		else
			f_name = argv[i];
	}

	if(verbose > 1) {
		for(i = 1; i < PT_MAXIMUM; i++)
			printf("%s\t", p_names[i].c);
		putchar('\n');
	}

	new_p_table();

	if(!(out = fopen(f_name, "w")))
		die("couldn't open out file");
	print_trailer();
	fflush(out);
	print_table();
	print_score();
	fflush(out);
	fclose(out);

	return EXIT_SUCCESS;
}

static void print_table(void)
{
	unsigned i;
	char ap_name[32];

	if(target_is_bigendian)
		fputs("\t/* table is big endian */\n", out);
	else
		fputs("\t/* table is little endian */\n", out);

	strcpy(ap_name, "PT_");
	if(target_is_bigendian == host_is_bigendian())
	{
		for(i = 0; i < table_used; i++)
		{
			strcpy(&ap_name[3], p_names[f[i].type].c);
			fprintf(out, "\t{0x%08x, %12s, %3u},\t /* \"%-4.4s\" */\n",
				f[i].start, ap_name, f[i].idx, (const char *)&f[i].start);
		}
	}
	else
	{
		for(i = 0; i < table_used; i++)
		{
			strcpy(&ap_name[3], p_names[f[i].type].c);
			fprintf(out, "\t{0x%08x, %12s, %3u},\t /* \"%-4.4s\" */\n",
				swab32(f[i].start), ap_name, f[i].idx, (const char *)&f[i].start);
		}
	}
}

static void print_trailer(void)
{
	struct tm *now_tm, a_tm;
	time_t now;

	now = time(NULL);
	if(!(now_tm = gmtime(&now))) {
		now_tm = &a_tm;
		now_tm->tm_year = 108;
	}
	fprintf(out, "/*\n"
		" * %s\n"
		" * automatic generated parser dict for G2-packets\n"
		" *\n"
		" * Copyright (c) 2004-%u Jan Seiffert\n"
		" *\n"
		" * This file is part of g2cd.\n"
		" *\n"
		" * g2cd is free software; you can redistribute it and/or modify\n"
		" * it under the terms of the GNU General Public License as\n"
		" * published by the Free Software Foundation, either version 3\n"
		" * of the License, or (at your option) any later version.\n"
		" *\n"
		" * g2cd is distributed in the hope that it will be useful, but\n"
		" * WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		" * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
		" * GNU General Public License for more details.\n"
		" *\n"
		" * You should have received a copy of the GNU General Public\n"
		" * License along with g2cd.\n"
		" * If not, see <http://www.gnu.org/licenses/>.\n"
		" *\n"
		" */\n"
		"\n"
		"/*\n"
		" * packet typer\n"
		" *\n"
		" */\n"
		"#define T_NUM_BASE %u\n"
		"#define T_NUM_TOTAL %u\n"
		"\n"
		"static const struct\n"
		"{\n"
		"	const unsigned int c;\n"
		"	const unsigned short t;\n"
		"	const unsigned short idx;\n"
		"} g2_ptyper_table[] =\n"
		"{\n",
	f_name, now_tm->tm_year + 1900, table_base_num, table_used);
}

static void print_score(void)
{
	fputs("};\n"
		"/* EOF */\n",
		out);
};

static void print_weight(void)
{
	unsigned i;
	for(i = 0; i < PT_MAXIMUM; i++)
		printf("%s\tcount: %llu\n", p_names[i].c, p_names[i].weight);
}

static int host_is_bigendian(void)
{
	static const union {
		unsigned int d;
		unsigned char endian[sizeof(unsigned int)];
	} x = {1};
	return x.endian[0] == 0;
}
