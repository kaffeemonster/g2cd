/*
 * atomic.h
 * atomic primitves
 *
 * Thanks Linux Kernel
 * 
 * Copyright (c) 2006, Jan Seiffert
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

#ifndef LIB_ATOMIC_H
# define LIB_ATOMIC_H

# ifdef HAVE_STDINT_H
#  include <stdint.h>
# else
#  include <inttypes.h>
# endif

/* __alpha__ 16 */
/* __powerpc__ implimentation defined ... sigh ... doc say some 2**4 */
# define ARCH_NEEDED_APAD 1

# define ATOMIC_INIT(x) {(x)}

/* warm beer and cheap tricks... */
# define deatomic(x) ((void *)(intptr_t)(x))

typedef struct xxxxxx1
{
	volatile int d;
} atomic_t;

typedef struct xxxxxx2
{
	volatile void *d;
} atomicptr_t;

/* keep layout in sync with above */
typedef struct xxxxxx3
{
	volatile struct xxxxxx3 *next;
} atomicst_t;

typedef union xxxxxx4
{
	volatile void *d;
	 /* some archs build atomic ops by the mean of an "atomic"
	  * load and store (notably ppc and alpha). When a load
	  * is executet, the CPU magicaly starts bookkeeping of
	  * acceses to the src mem-area, and the store ends this
	  * watch, failing if an change happend inbetween.
	  * This mem-area mostly has a size greater then the
	  * atomic-type size.
	  * For "normal" apps this would be fine. Think of a
	  * linked list, there is always the listmagic (*next etc.)
	  * you want to handle atomical (minus the fact you
	  * shouldn't stick *prev and *next directly together...)
	  * and some data which gives padding.
	  * But we have a Problem, in an array of pointers several
	  * are in such an area. An access to an neighboring element
	  * would trash the cpu-watch.
	  * So extra for array use, this type is defined, wich padds
	  * array-elems far enough from each other.
	  * (be warned, this makes the array larger! Keep the number
	  * of entrys small)
	  * (this is a hook atm, real magic has to follow)
	  */
	char pad[ARCH_NEEDED_APAD];
} atomicptra_t;

/* atomicptra_t and atomicptr_t should be abi compatible */
#define atomic_pxa(val, ptr) (atomic_px((val), (atomicptr_t *)((void *)(ptr))))
#define atomic_pxs(val, ptr) (atomic_px((val), (atomicptr_t *)((void *)(ptr))))
/* same for atomicptr_t and atomicst_t */
#define atomic_cmpalx(nval, oval, ptr) (atomic_cmppx((nval), (oval), (atomicptr_t *)(ptr)))

# if (!defined NEED_GENERIC) && (defined I_LIKE_ASM)
#  ifdef __i386__
#   include "i386/atomic.h"
#  elif defined(__x86_64__)
#   include "x86_64/atomic.h"
#  elif defined(__IA64__)
#   include "ia64/atomic.h"
#  elif defined(__sparcv8) || defined(__sparc_v8__) || defined(__sparcv9) || defined(__sparc_v9__)
/* 
 * gcc sometimes sets __sparcv8 even if you say "gimme v9" to not confuse
 * solaris tools. This will Bomb on a real v8 (maybe not a v8+)...
 */
#   include "sparc64/atomic.h"
#  elif defined(__sparc__) || defined(__sparc)
#   include "sparc/atomic.h"
#  elif defined(__powerpc__)
#   include "ppc/atomic.h"
#  else
#   include "generic/atomic.h"
#  endif
# else
#  include "generic/atomic.h"
# endif

static inline void atomic_push(atomicst_t *head, atomicst_t *node)
{
	void *tmp;
	do
		tmp = deatomic(atomic_sset(node, atomic_sread(head)));
	while(atomic_cmpalx(node, tmp, head) != tmp);
}

static inline atomicst_t *atomic_pop(atomicst_t *head)
{
	atomicst_t *ret_val, *tmp;
	do
	{
		if(!(tmp = deatomic(atomic_sread(head))))
			return NULL;
	} while((ret_val = atomic_cmpalx(atomic_sread(tmp), tmp, head)) != tmp);
	/* ABA race */
	return ret_val;
}

#endif /* LIB_ATOMIC_H */
