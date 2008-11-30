/*
 * hzp.c
 * Header for the hzp interface.
 *
 * Copyright (c) 2006 Jan Seiffert
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

#ifndef LIB_HZP_H
# define LIB_HZP_H

# include <stdbool.h>
# include "other.h"
# include "atomic.h"

# define LIB_HZP_EXTRN(x) x GCC_ATTR_VIS("hidden")

enum hzps
{
	HZP_EPOLL,
	/* keep this the last entry!! */
	HZP_MAX
};

struct hzp_free
{
	atomicst_t st;
	void *data;
	void (*free_func)(void *);
};

LIB_HZP_EXTRN(bool hzp_alloc(void));
LIB_HZP_EXTRN(void hzp_ref(enum hzps, void *));
LIB_HZP_EXTRN(void hzp_unref(enum hzps));
LIB_HZP_EXTRN(void hzp_deferfree(struct hzp_free *, void *, void (*)(void *)) GCC_ATTR_FASTCALL);
LIB_HZP_EXTRN(int hzp_scan(int));
	
#endif
