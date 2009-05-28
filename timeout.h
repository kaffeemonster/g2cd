/*
 * timeout.h
 * header-file for timeout.c
 *
 * Copyright (c) 2004-2009 Jan Seiffert
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
 * $Id: timeout.h,v 1.7 2005/11/05 09:57:43 redbully Exp redbully $
 */

#ifndef _TIMEOUT_H
# define _TIMEOUT_H

# include <sys/time.h>
# include <pthread.h>
# include "lib/rbtree.h"

struct timeout
{
	struct rb_node rb;
	struct timespec t;
	pthread_mutex_t lock;
	void *data;
	void (*fun)(void *);
};

# ifndef _TIMEOUT_C
#  define TOUT_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define TOUT_EXTRN(x) x GCC_ATTR_VIS("hidden")
# endif

static inline void INIT_TIMEOUT(struct timeout *t)
{
	RB_CLEAR_NODE(&t->rb);
	pthread_mutex_init(&t->lock, NULL);
}

static inline void DESTROY_TIMEOUT(struct timeout *t)
{
	pthread_mutex_destroy(&t->lock);
}

TOUT_EXTRN(bool timeout_add(struct timeout *, unsigned int));
TOUT_EXTRN(bool timeout_advance(struct timeout *, unsigned int));
TOUT_EXTRN(bool timeout_cancel(struct timeout *));
TOUT_EXTRN(void timeout_timer_task_abort(void));
TOUT_EXTRN(void *timeout_timer_task(void *));

#endif /* _TIMEOUT_H */
/* EOF */
