/*
 * config_parser.h
 * header-file for the config file parser
 *
 * Copyright (c) 2009-2010 Jan Seiffert
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
#ifndef LIB_CONFIG_PARSER_H
# define LIB_CONFIG_PARSER_H

# include <stdio.h>
# include <stdbool.h>
# include "other.h"
# include "list.h"

/* half private, for data access */
struct str_buff
{
	char *t;
	size_t len;
	bool freeable;
};

/* private */
struct pctx
{
	FILE *in_file;
	const char *in_filename;
	struct str_buff line;
	size_t line_size;
	size_t line_num;
};

/* public */
struct ptoken
{
	struct list_head list;
	struct list_head args;
	struct pctx *ctx;
	bool is_quote;
	bool is_quote_closed;
	bool is_comment;
	char quote_char;
	struct str_buff d;
};

struct config_item
{
	const char *name;
	size_t nlen;
	void *data;
	bool (*handler)(struct list_head *, void *);
};

#define CONF_ITEM(str, dttt, hddd) \
{.name=(str), .nlen=(str_size(str)), .data=(void *)(intptr_t)(dttt), .handler=(hddd)}

# define LIB_CONFIG_PARSER_EXTRN(x) x GCC_ATTR_VIS("hidden")
LIB_CONFIG_PARSER_EXTRN(bool config_parser_read(const char *filename, const struct config_item cf[], size_t num_cf));
LIB_CONFIG_PARSER_EXTRN(bool config_parser_handle_int(struct list_head *, void *));
LIB_CONFIG_PARSER_EXTRN(bool config_parser_handle_string(struct list_head *, void *));
#endif /* LIB_CONFIG_PARSER_H */
