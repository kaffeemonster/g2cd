/* 
 * calltree.c
 * generate gallgraphs in graphviz format from gcc RTL dumps
 *
 * Copyright (c) 2005, Jan Seiffert
 * 
 * a little bit limited, but does its job...
 *
 * Idea taken from the perl-script egypt (http://www.gson.org/egypt/)
 * wich states:
 * 
 * Copyright 1994-2004 Andreas Gustafsson
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the same terms as Perl itself.
 * 
 * $Id: calltree.c,v 1.5 2005/11/04 19:42:58 redbully Exp redbully $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define str_size(x)	(sizeof((x)) - 1)
#define DEF_FUNC	";; Function "
#define DEF_CALL	"call "
#define DEF_SYM_REF	"symbol_ref"

#define REF_DIRECT	0x01
#define REF_INDIRECT	0x02

struct function
{
	struct function *next, *next_ht;
	union
	{
		struct function *calls;
		int reftype;
	} d;
	char *filename;
	char name[];
};

static void usage(char *);
static ssize_t my_getline(char **, size_t *, FILE *);
static void add_func(char *, char *);
static void add_symbol(char *, int);

static struct function *current_func;

struct my_ht
{
	size_t num_entries, max_entries;
	struct function **entries;
};

static void ht_init(struct my_ht *, size_t);
static void ht_add(struct my_ht *, const char *, struct function *);
static struct function *ht_get(struct my_ht *, const char *);
static struct function *ht_get_list_all(struct my_ht *);
static int ht_remove(struct my_ht *, const char *);
static int ht_contains_key(struct my_ht *, const char *);
static void ht_grow(struct my_ht *);
static int hash_string(const char *);
static struct function *sort_lst(struct function *);
/* static void	print_ht(struct my_ht *); */
static void	print_lst(struct function *, int);

static struct my_ht all_func_ht;

int main(int argc, char *argv[])
{
	FILE *infile = NULL, *outfile = stdout;
	char *line = NULL;
	struct function *omit_func = NULL, *w_func, *ow_func;
	size_t length = 0;
	ssize_t rlength = 0; 
	int i, concentrate = 0;

	if(2 > argc)
	{
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	ht_init(&all_func_ht, 128);
	
	for(i = 1; i < argc; i++)
	{
		size_t line_num = 0;

		if('-' == argv[i][0])
		{
			if('f' == argv[i][1] && (i + 1) < argc)
			{
				/* outputfile */
				if(!(outfile = fopen(argv[++i], "w")))
				{
					fprintf(stderr, "opening outfile \"%s\": %s\n", argv[i], strerror(errno));
					return EXIT_FAILURE;
				}
				continue;
			}
			else if('o' == argv[i][1] && (i + 1) < argc)
			{
				/* omitlist */
				struct function *tmp_func;
				char *s_str = argv[++i], *n_str;
				while(*s_str)
				{
					n_str = strchr(s_str, ',');
					if(!n_str)
						n_str = s_str + strlen(s_str);
										
					*n_str = '\0';
					if(!(tmp_func = malloc(sizeof(*omit_func) + strlen(s_str) + 1)))
					{
						perror("allocating memory");
						return EXIT_FAILURE;
					}
					strcpy(tmp_func->name, s_str);
					tmp_func->next = omit_func;
					omit_func = tmp_func;
					s_str = n_str + 1;
				}
				continue;
			}
			else if('c' == argv[i][1])
			{
				concentrate++;
				continue;
			}
			else if('\0' == argv[i][1]) /* use stdin */
			{
				infile = stdin;
				argv[i][0] = '\0';
			}
		}
		
		if(!infile && !(infile = fopen(argv[i], "r")))
		{
			fprintf(stderr, "opening infile \"%s\": %s\n", argv[i], strerror(errno));
			continue;
		}

		if(strrchr(argv[i], '.'))
			*strrchr(argv[i], '.') = '\0';
		
		while(-1 != (rlength = my_getline(&line, &length, infile)))
		{
			char *s_str;
			line_num++;
			if((size_t) rlength <= str_size(DEF_CALL))
				continue;

			if(DEF_FUNC[0] == *line && (s_str = strstr(line, DEF_FUNC)))
			{
				s_str += str_size(DEF_FUNC);
				add_func(s_str, argv[i]);
			}
			else
			{
				size_t rej_len = strcspn(line, "cs");
				if(rej_len < (rlength - str_size(DEF_CALL)))
				{
					if(DEF_CALL[0] == *(line + rej_len) &&
						DEF_CALL[1] == *(line + rej_len + 1) &&
						(s_str = strstr((line + rej_len), DEF_CALL)))
					{
						s_str += str_size(DEF_CALL);
						add_symbol(s_str, REF_DIRECT);
					}
					else if(DEF_SYM_REF[0] == *(line + rej_len) &&
						DEF_SYM_REF[1] == *(line + rej_len + 1) &&
						(s_str = strstr((line + rej_len), DEF_SYM_REF)))
					{
						s_str += str_size(DEF_SYM_REF);
						add_symbol(s_str, REF_INDIRECT);
					}
				}
			}
		}
		if(ferror(infile))
		{
			fprintf(stderr, "reading infile \"%s\": %s\n", argv[i], strerror(errno));
			fclose(infile);
			return EXIT_FAILURE;
		}
		
		fclose(infile);
		infile = NULL; 
	}

	for(w_func = omit_func; w_func; w_func = w_func->next)
		ht_remove(&all_func_ht, w_func->name);

/*	print_lst(ht_get_list_all(&all_func_ht), 1); */
	
	fputs("digraph G {\n", outfile);
	if(concentrate)
		fputs("\tconcentrate = true\n", outfile);
	for(ow_func = w_func = ht_get_list_all(&all_func_ht); w_func; w_func = w_func->next)
	{
		struct function *tmp_func;
		for(tmp_func = w_func->d.calls; tmp_func; tmp_func = tmp_func->next)
		{
			if(ht_contains_key(&all_func_ht, tmp_func->name))
			{
				if(tmp_func->d.reftype & REF_DIRECT)
					fprintf(outfile, "\t%s -> %s [style=solid]\n", w_func->name, tmp_func->name);
				if(tmp_func->d.reftype & REF_INDIRECT)
					fprintf(outfile, "\t%s -> %s [style=dotted]\n", w_func->name, tmp_func->name);
			}
		}
	}
	w_func = ow_func;
	
/*	for(; w_func; w_func = w_func->next)
	{
		struct function *tmp_func;
		for(tmp_func = w_func->d.calls; tmp_func; tmp_func = tmp_func->next)
		{
			if(ht_contains_key(&all_func_ht, tmp_func->name))
			{
				if(tmp_func->d.reftype & REF_DIRECT)
					fprintf(outfile, "\t%s -> %s [style=solid]\n", w_func->name, tmp_func->name);
				if(tmp_func->d.reftype & REF_INDIRECT)
					fprintf(outfile, "\t%s -> %s [style=dotted]\n", w_func->name, tmp_func->name);
			}
		}
	}*/
	fputs("}\n", outfile);
	print_lst(w_func, 0);
	w_func = sort_lst(w_func);
	print_lst(w_func, 0);
//	print_ht(&all_func_ht);
	fclose(outfile);

	return EXIT_SUCCESS;
}

/* since getline is a GNU-Extension... */
static ssize_t my_getline(char **line, size_t *length, FILE *rfile)
{
	char *tmp_s;
	ssize_t ret_val = 0;

	if(!line || !length)
	{
		errno = EINVAL;
		return -1;
	}

	if(!*line)
	{
		*length = *length ? *length : 80;
		if(!(*line = malloc(*length)))
			return -1;
	}

	tmp_s = *line;
	
	while(1)
	{
		size_t tmp_r;
		if(!(tmp_s = fgets(tmp_s, *length - ret_val, rfile)))
			return -1;
	
		tmp_s += (tmp_r = strlen(tmp_s));
		ret_val = (ssize_t)((size_t) ret_val + tmp_r);

		if('\n' == *(tmp_s - 1))
			break;
		else
		{
			char *tmp_ptr;
			if(feof(rfile))
				break;
			if(ferror(rfile))
				return -1;

			if(!(tmp_ptr = realloc(*line, *length * 2)))
				return -1;
			*line = tmp_ptr;
			*length *= 2;
			tmp_s =  tmp_ptr + ret_val;
		}
	}

	return ret_val;
}

static void add_func(char *s_str, char *filename)
{
	struct function *tmp_func;
	if(strchr(s_str, '\n'))
		*strchr(s_str, '\n') = '\0';

	if((tmp_func = ht_get(&all_func_ht, s_str)))
	{
		current_func = tmp_func;
		return;
	}
			
	if(!(current_func = malloc(sizeof(*current_func) + strlen(s_str) + 1)))
	{
		perror("allocating memory");
		exit(EXIT_FAILURE);
	}
	strcpy(current_func->name, s_str);
	current_func->d.calls = NULL;
	current_func->next = NULL;
	current_func->next_ht = NULL;
	current_func->filename = filename;

	ht_add(&all_func_ht, current_func->name, current_func);
}

static void add_symbol(char *s_str, int reftype)
{
	struct function *tmp_func;
	if(!(s_str = strchr(s_str, '"')))
		return;

	*strchr(++s_str, '"') = '\0';
	for(tmp_func = current_func->d.calls; tmp_func; tmp_func = tmp_func->next)
	{
		if(!strcmp(s_str, tmp_func->name))
		{
			tmp_func->d.reftype |= reftype;
			return;
		}
	}

	if(!(tmp_func = malloc(sizeof(*tmp_func) + strlen(s_str) + 1)))
	{
		perror("allocating memory");
		exit(EXIT_FAILURE);
	}
	strcpy(tmp_func->name, s_str);
	tmp_func->d.reftype = reftype;
	tmp_func->next = current_func->d.calls;
	tmp_func->next_ht = NULL;
	current_func->d.calls = tmp_func;
}

static void usage(char *name)
{
	fputs("generate callgraph in graphviz format from gcc RTL dumps\n", stderr);
	fprintf(stderr, "usage: %s [-o foo[,bar]] [-f outfile] [-c] rtl-dump-files...\n", name);
	fputs("\t-c set the \"concentrate\"-option for the graph\n", stderr);
	fputs("\t-o omit-list\tcomma separtet list of function to omit in output\n", stderr);
	fputs("\t-f outfile\twrite output to file \"outfile\"\n", stderr);
	fputs("\trtl-dump-files\tRTL-dumps generated by gcc \"-dr\"-option, \"-\" should read stdin\n", stderr);
}

static void ht_init(struct my_ht *the_ht, size_t size)
{
	if(!(the_ht->entries = calloc(size, sizeof(*the_ht->entries))))
	{
		perror("allocating memory");
		exit(EXIT_FAILURE);
	}
	the_ht->max_entries = size;
	the_ht->num_entries = 0;
}

static void ht_add(struct my_ht *the_ht, const char *key, struct function *val)
{
	int hash = hash_string(key) % the_ht->max_entries;

	if(!the_ht->entries[hash])
		the_ht->entries[hash] = val;
	else
	{
		val->next_ht = the_ht->entries[hash];
		the_ht->entries[hash]= val; 
	}

	the_ht->num_entries++;
	if(the_ht->num_entries >= the_ht->max_entries)
		ht_grow(the_ht);
}

static struct function *ht_get(struct my_ht *the_ht, const char *key)
{
	int hash = hash_string(key) % the_ht->max_entries;
	struct function *tmp_func = the_ht->entries[hash];

	for( ; tmp_func; tmp_func = tmp_func->next_ht)
	{
		if(!strcmp(tmp_func->name, key))
			return tmp_func;
	}

	return NULL;
}

static struct function *ht_get_list_all(struct my_ht *the_ht)
{
	size_t i;
	struct function *ret_val= NULL, *last_val = NULL; 

	for(i = 0; i < the_ht->max_entries; i++)
	{
		if(the_ht->entries[i])
		{
			ret_val = last_val = the_ht->entries[i];
			for(; last_val->next_ht; last_val = last_val->next_ht)
				last_val->next = last_val->next_ht;
			i++;
			break;
		}
	}

	for(; i < the_ht->max_entries; i++)
	{
		if(the_ht->entries[i])
		{
			last_val->next = the_ht->entries[i];
			last_val = last_val->next;
			for(; last_val->next_ht; last_val = last_val->next_ht)
				last_val->next = last_val->next_ht;
		}
	}

	if(last_val)
		last_val->next = NULL; 
	
	return ret_val;
}

static int ht_remove(struct my_ht *the_ht, const char *key)
{
	int hash = hash_string(key) % the_ht->max_entries;
	struct function *tmp_func;
	int ret_val = 0; 
	
	while(the_ht->entries[hash] && !strcmp(the_ht->entries[hash]->name, key))
	{
		tmp_func = the_ht->entries[hash];
		the_ht->entries[hash] = the_ht->entries[hash]->next_ht;
		free(tmp_func);
	}

	tmp_func = the_ht->entries[hash];
	while(tmp_func && tmp_func->next_ht)
	{	
		if(!strcmp(tmp_func->next_ht->name, key))
		{
			struct function *tmp_func2 = tmp_func->next_ht;
			tmp_func->next_ht = tmp_func2->next_ht;
			free(tmp_func2);
			ret_val = 1; 
		}
		else
			tmp_func = tmp_func->next_ht;
	}

	return ret_val;
}

static int ht_contains_key(struct my_ht *the_ht, const char *key)
{
	int hash = hash_string(key) % the_ht->max_entries;
	struct function *tmp_func = the_ht->entries[hash];

	for(; tmp_func; tmp_func = tmp_func->next_ht)
	{
		if(!strcmp(tmp_func->name, key))
			return 1;
	}

	return 0;
}

static void ht_grow(struct my_ht *the_ht)
{
	struct my_ht old_ht = *the_ht;
	size_t i;

	ht_init(the_ht, old_ht.max_entries * 2);

	for(i = 0; i < old_ht.max_entries; i++)
	{
		struct function *tmp_func;
		tmp_func = old_ht.entries[i];
		while(tmp_func)
		{
			struct function *tmp_next = tmp_func->next_ht;
			tmp_func->next_ht = NULL;
			ht_add(the_ht, tmp_func->name, tmp_func);
			tmp_func = tmp_next;
		}
	}

	free(old_ht.entries);
}

#define mix(a,b,c) \
{ \
	a -= b;	a -= c;  a ^= (c>>13); \
	b -= c;	b -= a;  b ^= (a<<8);  \
	c -= a;	c -= b;  c ^= (b>>13); \
	a -= b;	a -= c;  a ^= (c>>12); \
	b -= c;	b -= a;  b ^= (a<<16); \
	c -= a;	c -= b;  c ^= (b>>5);  \
	a -= b;	a -= c;  a ^= (c>>3);  \
	b -= c;	b -= a;  b ^= (a<<10); \
	c -= a;	c -= b;  c ^= (b>>15); \
}

static int hash_string(const char *k)
{
	size_t length = strlen(k);
	size_t len = length; 
	int a,b,c;

	a = b = 0x9e3779b9;
	c = 0; /* normaly the continuation-value */

	for(; len >= 12; k += 12, len -= 12)
	{
		a = a + (k[0] + ((int)k[1]<<8) + ((int)k[2]<<16) + ((int)k[3]<<24));
		b = b + (k[4] + ((int)k[5]<<8) + ((int)k[6]<<16) + ((int)k[7]<<24));
		c = c + (k[8] + ((int)k[9]<<8) + ((int)k[10]<<16) + ((int)k[11]<<24));
		mix(a,b,c);
	}
	c += length;

	switch(len)
   {
	case 11: c += ((int)k[10]<<24);
	case 10: c += ((int)k[9]<<16);
	case 9 : c += ((int)k[8]<<8);
	/* the first byte of c is reserved for the length */
	case 8 : b += ((int)k[7]<<24);
	case 7 : b += ((int)k[6]<<16);
	case 6 : b += ((int)k[5]<<8);
	case 5 : b += k[4];
	case 4 : a += ((int)k[3]<<24);
	case 3 : a += ((int)k[2]<<16);
	case 2 : a += ((int)k[1]<<8);
	case 1 : a += k[0];
	}
	mix(a,b,c);
	return c;
}

static int filename_cmp(const void *f1, const void *f2)
{
	int ret_val = strcmp((*(struct function *const*)f1)->filename, (*(struct function *const*)f2)->filename);
	if(!ret_val)
		ret_val = strcmp((*(struct function *const*)f1)->name, (*(struct function *const*)f2)->name);

	return ret_val;
}

static struct function *sort_lst(struct function *the_lst)
{
	struct function *w_func, *r_func, **table_func;
	size_t num_elem = 0, i;

	for(w_func = the_lst; w_func; w_func = w_func->next)
		num_elem++;

	if(!num_elem)
		return the_lst;
	
	if(!(table_func = malloc(num_elem * sizeof(w_func))))
	{
		perror("allocating sort mem");
		exit(EXIT_FAILURE);
	}
	
	for(i = 0, w_func = the_lst; i < num_elem; i++, w_func = w_func->next)
		table_func[i] = w_func;

	qsort(table_func, num_elem, sizeof(*table_func), filename_cmp);

	for(r_func = w_func = table_func[0], i = 1; i < num_elem; i++)
	{
		w_func->next = table_func[i];
		w_func = w_func->next;
	}

	w_func->next = NULL;
	free(table_func);
	
	return r_func;
}

/*static void	print_ht(struct my_ht *the_ht)
{
	size_t i;


	for(i = 0; i < the_ht->max_entries; i++)
	{
		printf("[%.6i]\n\t|->%p (\"%s\")\n", i, (void *) the_ht->entries[i], (the_ht->entries[i]) ? the_ht->entries[i]->name : "");
		if(the_ht->entries[i])
		{
			struct function *tmp_func;
			for(tmp_func = the_ht->entries[i]->next_ht; tmp_func; tmp_func = tmp_func->next_ht)
				printf("\t|->%p (\"%s\")\n", (void *) tmp_func, tmp_func->name);
		}
	}
	printf("max: %u\tnum: %u\n", the_ht->max_entries, the_ht->num_entries);
}*/

static void	print_lst(struct function *the_lst, int all)
{
	for(; the_lst; the_lst = the_lst->next)
	{
		struct function *tmp_func;
		printf("%s::%s\n", the_lst->filename, the_lst->name);
		if(all)
		{
			for(tmp_func = the_lst->d.calls; tmp_func; tmp_func = tmp_func->next)
				printf("\t|-> %s\n", tmp_func->name);
		}
	}
}
