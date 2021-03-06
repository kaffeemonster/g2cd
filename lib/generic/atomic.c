/*
 * atomic.c
 * generic implementation of atomic ops
 *
 * Copyright (c) 2006-2012 Jan Seiffert
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
 * Thanks go to the Linux kernel for the ideas
 *
 * $Id:$
 *
 */

#include <stdlib.h>
#include "../my_pthread.h"

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
static __init void gen_atomic_init(void)
{
	int i = ATOMIC_HASH_SIZE - 1;
	do
	{
		if(shortlock_init(gen_atomic_lock + i))
			diedie("initialising generic atomic locks");
	}
	while(i--);
}

// TODO: catch errors?
void gen_atomic_inc(atomic_t *x)
{
	shortlock_lock(ATOMIC_HASH(x));
	x->d++;
	shortlock_unlock(ATOMIC_HASH(x));
}

int gen_atomic_inc_return(atomic_t *x)
{
	int ret;
	shortlock_lock(ATOMIC_HASH(x));
	ret = x->d++;
	shortlock_unlock(ATOMIC_HASH(x));
	return ret;
}

void gen_atomic_dec(atomic_t *x)
{
	shortlock_lock(ATOMIC_HASH(x));
	x->d--;
	shortlock_unlock(ATOMIC_HASH(x));
}

int gen_atomic_dec_test(atomic_t *x)
{
	int retval;
	shortlock_lock(ATOMIC_HASH(x));
	retval = --(x->d) == 0;
	shortlock_unlock(ATOMIC_HASH(x));
	return retval;
}

int gen_atomic_x(int nval, atomic_t *oval)
{
	int tmp;
	shortlock_lock(ATOMIC_HASH(oval));
	tmp = oval->d;
	oval->d = nval;
	shortlock_unlock(ATOMIC_HASH(oval));
	return tmp;
}

void *gen_atomic_px(void *nval, atomicptr_t *oval)
{
	void *tmp;
	shortlock_lock(ATOMIC_HASH(oval));
	tmp = oval->d;
	oval->d = nval;
	shortlock_unlock(ATOMIC_HASH(oval));
	return tmp;
}

int gen_atomic_cmpx(int nval, int oval, atomic_t *x)
{
	int ret_val = oval;
	shortlock_lock(ATOMIC_HASH(x));
	ret_val = x->d;
	if(oval == ret_val)
		x->d = nval;
	shortlock_unlock(ATOMIC_HASH(x));
	return ret_val;
}

void *gen_atomic_cmppx(void *nval, void *oval, atomicptr_t *x)
{
	void *ret_val = oval;
	shortlock_lock(ATOMIC_HASH(x));
	ret_val = x->d;
	if(oval == ret_val)
		x->d = nval;
	shortlock_unlock(ATOMIC_HASH(x));
	return ret_val;
}

static char const rcsid_ag[] GCC_ATTR_USED_VAR = "$Id:$";
