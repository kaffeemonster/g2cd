/*
 * atomic.c
 * generic implementation of atomic ops
 *
 * Thanks to the Linux kernel
 *
 * Copyright (c) 2006-2009 Jan Seiffert
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
 * $Id:$
 *
 */

#include <stdlib.h>
#include <pthread.h>

#define NEED_GENERIC
#include "../atomic.h"
#include "../log_facility.h"

#define ATOMIC_HASH_SIZE 4
#define L1_CACHE_BYTES 64
#define ATOMIC_HASH(y) \
	(&(gen_atomic_lock[ (((intptr_t) (y))/L1_CACHE_BYTES) & (ATOMIC_HASH_SIZE-1) ]))
static shortlock_t gen_atomic_lock[ATOMIC_HASH_SIZE];

/* you better not remove this prototype... */
static void gen_atomic_init(void) GCC_ATTR_CONSTRUCT;
static void gen_atomic_init(void)
{
	int i = ATOMIC_HASH_SIZE - 1;
	do
	{
		if(shortlock_t_init(gen_atomic_lock + i))
			diedie("initialising generic atomic locks");
	}
	while(i--);
}

// TODO: catch errors?
void gen_atomic_inc(atomic_t *x)
{
	shortlock_t_lock(ATOMIC_HASH(x));
	x->d++;
	shortlock_t_unlock(ATOMIC_HASH(x));
}

int gen_atomic_inc_return(atomic_t *x)
{
	int ret;
	shortlock_t_lock(ATOMIC_HASH(x));
	ret = x->d++;
	shortlock_t_unlock(ATOMIC_HASH(x));
	return ret;
}

void gen_atomic_dec(atomic_t *x)
{
	shortlock_t_lock(ATOMIC_HASH(x));
	x->d--;
	shortlock_t_unlock(ATOMIC_HASH(x));
}

int gen_atomic_dec_test(atomic_t *x)
{
	int retval;
	shortlock_t_lock(ATOMIC_HASH(x));
	retval = --(x->d) == 0;
	shortlock_t_unlock(ATOMIC_HASH(x));
	return retval;
}

int gen_atomic_x(int nval, atomic_t *oval)
{
	int tmp;
	shortlock_t_lock(ATOMIC_HASH(oval));
	tmp = oval->d;
	oval->d = nval;
	shortlock_t_unlock(ATOMIC_HASH(oval));
	return tmp;
}

void *gen_atomic_px(void *nval, atomicptr_t *oval)
{
	intptr_t tmp;
	shortlock_t_lock(ATOMIC_HASH(oval));
	tmp = (intptr_t) oval->d;
	oval->d = nval;
	shortlock_t_unlock(ATOMIC_HASH(oval));
	return (void *) tmp;
}

void *gen_atomic_cmppx(volatile void *nval, volatile void *oval, atomicptr_t *x)
{
	intptr_t ret_val = (intptr_t) oval;
	shortlock_t_lock(ATOMIC_HASH(x));
	if(oval == x->d)
		x->d = nval;
	else
		ret_val = (intptr_t) nval;
	shortlock_t_unlock(ATOMIC_HASH(x));
	return (void *) ret_val;
}

static char const rcsid_ag[] GCC_ATTR_USED_VAR = "$Id:$";
