/*
 * atomic.h
 * atomic primitves for sparc
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

/* could make problems on _real_ old Fujitsu build sparcs */
# define atomic_px atomic_px
static inline void *atomic_px(void *val, atomicptr_t *ptr)
{
	__asm__ __volatile__(
		"swap\t\t%1, %0"
		: "=&r" (val)
		: "m" (atomic_pread(ptr)),
		  "0" (val)
		: "memory");
	return val;
}

# define atomic_p atomic_p
static inline int atomic_p(int val, atomic_t *ptr)
{
	__asm__ __volatile__(
		"swap\t\t%1, %0"
		: "=&r" (val)
		: "m" (atomic_read(ptr)),
		  "0" (val)
		: "memory");
	return val;
}

# undef LIB_IMPL_ATOMIC_H
# include "../generic/atomic.h"

#endif /* LIB_IMPL_ATOMIC_H */
