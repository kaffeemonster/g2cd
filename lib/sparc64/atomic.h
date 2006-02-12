/*
 * atomic.h
 * atomic primitves for sparc64
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

/* membars, anyone? */
# define atomic_read(x)	((x)->d)
# define atomic_set(x, y) (((x)->d) = (y))
# define atomic_pread(x)	((x)->d)
# define atomic_pset(x, y) ((x)->d = (y))
# define atomic_sread(x)	((x)->next)
# define atomic_sset(x, y) ((x)->next = (y))

static inline void *atomic_px_32(void *val, atomicptr_t *ptr)
{
	__asm__ __volatile__(
		"swap\t\t%1, %0"
		: "=&r" (val),
		  "=m" (atomic_pread(ptr))
		: "m" (atomic_pread(ptr)),
		  "0" (val));
	return val;
}

static inline void *atomic_px_64(void *val, atomicptr_t *ptr)
{
	__asm__ __volatile__(
		"swapx\t\t%2, %0"
		: "=&r" (val),
		  "=m" (atomic_pread(ptr))
		: "m" (atomic_pread(ptr)),
		  "0" (val));
	return val;
}

extern void *_illigal_ptr_size(void *,atomicptr_t *);
static inline void *atomic_px(void *val, atomicptr_t *ptr)
{
	switch(sizeof(*val))
	{
	case 4:
		return atomic_px_32(val, ptr);
	case 8:
		return atomic_px_64(val, ptr);
	}
	return _illigal_ptr_size(val, ptr);
}

static inline int atomic_x_64(int val, atomic_t *ptr)
{
	__asm__ __volatile__(
		"swap\t\t%2, %0"
		: "=&r" (val),
		  "=m" (atomic_read(ptr)),
		: "m" (atomic_read(ptr)),
		  "0" (val));
	return val;
}

static inline int atomic_x_32(int val, atomic_t *ptr)
{
	__asm__ __volatile__(
		"swapx\t\t%2, %0"
		: "=&r" (val),
		  "=m" (atomic_read(ptr))
		: "m" (atomic_read(ptr)),
		  "0" (val));
	return val;
}

extern int _illigal_int_size(int, atomic_t *)
static inline int atomic_x(int val, atomic_t *ptr)
{
	switch(sizeof(val))
	{
	case 4:
		return atomic_x_32(val, ptr);
	case 8:
		return atomic_x_64(val, ptr);
	}
	return _illigal_int_size(val, ptr)
}

# ifdef HAVE_SMP
#  define MEMBAR_1	"membar #StoreLoad | #LoadLoad\n\t"
#  define MEMBAR_2	"membar #StoreLoad | #StoreStore"
# else
#  define MEMBAR_1
#  define MEMBAR_2
# endif


static inline int atomic_cmpx_32(int nval, int oval, atomic_t *ptr)
{
	__asm__ __volatile__(
		MEMBAR_1
		"cas %3, %2, %0\n\t"
		MEMBAR_2
		: "=&r" (nval),
		  "=m" (atomic_read(ptr))
		: "r" (oval),
		  "m" (atomic_read(ptr)),
		  "0" (nval));
	return nval;
}

static inline int atomic_cmpx_64(int nval, int oval, atomic_t *ptr)
{
	__asm__ __volatile__(
		MEMBAR_1
		"casx %3, %2, %0\n\t"
		MEMBAR_2
		: "=&r" (nval),
		  "=m" (atomic_read(ptr))
		: "r" (oval),
		  "m" (atomic_read(ptr)),
		  "0" (nval));
	return nval;
}

static inline int atomic_cmpx(int nval, int oval, atomic_t *ptr)
{
	switch(sizeof(nval))
	{
	case 4:
		return atomic_cmpx_32(nval, oval, ptr);
	case 8:
		return atomic_cmpx_64(nval, oval, ptr);
	}
	return _illigal_int_size(nval, ptr);
}

static inline void *atomic_cmppx_32(void nval, void oval, atomicptr_t *ptr)
{
	__asm__ __volatile__(
		MEMBAR_1
		"cas %3, %2, %0\n\t"
		MEMBAR_2
		: "=&r" (nval),
		  "=m" (atomic_pread(ptr))
		: "r" (oval),
		  "m" (atomic_pread(ptr)),
		  "0" (nval));
	return nval;
}

static inline void *atomic_cmppx_64(void *nval, void *oval, atomicptr_t *ptr)
{
	__asm__ __volatile__(
		MEMBAR_1
		"casx %3, %2, %0\n\t"
		MEMBAR_2
		: "=&r" (nval),
		  "=m" (atomic_pread(ptr))
		: "r" (oval),
		  "m" (atomic_pread(ptr)),
		  "0" (nval));
	return nval;
}

static inline void *atomic_cmppx(void *nval, void *oval, atomicptr_t *ptr)
{
	switch(sizeof(nval))
	{
	case 4:
		return atomic_cmppx_32(nval, oval, ptr);
	case 8:
		return atomic_cmppx_64(nval, oval, ptr);
	}
	return _illigal_ptr_size(nval, ptr);
}

static inline void atomic_add_return(int val, atomic_t *ptr)
{
	int res;
	do
		res = atomic_read(ptr);
	while(atomic_cmpx(res + val, res, ptr) != res);
	return res;
}

static inline void atomic_sub_return(int val, atomic_t *ptr)
{
	int res;
	do
		res = atomic_read(ptr);
	while(atomic_cmpx(res - val, res, ptr) != res);
	return res;
}

# define atomic_inc(x) ((void) atomic_add_return(1, (x)))
# define atomic_dec(x) ((void) atomic_sub_return(1, (x)))

#endif /* LIB_IMPL_ATOMIC_H */
