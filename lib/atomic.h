/*
 * atomic.h
 * atomic primitves
 *
 * Thanks Linux Kernel
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
	void * volatile d;
} atomicptr_t;

/* keep layout in sync with above */
typedef struct xxxxxx3
{
	struct xxxxxx3 * volatile next;
} atomicst_t;

typedef union xxxxxx4
{
	void * volatile d;
	 /* some archs build atomic ops by the mean of an "atomic"
	  * load and store (notably ppc and alpha). When a load
	  * is executed, the CPU magicaly starts bookkeeping of
	  * accesses to the src mem-area, and the store ends this
	  * watch, failing if a change happend in between.
	  * This mem-area mostly has a size greater than the
	  * atomic-type size.
	  * For "normal" apps this would be fine. Think of a
	  * linked list, there is always the listmagic (*next etc.)
	  * you want to handle atomical (minus the fact you
	  * shouldn't stick *prev and *next directly together...)
	  * and some data which gives padding.
	  * But we have a problem, in an array of pointers several
	  * are in such an area. An access to an neighbouring element
	  * would trash the cpu-watch.
	  * So extra for array use, this type is defined, which pads
	  * array-elements far enough from each other.
	  * (be warned, this makes the array larger! Keep the number
	  * of entrys small)
	  * (this is a hook atm, real magic has to follow)
	  */
	char pad[ARCH_NEEDED_APAD];
} atomicptra_t;

/* atomicptra_t and atomicptr_t should be abi compatible */
#define atomic_pxa(val, ptr) (atomic_px((val), (atomicptr_t *)(ptr)))
#define atomic_pxs(val, ptr) (atomic_px((val), (atomicptr_t *)(ptr)))
/* same for atomicptr_t and atomicst_t */
#define atomic_cmpalx(nval, oval, ptr) (atomic_cmppx((nval), (oval), ((atomicptr_t *)(ptr))))

# if !(defined NEED_GENERIC) && (defined I_LIKE_ASM)
#  if defined(__i386__) || defined(__x86_64__)
	/* work for both */
#   include "x86/atomic.h"
#  elif defined(__IA64__)
#   include "ia64/atomic.h"
#  elif defined(__sparc__) || defined(__sparc)
	/* for both archs */
#   include "sparc/atomic.h"
#  elif defined(__mips)
#   include "mips/atomic.h"
#  elif defined(__powerpc__) || defined(__powerpc64__)
#   include "ppc/atomic.h"
#  elif defined(__alpha__)
#   include "alpha/atomic.h"
#  elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || \
        defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || \
        defined(__ARM_ARCH_7A__)
#   include "arm/atomic.h"
#  else
#   if _GNUC_PREREQ(4, 1)
#    define atomic_cmppx(nval, oval, ptr) (void *)__sync_val_compare_and_swap(&(ptr)->d, (oval), (nval))
#    define atomic_inc(ptr) ((void)__sync_fetch_and_add(&(ptr)->d, 1))
#    define atomic_inc_return(ptr) __sync_fetch_and_add(&(ptr)->d, 1)
#    define atomic_dec(ptr) ((void)__sync_fetch_and_sub(&(ptr)->d, 1))
#    define atomic_dec_test(ptr) (!__sync_sub_and_fetch(&(ptr)->d, 1))
#   endif
#   include "generic/atomic.h"
#  endif
# else
#  if _GNUC_PREREQ(4, 1)
#   define atomic_cmppx(nval, oval, ptr) ((void *)__sync_val_compare_and_swap(&(ptr)->d, (oval), (nval)))
#   define atomic_inc(ptr) ((void)__sync_fetch_and_add(&(ptr)->d, 1))
#   define atomic_inc_return(ptr) __sync_fetch_and_add(&(ptr)->d, 1)
#   define atomic_dec(ptr) ((void)__sync_fetch_and_sub(&(ptr)->d, 1))
#   define atomic_dec_test(ptr) (!__sync_sub_and_fetch(&(ptr)->d, 1))
#  endif
#  include "generic/atomic.h"
# endif

# ifndef ATOMIC_PUSH_ARCH
static always_inline void atomic_push(atomicst_t *head, atomicst_t *node)
{
	void *tmp1, *tmp2;
	tmp2 = atomic_sread(head);
	do {
		node->next = tmp2;
		tmp1 = tmp2;
		tmp2 = atomic_cmpalx(node, tmp2, head);
	} while(unlikely(tmp1 != tmp2));
}
# endif

static always_inline atomicst_t *atomic_pop(atomicst_t *head)
{
	atomicst_t *tmp1, *tmp2;
	if(!(tmp2 = atomic_sread(head)))
		return NULL;
	do
	{
		tmp1 = tmp2;
		tmp2 = atomic_cmpalx(atomic_sread(tmp2), tmp2, head);
		if(!tmp2)
			break;
	} while(unlikely(tmp1 != tmp2));
	/*
	 * We may now have poped the "wrong" element
	 * If you wanted simply the next, everthing is OK
	 * If you wanted THIS element, oops
	 */
	return tmp2;
}

#endif /* LIB_ATOMIC_H */
