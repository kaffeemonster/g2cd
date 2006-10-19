/*
 * atomic.h
 * atomic primitves for i386
 *
 * Thanks Linux Kernel
 * 
 * Copyright (c) 2006, Jan Seiffert
 * 
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2
 * of the License, or any later version.
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

#ifndef LIB_IMPL_ATOMIC_H
# define LIB_IMPL_ATOMIC_H

# define atomic_read(x)	((x)->d)
# define atomic_set(x, y) (((x)->d) = (y))
# define atomic_pread(x)	((x)->d)
# define atomic_pset(x, y) ((x)->d = (y))
# define atomic_sread(x)	((x)->next)
# define atomic_sset(x, y) ((x)->next = (y))

# ifdef HAVE_SMP
#  define LOCK "lock; "
# else
#  define LOCK
# endif

static inline void *atomic_px(void *val, atomicptr_t *ptr)
{
	__asm__ __volatile__(
		"xchgl\t%0, %1"
		: "=r" (val)
		: "m"	(atomic_pread(ptr)),
		  "0" (val)
		: "memory");
	return val;
}

static inline int atomic_x(int val, atomic_t *ptr)
{
	__asm__ __volatile__(
		"xchgl\t%0, %1"
		: "=r" (val)
		: "m"	(atomic_read(ptr)),
		  "0" (val)
		: "memory");
	return val;
}

static inline void *atomic_cmppx(volatile void *nval, volatile void *oval, atomicptr_t *ptr)
{
	void *prev;
	__asm__ __volatile__(
			LOCK "cmpxchgl %1,%2"
			: "=a"(prev)
			: "r"(nval),
			  "m"(atomic_pread(ptr)),
			  "0"(oval)
			: "cc",
			  "memory");
	return prev;
}

static inline void atomic_inc(atomic_t *ptr)
{
	__asm__ __volatile__(
		LOCK "incl %0"
		/* gcc < 3 needs this, "+m" will not work reliable */
		: "=m" (atomic_read(ptr))
		: "m" (atomic_read(ptr)));
}

static inline void atomic_dec(atomic_t *ptr)
{
	__asm__ __volatile__(
		LOCK "decl %0"
		/* gcc < 3 needs this, "+m" will not work reliable */
		: "=m" (atomic_read(ptr))
		: "m" (atomic_read(ptr)));
}

#endif /* LIB_IMPL_ATOMIC_H */
