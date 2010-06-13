/*
 * config_parser.c
 * config file parser
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "config_parser.h"
#include "log_facility.h"
#include "../G2Connection.h"

#define STR_BUFF_SIZE 80
#define TF(x) ((x) ? 't' : 'f')

static struct ptoken *config_ptoken_alloc(void);
static struct ptoken *config_ptoken_new(struct str_buff *s, struct pctx *ctx);
static struct ptoken *config_ptoken_clone(struct ptoken *s);
static void config_ptoken_free(struct ptoken *);
static void config_ptoken_free_list(struct list_head *);
static bool config_ptoken_split(struct ptoken *to_split, size_t place);
static bool config_ptoken_split_precise(struct ptoken *to_split, size_t place);
static bool config_ptoken_concat(struct ptoken *, struct ptoken *);
static void config_ptoken_debug(struct list_head *);

static void config_comments_free(struct list_head *);
static void config_empty_free(struct list_head *);
static bool config_split_out_quote(struct list_head *);
static bool config_split_out_comment(struct list_head *);
static bool config_split_at_white(struct list_head *);
static bool config_split_at_equal(struct list_head *);
static struct ptoken *config_token_after_equal(struct list_head *);
static bool config_rejoin_quote(struct list_head *);
static bool config_copy_chain(struct list_head *, struct list_head *);
static size_t config_count_equal(struct list_head *);

static size_t config_trim_white(struct str_buff *s);
static size_t config_prune_white(struct str_buff *s);
static size_t config_skip_white(struct str_buff *s);

static int config_read_line(struct pctx *ctx);

bool config_parser_handle_int(struct list_head *head, void *data)
{
	struct ptoken *t, *first;
	int *target = data;
	char *endp;
	long long tmp;

	first = list_entry(head->next, struct ptoken, list);
	t = config_token_after_equal(head);
	if(!t) {
		logg(LOGF_NOTICE, "Parsing config file %s@%zu: Option \"%s\" wants a value assigned, will ignore\n",
		     first->ctx->in_filename, first->ctx->line_num, first->d.t);
		return true;
	}
	tmp = strtoll(t->d.t, &endp, 0);
	if(t->d.t == endp) {
		logg(LOGF_NOTICE, "Parsing config file %s@%zu: Option \"%s\" wants a numerical value, not \"%s\", will ignore\n",
		     first->ctx->in_filename, first->ctx->line_num, first->d.t, endp);
		return true;
	}
	if(*endp)
		logg(LOGF_INFO, "Parsing config file %s@%zu: Junk (\"%s\") after numerical value %lli, hopefully OK\n",
		     first->ctx->in_filename, first->ctx->line_num, endp, tmp);
	if(tmp < INT_MIN || tmp > INT_MAX) {
		logg(LOGF_NOTICE, "Parsing config file %s@%zu:  Value \"%s\" is to large for option \"%s\", will ignore\n",
		     first->ctx->in_filename, first->ctx->line_num, t->d.t, first->d.t);
		return true;
	}
	*target = (int)tmp;
	if(!list_is_last(&t->list, head))
		logg(LOGF_INFO, "Parsing config file %s@%zu: ignored tokens after \"%s\", hopefully OK\n",
		     first->ctx->in_filename, first->ctx->line_num, t->d.t);
	return true;
}

bool config_parser_handle_string(struct list_head *head, void *data)
{
	struct ptoken *t, *first;
	char **target = data;
	char *tmp;

	first = list_entry(head->next, struct ptoken, list);
	t = config_token_after_equal(head);
	if(!t) {
		logg(LOGF_NOTICE, "Parsing config file %s@%zu: Option \"%s\" wants a value assigned, will ignore\n",
		     first->ctx->in_filename, first->ctx->line_num, first->d.t);
		return true;
	}
	tmp = malloc(t->d.len + 1);
	if(!tmp) {
		logg(LOGF_NOTICE, "Parsing config file %s@%zu: Couldn't allocate memory for option \"%s\", will ignore\n",
		     first->ctx->in_filename, first->ctx->line_num, first->d.t);
		return true;
	}
	memcpy(tmp, t->d.t, t->d.len);
	tmp[t->d.len] = '\0';
	*target = tmp;
	if(!list_is_last(&t->list, head))
		logg(LOGF_INFO, "Parsing config file %s@%zu: ignored tokens after \"%s\", hopefully OK\n",
		     first->ctx->in_filename, first->ctx->line_num, t->d.t);
	return true;
}

bool config_parser_handle_bool(struct list_head *head, void *data)
{
	static const struct tr_vals
	{
		bool val;
		char txt[6];
	} tr_vals[] =
	{
		{true, "true"}, {false, "false"}, {true, "yes"}, {false, "no"},
		{true, "1"}, {false, "0"}, {true, "t"}, {false, "f"}, {true, "y"}, {false, "n"},
	};
	struct ptoken *t, *first;
	bool *target = data, found;
	unsigned i;

	first = list_entry(head->next, struct ptoken, list);
	t = config_token_after_equal(head);
	if(!t) {
		logg(LOGF_NOTICE, "Parsing config file %s@%zu: Option \"%s\" wants a value assigned, will ignore\n",
		     first->ctx->in_filename, first->ctx->line_num, first->d.t);
		return true;
	}
	for(i = 0, found = false; i < anum(tr_vals); i++)
	{
		if(0 == strcmp(tr_vals[i].txt, t->d.t)) {
			*target = tr_vals[i].val;
			found = true;
			break;
		}
	}
	if(!found) {
		logg(LOGF_NOTICE, "Parsing config file %s@%zu: I don't understand \"%s\"  for a boolean value, will ignore\n",
		     first->ctx->in_filename, first->ctx->line_num, t->d.t);
		return true;
	}
	if(!list_is_last(&t->list, head))
		logg(LOGF_INFO, "Parsing config file %s@%zu: ignored tokens after \"%s\", hopefully OK\n",
		     first->ctx->in_filename, first->ctx->line_num, t->d.t);
	return true;
}

bool config_parser_handle_guid(struct list_head *head, void *data)
{
	struct ptoken *t, *first;
	unsigned char *target = data;
	union
	{
		unsigned tmp_ints[16];
		struct
		{
			unsigned f[4];
			unsigned long long l;
		} s;
	} x;
	unsigned i, *tmp_id = x.tmp_ints;

	first = list_entry(head->next, struct ptoken, list);
	t = config_token_after_equal(head);
	if(!t) {
		logg(LOGF_NOTICE, "Parsing config file %s@%zu: Option \"%s\" wants a value assigned, will ignore\n",
		     first->ctx->in_filename, first->ctx->line_num, first->d.t);
		return true;
	}
	if(16 != sscanf(t->d.t,
	                "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
	                tmp_id, tmp_id+1, tmp_id+2, tmp_id+3, tmp_id+4, tmp_id+5,
	                tmp_id+6, tmp_id+7, tmp_id+8, tmp_id+9, tmp_id+10,
	                tmp_id+11, tmp_id+12, tmp_id+13, tmp_id+14, tmp_id+15)) {
		if(5 != sscanf(t->d.t,
		              "%08X-%04X-%04X-%04X-%012"
#ifndef WIN32
		                                       "llX",
#else
		                                       "I64X",
#endif
		              x.s.f, x.s.f+1, x.s.f+2, x.s.f+3, &x.s.l)) {
			logg(LOGF_NOTICE, "Parsing config file %s@%zu: \"%s\" does not seem to be a valid guid, will ignore\n",
			     first->ctx->in_filename, first->ctx->line_num, t->d.t);
			return true;
		}
		else
		{
			/*
			 * guids come from windows systems, their format
			 * has a plattform endian touch (first 3 fields),
			 * but that plattform is "always" intel...
			 */
// TODO: GUIDs and enianess?
			put_unaligned_le32(x.s.f[0], target);
			put_unaligned_le16(x.s.f[1], target + 4);
			put_unaligned_le16(x.s.f[2], target + 6);
			put_unaligned_be16(x.s.f[3], target + 8);
			put_unaligned_be16(x.s.l >> 32, target + 10);
			put_unaligned_be32(x.s.l, target + 12);
		}
	} else {
		for(i = 0; i < 16; i++)
			target[i] = (unsigned char)tmp_id[i];
	}
	if(!list_is_last(&t->list, head))
		logg(LOGF_INFO, "Parsing config file %s@%zu: ignored tokens after \"%s\", hopefully OK\n",
		     first->ctx->in_filename, first->ctx->line_num, t->d.t);
	return true;
}

bool config_parser_handle_encoding(struct list_head *head, void *data)
{
#define ENUM_CMD(x) {str_it(x)}
	static const char enc_vals[][11] =
	{
		G2_CONNECTION_ENCODINGS
	};
#undef ENUM_CMD
	struct ptoken *t, *first;
	enum g2_connection_encodings *target = data;
	bool found;
	unsigned i;

	first = list_entry(head->next, struct ptoken, list);
	t = config_token_after_equal(head);
	if(!t) {
		logg(LOGF_NOTICE, "Parsing config file %s@%zu: Option \"%s\" wants a value assigned, will ignore\n",
		     first->ctx->in_filename, first->ctx->line_num, first->d.t);
		return true;
	}
	for(i = 0, found = false; i < anum(enc_vals); i++)
	{
		if(0 == strcmp(enc_vals[i], t->d.t)) {
			*target = i;
			found = true;
			break;
		}
	}
	if(!found) {
		logg(LOGF_NOTICE, "Parsing config file %s@%zu: I don't understand \"%s\"  for an encoding value, will ignore\n",
		     first->ctx->in_filename, first->ctx->line_num, t->d.t);
		return true;
	}
	if(!list_is_last(&t->list, head))
		logg(LOGF_INFO, "Parsing config file %s@%zu: ignored tokens after \"%s\", hopefully OK\n",
		     first->ctx->in_filename, first->ctx->line_num, t->d.t);
	return true;
}

bool config_parser_read(const char *filename, const struct config_item cf[], size_t num_cf)
{
	struct pctx ctx;
	struct list_head last_line, head;
	size_t i;
	int line_ret;
	bool ret_val = false;

	memset(&ctx, 0, sizeof(ctx));
	if(!(ctx.in_file = fopen(filename, "r")))
		return false;
	ctx.in_filename = filename;

	if(!(ctx.line.t = malloc(STR_BUFF_SIZE)))
		goto out_no_mem;
	ctx.line.freeable = true;
	ctx.line_size = STR_BUFF_SIZE;

	INIT_LIST_HEAD(&last_line);
	INIT_LIST_HEAD(&head);
	for(; 0 < (line_ret = config_read_line(&ctx)); config_ptoken_free_list(&head))
	{
		struct ptoken *first;
		struct ptoken *last;
		struct str_buff w_buf = ctx.line;

		INIT_LIST_HEAD(&head);
		w_buf.freeable = false;
		if(!config_trim_white(&w_buf))
			continue;

		if(!(first = config_ptoken_new(&w_buf, &ctx)))
			break;

		list_add(&first->list, &head);
		if(!config_split_out_quote(&head))
			break;
		config_ptoken_debug(&head);

		if(!config_split_out_comment(&head))
			break;
		config_ptoken_debug(&head);

// TODO: removing comments good?
		config_comments_free(&head);
		config_ptoken_debug(&head);

		if(!config_split_at_white(&head))
			break;
		config_ptoken_debug(&head);

		config_empty_free(&head);
		config_ptoken_debug(&head);

		if(list_empty(&head))
			continue;

		last = list_entry(head.prev, struct ptoken, list);
		if(!last->is_quote && '\\' == last->d.t[last->d.len - 1]) {
			list_del(&last->list);
			if(!config_copy_chain(&last_line, &head))
				break;
			continue;
		}
		if(!list_empty(&last_line))
			list_splice_init(&last_line, &head);
		config_ptoken_debug(&head);

		if(!config_rejoin_quote(&head))
			break;
		config_ptoken_debug(&head);

		last = list_entry(head.prev, struct ptoken, list);
		/* whitespace should be removed and things split and categorized */
		if(last->is_quote && !last->is_quote_closed)
			logg(LOGF_NOTICE, "Parsing config file %s@%zu: unclosed quoted string - warning\n",
			     ctx.in_filename, ctx.line_num);

		if(!config_split_at_equal(&head))
			break;
		config_ptoken_debug(&head);
		if(config_count_equal(&head) > 1) {
			logg(LOGF_ERR, "Parsing config file %s@%zu: to many assignments - fatal error\n",
			     ctx.in_filename, ctx.line_num);
			break;
		}

		if(list_empty(&head))
			continue;

		first = list_entry(head.next, struct ptoken, list);
		for(i = 0; i < num_cf; i++)
		{
			if(cf[i].nlen != first->d.len)
				continue;
			if(strcmp(cf[i].name, first->d.t) != 0)
				continue;
			cf[i].handler(&head, cf[i].data);
			break;
		}
		if(i >= num_cf)
			logg(LOGF_NOTICE, "Parsing config file %s@%zu: Didn't unterstand \"%s\", will ignore\n",
			     ctx.in_filename, ctx.line_num, first->d.t);
	}

	if(0 == line_ret)
		ret_val = true;

	if(!list_empty(&last_line)) {
		logg(LOGF_ERR, "Parsing config file %s@%zu: line continuation on last line - fatal error\n",
		     ctx.in_filename, ctx.line_num);
		ret_val = false;
	}

	free(ctx.line.t);
out_no_mem:
	fclose(ctx.in_file);
	return ret_val;
}

/*****************************************************************************/

static void config_comments_free(struct list_head *head)
{
	struct ptoken *t, *n;

	list_for_each_entry_safe(t, n, head, list) {
		if(!t->is_comment)
			continue;
		list_del(&t->list);
		config_ptoken_free(t);
	}
}

static void config_empty_free(struct list_head *head)
{
	struct ptoken *t, *n;

	list_for_each_entry_safe(t, n, head, list) {
		if(t->d.len)
			continue;
		list_del(&t->list);
		config_ptoken_free(t);
	}
}

static bool config_split_out_quote(struct list_head *head)
{
	struct ptoken *t;
	char quote_char = '"';
	bool in_quote = false;

	list_for_each_entry(t, head, list)
	{
		char *quote_ptr;

		if(!in_quote)
		{
			quote_ptr = strpbrk(t->d.t, "\"'");
			if(!quote_ptr)
				continue;
			quote_char = *quote_ptr;
			in_quote = true;
			/* when not first char */
			if(quote_ptr != t->d.t) { /* split out starting chars  */
				if(!config_ptoken_split(t, quote_ptr + 1 - t->d.t))
					return false;
				config_prune_white(&t->d);
				continue;
			}
			t->d.t++, t->d.len--;
		}
		t->quote_char = quote_char;
		t->is_quote = true;
		quote_ptr = strchr(t->d.t, quote_char);
		if(!quote_ptr) { /* corner case, no matching quote */
			t->is_quote_closed = false;
			break;
		}
		t->is_quote_closed = true;
		in_quote = false;

		/* if still chars after quote */
		if(t->d.t + t->d.len != quote_ptr + 1) { /* split them out */
			if(!config_ptoken_split(t, quote_ptr + 1 - t->d.t))
				return false;
			config_skip_white(&t->d);
		} else /* else yank */
			*(t->d.t + --t->d.len) = '\0';
	}
	return true;
}

static bool config_split_out_comment(struct list_head *head)
{
	bool in_line_comment = false;
	struct ptoken *t;

	list_for_each_entry(t, head, list)
	{
		char *comm_ptr;

		if(in_line_comment) {
			t->is_comment = true;
			continue;
		}
		if(t->is_quote)
			continue;

		comm_ptr = strchr(t->d.t, '#');
		if(!comm_ptr)
			continue;
		if(t->d.t != comm_ptr) {
			if(!config_ptoken_split(t, comm_ptr + 1 - t->d.t))
				return false;
			config_prune_white(&t->d);
		} else {
			t->d.t++, t->d.len--;
			t->is_comment = true;
		}
		in_line_comment = true;
	}
	return true;
}

static bool config_split_at_white(struct list_head *head)
{
	struct ptoken *t;

	list_for_each_entry(t, head, list)
	{
		char *w_ptr;
		size_t len;

		if(t->is_comment || t->is_quote)
			continue;

		config_skip_white(&t->d);
		w_ptr = t->d.t;
		len   = t->d.len;

		while(len && !isspace(*w_ptr))
			len--, w_ptr++;

		if(!len)
			continue;

		if(!config_ptoken_split(t, w_ptr + 1 - t->d.t))
			return false;
	}
	return true;
}

static bool config_split_at_equal(struct list_head *head)
{
	struct ptoken *t;

	list_for_each_entry(t, head, list)
	{
		char *w_ptr;
		if(t->is_comment || t->is_quote || t->d.len < 2)
			continue;

		w_ptr = strchr(t->d.t, '=');
		if(!w_ptr)
			continue;

		if((ssize_t)t->d.len > w_ptr + 1 - t->d.t) {
			if(!config_ptoken_split_precise(t, w_ptr + 1 - t->d.t))
				return false;
		}
		if(!config_ptoken_split_precise(t, w_ptr - t->d.t))
			return false;

		config_prune_white(&t->d);
	}
	return true;
}

static struct ptoken *config_token_after_equal(struct list_head *head)
{
	struct ptoken *first, *second;

	first = list_entry(head->next, struct ptoken, list);
	if(list_is_last(&first->list, head))
		return NULL;

	second = list_entry(first->list.next, struct ptoken, list);
	if(second->is_comment || second->is_quote)
		return NULL;

	if('=' != *second->d.t)
		return NULL;

	if(list_is_last(&second->list, head))
		return NULL;
	return list_entry(second->list.next, struct ptoken, list);
}

static bool config_rejoin_quote(struct list_head *head)
{
	struct ptoken *t, *n, *last = NULL;

	list_for_each_entry_safe(t, n, head, list)
	{
		if(!last)
			goto next_round;

		if(!(t->is_quote && last->is_quote))
			goto next_round;

		if(t->quote_char != last->quote_char)
			goto next_round;

		if(!config_ptoken_concat(last, t))
			return false;
		last->is_quote_closed |= t->is_quote_closed;
		list_del(&t->list);
		config_ptoken_free(t);
next_round:
		last = t;
	}
	return true;
}

static bool config_copy_chain(struct list_head *dst, struct list_head *src)
{
	struct ptoken *t;

	list_for_each_entry(t, src, list) {
		struct ptoken *a = config_ptoken_clone(t);
		if(!a)
			return false;
		list_add_tail(&a->list, dst);
	}
	return true;
}

static size_t config_count_equal(struct list_head *head)
{
	struct ptoken *t;
	size_t ret_val = 0;

	list_for_each_entry(t, head, list) {
		if('=' == *t->d.t)
			ret_val++;
	}

	return ret_val;
}

/*****************************************************************************/

static struct ptoken *config_ptoken_alloc(void)
{
	struct ptoken *ret_val = malloc(sizeof(*ret_val));

	if(ret_val) {
		memset(ret_val, 0, sizeof(*ret_val));
		INIT_LIST_HEAD(&ret_val->list);
		INIT_LIST_HEAD(&ret_val->args);
	}
	return ret_val;
}

static struct ptoken *config_ptoken_new(struct str_buff *s, struct pctx *ctx)
{
	struct ptoken *ret_val = config_ptoken_alloc();

	if(ret_val) {
		ret_val->d = *s;
		ret_val->d.freeable = false;
		ret_val->ctx = ctx;
	}
	return ret_val;
}

static struct ptoken *config_ptoken_clone(struct ptoken *s)
{
	struct ptoken *ret_val = malloc(sizeof(*ret_val));

	if(!ret_val)
		return ret_val;

	*ret_val = *s;
	INIT_LIST_HEAD(&ret_val->list);
	INIT_LIST_HEAD(&ret_val->args);
	ret_val->d.t = malloc(ret_val->d.len + 1);
	if(!ret_val->d.t) {
		free(ret_val);
		return NULL;
	}
	memcpy(ret_val->d.t, s->d.t, ret_val->d.len);
	ret_val->d.t[ret_val->d.len] = '\0';
	ret_val->d.freeable = true;
	return ret_val;
}

static void config_ptoken_free(struct ptoken *t)
{
	if(t->d.freeable)
		free(t->d.t);
	free(t);
}

static void config_ptoken_free_list(struct list_head *head)
{
	struct ptoken *pos, *n;

	list_for_each_entry_safe(pos, n, head, list)
	{
		struct ptoken *sos, *m;

		list_for_each_entry_safe(sos, m, &pos->args, list) {
			list_del(&sos->list);
			config_ptoken_free(sos);
		}
		list_del(&pos->list);
		config_ptoken_free(pos);
	}
}

static bool config_ptoken_split(struct ptoken *to_split, size_t place)
{
	struct str_buff s;
	struct ptoken *splitter;

	s.t   = to_split->d.t + place;
	s.len = to_split->d.len - place;
	s.freeable = false;

	splitter = config_ptoken_new(&s, to_split->ctx);
	if(!splitter)
		return false;

	to_split->d.t[place - 1] = '\0';
	to_split->d.len = place - 1;
	list_add(&splitter->list, &to_split->list);
	return true;
}

static bool config_ptoken_split_precise(struct ptoken *to_split, size_t place)
{
	struct str_buff s;
	struct ptoken *splitter;

	s.t = malloc((to_split->d.len - place) + 1);
	if(!s.t)
		return false;

	memcpy(s.t, to_split->d.t + place, to_split->d.len - place);
	s.len = to_split->d.len - place;
	s.freeable = true;
	s.t[s.len] = '\0';

	splitter = config_ptoken_new(&s, to_split->ctx);
	if(!splitter)
		return false;

	to_split->d.t[place] = '\0';
	to_split->d.len = place;
	list_add(&splitter->list, &to_split->list);
	return true;
}

static bool config_ptoken_concat(struct ptoken *a, struct ptoken *b)
{
	char *c;

	c = malloc(a->d.len + b->d.len + 1);
	if(!c)
		return false;

	memcpy(mempcpy(c, a->d.t, a->d.len), b->d.t, b->d.len);
	if(a->d.freeable)
		free(a->d.t);

	a->d.t = c;
	a->d.len += b->d.len;
	a->d.freeable = true;
	c[a->d.len] = '\0';
	return true;
}

static void config_ptoken_debug(struct list_head *head GCC_ATTR_UNUSED_PARAM)
{
#ifdef DEBUG_CONFIG_PARSER
	struct ptoken *t;
	logg(LOGF_DEBUG, "       -------------------------- token ----------------------------");
	list_for_each_entry(t, head, list) {
		logg(LOGF_DEBUG, "%4zu\tlen: %u,%u\tisq: %c\tisqc: %c\tisc: %c\ttoken: \"%s\"\n",
		     t->ctx->line_num, t->d.len, strlen(t->d.t),
		     TF(t->is_quote), TF(t->is_quote_closed), TF(t->is_comment), t->d.t);
	}
#endif
}

/*****************************************************************************/

static int config_read_line(struct pctx *ctx)
{
	char *read_line;
	int ret_val = 1;

	ctx->line.len = 0;
	while(true)
	{
		read_line = fgets(ctx->line.t + ctx->line.len,
		                  ctx->line_size - ctx->line.len, ctx->in_file);
		if(!read_line)
		{
			if(feof(ctx->in_file))
				ret_val = 0;
			if(ferror(ctx->in_file))
				ret_val = -1;
			break;
		}

		ctx->line.len += strlen(read_line);
		if('\n' == *(ctx->line.t + ctx->line.len - 1)) {
			*(ctx->line.t + --ctx->line.len) = '\0';
			break;
		}
		else
		{
			char *tmp_line = realloc(ctx->line.t, ctx->line_size * 2);
			if(!tmp_line) {
				ret_val = -1;
				break;
			}

			ctx->line.t = tmp_line;
			ctx->line_size *= 2;
		}
	}

	ctx->line_num++;
	return ret_val;
}

/*
 * Warning, all work descructive on s->t
 */
static size_t config_prune_white(struct str_buff *s)
{
	/* remove trailing whitespace */
	while(s->len && isspace(*(s->t + s->len - 1)))
		s->len--;

	*(s->t + s->len) = '\0';

	return s->len;
}

static size_t config_skip_white(struct str_buff *s)
{
	/* sanity check */
	if(s->freeable && s->len && isspace(*s->t)) {
		logg_develd("modified freeable pointer %p -> will try to leak", s->t);
		s->freeable = false;
	}

	/* skip whitespace at start */
	while(s->len && isspace(*s->t))
		s->t++, s->len--;

	/* no compacting, we modified s->t */
	return s->len;
}

static size_t config_trim_white(struct str_buff *s)
{
	/* sanity check */
	if(s->freeable && s->len && isspace(*s->t)) {
		logg_develd("modified freeable pointer %p -> will try to leak", s->t);
		s->freeable = false;
	}

	/* skip whitespace at start */
	while(s->len && isspace(*s->t))
		s->t++, s->len--;

	/* remove trailing whitespace */
	while(s->len && isspace(*(s->t + s->len - 1)))
		s->len--;

	/* no compacting, we modified s->t */
	*(s->t + s->len) = '\0';

	return s->len;
}

static char const rcsid_cfgp[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
