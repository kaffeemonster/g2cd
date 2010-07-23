/*
 * timeout.h
 * header-file for timeout.c
 *
 * Copyright (c) 2004-2010 Jan Seiffert
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
 * $Id: timeout.h,v 1.7 2005/11/05 09:57:43 redbully Exp redbully $
 */

#ifndef _TIMEOUT_H
# define _TIMEOUT_H

# include <sys/time.h>
# include "lib/my_pthread.h"
# include "lib/rbtree.h"

struct timeout
{
	struct rb_node rb;
	struct timespec t;
	pthread_mutex_t lock;
	void *data;
	int (*fun)(void *);
	int rearm_in_progress;
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
	t->rearm_in_progress = 0;
}

static inline void DESTROY_TIMEOUT(struct timeout *t GCC_ATTR_UNUSED_PARAM)
{
//	pthread_mutex_destroy(&t->lock);
}

TOUT_EXTRN(bool timeout_add(struct timeout *, unsigned int));
TOUT_EXTRN(bool timeout_advance(struct timeout *, unsigned int));
TOUT_EXTRN(bool timeout_cancel(struct timeout *));
TOUT_EXTRN(void timeout_timer_task_abort(void));
TOUT_EXTRN(void *timeout_timer_task(void *));

#endif /* _TIMEOUT_H */
/* EOF */
