/*
 * G2PacketTyperGenerator.c
 * Automatic generator for the G2-Packet typer tables
 *
 * Copyright (c) 2008-2010 Jan Seiffert
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

#define die(x) do { fprintf(stderr, "%s::%s@%d: %s\n", __FILE__, __func__, __LINE__, x); exit(EXIT_FAILURE); } while(false)

struct tree_hl
{
	char c;
	bool last;
	enum g2_ptype type;
	long long weight;
	int line;
	size_t num_child;
	size_t size_child;
	struct tree_hl *child[];
};

struct table_hl
{
	char c;
	bool last;
	bool last_table;
	enum g2_ptype type;
	uint8_t delta;
	unsigned next;
};

static struct tree_hl *tree_hl_first[128];
static unsigned table_hl_first[128];
static struct table_hl table_hl[1024];
static unsigned table_max;
static const char *f_name = "G2PacketTyper.h";
static FILE *out;


static void make_tree_hl(void);
static void print_tree_hl(void);
static void sort_tree_hl(void);
static void clean_tree_hl(void);
static void tree2table(void);
static void print_table_hl(void);
static void print_table_ll(void);
static void print_trailer(void);
static void print_score(void);
static void print_weight(void);

int main(int argc, char *argv[])
{
	int i, verbose = 0;

	if(PT_MAXIMUM > 0x7F)
		die("More than 7Bit packet types!! new trick needed");

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

	memset(table_hl_first, -1, sizeof(table_hl_first));
	make_tree_hl();
	if(verbose > 2)
		print_tree_hl();
	sort_tree_hl();
	if(verbose > 3)
		print_tree_hl();
	clean_tree_hl();
	if(verbose)
		print_tree_hl();
	tree2table();
	if(verbose)
		print_table_hl();
	for(i = 0; i < (sizeof(table_hl_first)/sizeof(table_hl_first[0])); i++) {
		if((unsigned)-1 != table_hl_first[i] && 0xFF < table_hl_first[i])
			die("jump delta to big, new tricks needed!!");
	}

	if(!(out = fopen(f_name, "w")))
		die("couldn't open out file");
	print_trailer();
	fflush(out);
	print_table_ll();
	print_score();
	fflush(out);
	fclose(out);

	return EXIT_SUCCESS;
}

static void make_tree_hl_name_r(const char *name, struct tree_hl *t, long long weight, enum g2_ptype type)
{
	struct tree_hl *x;
	int i;
	char c = name[0];

	t->weight += weight;
	if(c) {
		if(c < 0x20)
			die("char smaller 0x20!! Control char in packetname?? Trick needed");
		if((unsigned)c > 0x7F)
			die("char greater 7Bit!! new trick needed");
		for(i = 0; i < t->num_child; i++) {
			if(t->child[i] && t->child[i]->c == c)
				goto recurse;
		}
	}

	if(!(x = calloc(1, sizeof(*x) + (c ? 4 : 1)*sizeof(x))))
		die("Error allocating mem");
	x->c = c;
	x->size_child = c ? 4 : 1;
	t->child[t->num_child++] = x;
	if(!c)
	{
		x->last = true;
		x->type = type;
		x->weight = weight;
		return;
	} else
		i = t->num_child - 1;

recurse:
	make_tree_hl_name_r(&name[1], t->child[i], weight, type);
	if(t->child[i]->num_child >= t->child[i]->size_child)
	{
		if(!(x = realloc(t->child[i], sizeof(*t) + t->child[i]->size_child * 2 * sizeof(t))))
			die("Error reallocating mem");
		x->size_child *= 2;
		t->child[i] = x;
	}
}

static void make_tree_hl_name(const char *name, long long weight, enum g2_ptype type)
{
	struct tree_hl *t;
	unsigned c = name[0];

	if(!c)
		return;
	if(c < 0x20)
		die("char smaller 0x20!! Control char in packetname?? Trick needed");
	if(c > 0x7F)
		die("char greater 7Bit!! new trick needed");

	if(!(t = tree_hl_first[c]))
	{
		if(!(t = calloc(1, sizeof(*t) + 4 * sizeof(t))))
			die("Error allocating mem");
		t->c = c;
		t->size_child = 4;
		tree_hl_first[c] = t;
	}

	make_tree_hl_name_r(&name[1], t, weight, type);
	if(t->num_child >= t->size_child)
	{
		struct tree_hl *x;
		if(!(x = realloc(t, sizeof(*t) + t->size_child * 2 * sizeof(t))))
			die("Error reallocating mem");
		x->size_child *= 2;
		tree_hl_first[c] = x;
	}
}

static void make_tree_hl(void)
{
	int i;
	for(i = 1; i < PT_MAXIMUM; i++) {
		if(p_names[i].c[0] & 0x80)
			die("8 Bit character on begining of packet name?");
		make_tree_hl_name(p_names[i].c, p_names[i].weight, i);
	}
}

static void clean_tree_hl_r(struct tree_hl *t, unsigned level)
{
	int i;

	if(t->num_child == 1 &&
	   t->child[0]->c == '\0' &&
	   t->child[0]->last &&
	   level != 1)
	{
		t->last = true;
		t->type = t->child[0]->type;
		free(t->child[0]);
		t->child[0] = NULL;
		t->num_child = 0;
	}

	for(i = 0; i < t->num_child; i++)
		clean_tree_hl_r(t->child[i], level + 1);

}

static void clean_tree_hl(void)
{
	int i;
	for(i = 0; i < sizeof(tree_hl_first)/sizeof(tree_hl_first[0]); i++) {
		if(!tree_hl_first[i])
			continue;
		clean_tree_hl_r(tree_hl_first[i], 1);
	}
}

static void print_tree_hl_r(struct tree_hl *t, int level)
{
	printf("%c %lli%s\t", t->c ? t->c : '0', t->weight, t->last ? "!" : "");

	if(!t->last)
	{
		int i;
		for(i = 0; i < t->num_child; i++)
		{
			if(i) {
				int j;
				for(j = 0; j < level ; j++)
					putchar('\t');
			}
			print_tree_hl_r(t->child[i], level + 1);
		}
	}
	else
		putchar('\n');
}

static void print_tree_hl(void)
{
	int i;
	for(i = 0; i < sizeof(tree_hl_first)/sizeof(tree_hl_first[0]); i++)
	{
		if(!tree_hl_first[i])
			continue;
		print_tree_hl_r(tree_hl_first[i], 1);
		putchar('\n');
	}
}

static int tree_hl_cmp(const void *a, const void *b)
{
	const struct tree_hl * const *x = a;
	const struct tree_hl * const *y = b;

	return (*y)->weight - (*x)->weight;
}

static void sort_tree_hl_r(struct tree_hl *t)
{
	int i;

	if(t->last)
		return;

	if(t->num_child)
	{
		for(i = 0; i < t->num_child; i++)
			sort_tree_hl_r(t->child[i]);

		qsort(t->child, t->num_child, sizeof(t->child[0]), tree_hl_cmp);
	}
}

static void sort_tree_hl(void)
{
	int i;
	for(i = 0; i < sizeof(tree_hl_first)/sizeof(tree_hl_first[0]); i++) {
		if(!tree_hl_first[i])
			continue;
		sort_tree_hl_r(tree_hl_first[i]);
	}
}

static void print_table_hl(void)
{
	unsigned i;
	for(i = 0; i < table_max; i++)
	{
		char tmp[16];
		int j = 8 - strlen(p_names[table_hl[i].type].c);
		tmp[j] = '\0';
		while(j)
			tmp[--j] = ' ';
		strcat(tmp, p_names[table_hl[i].type].c);

		printf("[% 4d] '%c' %s\tla: %c lt: %c n: %-4u d: %-4u\n%s", i,
		       table_hl[i].c ? table_hl[i].c : '0',
		       tmp, table_hl[i].last ? 't' : 'f',
		       table_hl[i].last_table ? 't' : 'f', table_hl[i].next,
		       table_hl[i].delta, table_hl[i].last_table ? "\n" : "");
	}
}

static void tree2table_r(struct tree_hl *t)
{
	struct table_hl *x;
	unsigned i;

	for(i = 0; i < t->num_child; i++)
	{
		x = &table_hl[table_max];
		t->child[i]->line = table_max++;
		x->c = t->child[i]->c;
		x->type = t->child[i]->type;
		x->last = t->child[i]->last;
	}
	table_hl[table_max - 1].last_table = true;
	for(i = 0; i < t->num_child; i++)
	{
		if(!t->child[i]->last) {
			table_hl[t->child[i]->line].next = table_max;
			table_hl[t->child[i]->line].delta = table_max - t->child[i]->line;
			if(table_hl[t->child[i]->line].delta > 0x7F)
				die("Table delta to big!! New trick needed");
		}
		tree2table_r(t->child[i]);
	}
}

static void tree2table(void)
{
	int i;
	for(i = 0; i < sizeof(tree_hl_first)/sizeof(tree_hl_first[0]); i++)
	{
		if(!tree_hl_first[i])
			continue;
		if(table_max % 2)
			table_max++;
		table_hl_first[i] = table_max / 2;
		tree2table_r(tree_hl_first[i]);
	}
}

static void print_table_ll_r(unsigned index, unsigned level)
{
	unsigned i = index;

	do
	{
		unsigned t;
		char c[3];

		c[0]= table_hl[i].c ? table_hl[i].c : '\\';
		c[1]= table_hl[i].c ? '\0' : '0';
		c[2]= '\0';
		fprintf(out, "\t[% 4d] =", i);
		for(t = 0; t < level; t++)
			putc('\t', out);
		if(!table_hl[i].last && !table_hl[i].last_table)
			fprintf(out, "T_NEXT('%s',% 3d),\n", c, table_hl[i].delta);
		else if(!table_hl[i].last && table_hl[i].last_table)
			fprintf(out, "T_TEND('%s',% 3d),\n", c, table_hl[i].delta);
		else if(table_hl[i].last && !table_hl[i].last_table)
			fprintf(out, "T_LAST('%s', PT_%s),\n", c, p_names[table_hl[i].type].c);
		else
			fprintf(out, "T_LEND('%s', PT_%s),\n", c, p_names[table_hl[i].type].c);
	} while(!table_hl[i++].last_table);

	i = index;
	do
	{
		if(!table_hl[i].last) {
			int j = i + table_hl[i].delta;
			print_table_ll_r(j, level + 1);
		}
	} while(!table_hl[i++].last_table);
}

static void print_table_ll(void)
{
	int i;
	for(i = 0; i < sizeof(table_hl_first)/sizeof(table_hl_first[0]); i++)
	{
		char aline[80];
		if((unsigned)-1 == table_hl_first[i])
			continue;

		memset(aline, isalnum(i) ? i : '-', 65);
		aline[65] = '\0';
		fprintf(out, "/*\t       %s */\n", aline);
		print_table_ll_r(table_hl_first[i] * 2, 1);
		fflush(out);
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
		" * it under the terms of the GNU General Public License version\n"
		" * 2 as published by the Free Software Foundation.\n"
		" *\n"
		" * g2cd is distributed in the hope that it will be useful, but\n"
		" * WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		" * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
		" * GNU General Public License for more details.\n"
		" *\n"
		" * You should have received a copy of the GNU General Public\n"
		" * License along with g2cd; if not, write to the Free Software\n"
		" * Foundation, Inc., 59 Temple Place, Suite 330, Boston,\n"
		" * MA  02111-1307  USA\n"
		" *\n"
		" */\n"
		"\n"
		"/*\n"
		" * packet typer\n"
		" *\n"
		" */\n"
		"#define T_END_FLAG	(1 << 7)\n"
		"#define T_LAST_FLAG	(1 << 7)\n"
		"\n"
		"#define T_IS_END(x)	((x) & T_END_FLAG)\n"
		"#define T_IS_LAST(x)	((x) & T_LAST_FLAG)\n"
		"#define T_GET_CHAR(x)	((char)((x) & 0x7F))\n"
		"#define T_GET_TYPE(x)	((enum g2_ptype)((x) & 0x7F))\n"
		"#define T_GET_DELTA(x)	((unsigned char)((x) & 0x7F))\n"
		"\n"
		"#define T_LEND(x, y) \\\n"
		"	{ .c = (x)|T_LAST_FLAG, .u = { .t = y|T_END_FLAG }}\n"
		"#define T_LAST(x, y) \\\n"
		"	{ .c = (x)|T_LAST_FLAG, .u = { .t = y }}\n"
		"#define T_NEXT(x, y) \\\n"
		"	{ .c = (x), .u = { .d = y }}\n"
		"#define T_TEND(x, y) \\\n"
		"	{ .c = (x), .u = { .d = y|T_END_FLAG }}\n"
		"static const struct\n"
		"{\n"
		"	const unsigned char c;\n"
		"	union\n"
		"	{\n"
		"		const enum g2_ptype t;\n"
		"		const unsigned char d;\n"
		"	} u;\n"
		"} g2_ptype_state_table[] =\n"
		"{\n"
		"	/* Align entry points on even numbers */\n",
	f_name, now_tm->tm_year + 1900);
}

static void print_score(void)
{
	static const char *text[] =
	{
		"	/*         ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   ,   , */\n",
		"	/*      NUL,SOH,STX,ETX,EOT,ENQ,ACK,BEL, BS, HT, LF, VT, FF, CR, SO, SI, */\n",
		"	/*      DLE,DC1,DC2,DC3,DC4,NAK,SYN,ETB,CAN, EM,SUB,ESC, FS, GS, RS, US, */\n",
		"	/*      SPC,  !,  \",  #,  $,  %,  &,  '   (,  ),  *,  +,  ,,  -,  .,  /, */\n",
		"	/*        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  :,  ;,  <,  =,  >,  ?, */\n",
		"	/*        @,  A,  B,  C,  D,  E,  F,  G,  H,  I,  J,  K,  L,  M,  N,  O, */\n",
		"	/*        P,  Q,  R,  S,  T,  U,  V,  W,  X,  Y,  Z,  [,  \\,  ],  ^,  _, */\n",
		"	/*        `,  a,  b,  c,  d,  e,  f,  g,  h,  i,  j,  k,  l,  m,  n,  o, */\n",
		"	/*        p,  q,  r,  s,  t,  u,  v,  w,  x,  y,  z,  {,  |   },  ~,DEL, */\n",
	};
	const unsigned nelem = sizeof(table_hl_first) / sizeof(table_hl_first[0]);
	unsigned i, j;

	fprintf(out, "};\n"
			"\n"
			"/* index from above / 2 */\n"
			"static const unsigned char g2_ptype_dict_table[%u] =\n"
			"{\n"
			"	/*       00, 01, 02, 03, 04, 05, 06, 07, 08, 09, 0A, 0B, 0C, 0D, 0E, 0F, */\n",
		nelem);

	for(i = 0; i < (nelem >> 4); i++)
	{
		fprintf(out, "	/* %02X */", i << 4);
		for(j = 0; j < (1 << 4); j++)
			fprintf(out, "% 3i,", table_hl_first[(i << 4) + j]);
		putc('\n', out);
		fputs(text[i < 8 ? i + 1 : 0], out);
	}
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
