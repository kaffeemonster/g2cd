/*
 * timeout.h
 * header-file for timeout.c
 *
 * Copyright (c) 2004-2012 Jan Seiffert
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
# ifndef WIN32
#  include "lib/my_pthread.h"
#  include "lib/rbtree.h"
# else
#  ifndef _WIN32_WINNT
#   define _WIN32_WINNT 0x0501
#  endif
#  include <winbase.h>
# endif

struct timeout
{
# ifndef WIN32
	struct rb_node rb;
	struct timespec t;
	mutex_t lock;
# else
	HANDLE t_handle;
# endif
	void *data;
	int (*fun)(void *);
# ifndef WIN32
	int rearm_in_progress;
# endif
};

# ifndef _TIMEOUT_C
#  define TOUT_EXTRN(x) extern x GCC_ATTR_VIS("hidden")
# else
#  define TOUT_EXTRN(x) x GCC_ATTR_VIS("hidden")
# endif

# ifndef WIN32
static inline void INIT_TIMEOUT(struct timeout *t)
{
	RB_CLEAR_NODE(&t->rb);
	mutex_init(&t->lock);
	t->rearm_in_progress = 0;
}

static inline void DESTROY_TIMEOUT(struct timeout *t GCC_ATTR_UNUSED_PARAM)
{
//	mutex_destroy(&t->lock);
}
# else
static inline void INIT_TIMEOUT(struct timeout *t)
{
	t->t_handle = INVALID_HANDLE_VALUE;
}
static inline void DESTROY_TIMEOUT(struct timeout *t)
{
	t->t_handle = INVALID_HANDLE_VALUE;
}
# endif

TOUT_EXTRN(bool timeout_add(struct timeout *, unsigned int));
TOUT_EXTRN(bool timeout_advance(struct timeout *, unsigned int));
TOUT_EXTRN(bool timeout_cancel(struct timeout *));
TOUT_EXTRN(void timeout_timer_task_abort(void));
TOUT_EXTRN(void *timeout_timer_task(void *));

#endif /* _TIMEOUT_H */
/* EOF */
